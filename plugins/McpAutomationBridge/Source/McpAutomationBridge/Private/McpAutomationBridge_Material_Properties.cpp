// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_Material_Properties.cpp
//
// Task F.5 - top-level material property setters for the manage_material tool.
//
// External handlers exposed by this TU:
//   - set_blend_modes
//   - set_shading_models
//   - set_material_domains
//   - set_material_attributes_modes
//   - set_two_sided_flags
//
// All five actions take {items: [{assetPath, value}, ...], save?}. Each item
// runs in its own FScopedTransaction; per-item failures don't abort the rest.
// Engine content (/Engine/, /EnginePlugins/) is rejected per-item with
// errorCode = "ENGINE_ASSET_BLOCKED". Unrecognized enum strings return
// "INVALID_ENUM_VALUE". Wire response: {success, results: [{assetPath, success,
// error?, errorCode?}, ...]}.
//
// Plan: docs/superpowers/plans/2026-05-07-mcp-material-tools-redesign.md
// Spec: docs/superpowers/specs/2026-05-07-mcp-material-tools-redesign-design.md (sec 5)

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR

#include "Materials/Material.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"

// EMaterialDomain lives in MaterialDomain.h on UE 5.1+; UE 5.0 ships it inside
// Material.h directly.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "MaterialDomain.h"
#endif

#endif // WITH_EDITOR

DEFINE_LOG_CATEGORY_STATIC(LogMcpMaterialTopLevelProps, Log, All);

namespace
{
#if WITH_EDITOR

// True for any path under engine content; per-item write is refused for these.
static bool McpPropsIsEngineAsset(const FString& InPath)
{
    return InPath.StartsWith(TEXT("/Engine/")) || InPath.StartsWith(TEXT("/EnginePlugins/"));
}

// Result row scaffold. Every per-item row carries {assetPath, success}; failure
// rows additionally carry {error, errorCode}.
static TSharedPtr<FJsonObject> McpMakePropsRow(
    const FString& AssetPath, bool bSuccess)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("assetPath"), AssetPath);
    Row->SetBoolField(TEXT("success"), bSuccess);
    return Row;
}

// Stamp an error row with {error, errorCode}.
static void McpStampPropsError(
    TSharedPtr<FJsonObject>& Row, const FString& Message, const FString& Code)
{
    if (!Row.IsValid()) return;
    Row->SetBoolField(TEXT("success"), false);
    Row->SetStringField(TEXT("error"), Message);
    Row->SetStringField(TEXT("errorCode"), Code);
}

// Loads UMaterial for a wire path with engine-block + non-Material guards.
// Returns nullptr and stamps OutRow on failure.
static UMaterial* McpLoadMaterialForPropsItem(
    const TSharedPtr<FJsonObject>& Item, TSharedPtr<FJsonObject>& OutRow,
    FString& OutAssetPath)
{
    OutAssetPath.Reset();
    if (!Item.IsValid())
    {
        OutRow = McpMakePropsRow(TEXT(""), false);
        McpStampPropsError(OutRow, TEXT("Item is not an object"), TEXT("INVALID_ARGUMENT"));
        return nullptr;
    }

    Item->TryGetStringField(TEXT("assetPath"), OutAssetPath);
    OutRow = McpMakePropsRow(OutAssetPath, false);

    if (OutAssetPath.IsEmpty())
    {
        McpStampPropsError(OutRow, TEXT("assetPath is required"), TEXT("INVALID_ARGUMENT"));
        return nullptr;
    }

    if (McpPropsIsEngineAsset(OutAssetPath))
    {
        McpStampPropsError(OutRow,
            FString::Printf(TEXT("Asset path '%s' is under engine content. Copy to /Game first."), *OutAssetPath),
            TEXT("ENGINE_ASSET_BLOCKED"));
        return nullptr;
    }

    const FString Sanitized = SanitizeProjectRelativePath(OutAssetPath);
    if (Sanitized.IsEmpty())
    {
        McpStampPropsError(OutRow,
            FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *OutAssetPath),
            TEXT("INVALID_PATH"));
        return nullptr;
    }
    OutAssetPath = Sanitized;
    OutRow->SetStringField(TEXT("assetPath"), OutAssetPath);

    // Distinguish "asset is wrong type" from "asset missing" with a generic load.
    UObject* Probe = LoadObject<UObject>(nullptr, *OutAssetPath);
    if (!Probe)
    {
        McpStampPropsError(OutRow,
            FString::Printf(TEXT("Could not load asset '%s'."), *OutAssetPath),
            TEXT("ASSET_NOT_FOUND"));
        return nullptr;
    }

    UMaterial* Material = Cast<UMaterial>(Probe);
    if (!Material)
    {
        McpStampPropsError(OutRow,
            FString::Printf(TEXT("Asset '%s' is %s, expected UMaterial."),
                *OutAssetPath, *Probe->GetClass()->GetName()),
            TEXT("WRONG_ASSET_TYPE"));
        return nullptr;
    }

    return Material;
}

// Enum mappers. Return false on miss; caller stamps INVALID_ENUM_VALUE.
static bool McpParseBlendMode(const FString& In, EBlendMode& Out)
{
    if      (In == TEXT("Opaque"))         { Out = BLEND_Opaque;         return true; }
    else if (In == TEXT("Masked"))         { Out = BLEND_Masked;         return true; }
    else if (In == TEXT("Translucent"))    { Out = BLEND_Translucent;    return true; }
    else if (In == TEXT("Additive"))       { Out = BLEND_Additive;       return true; }
    else if (In == TEXT("Modulate"))       { Out = BLEND_Modulate;       return true; }
    else if (In == TEXT("AlphaComposite")) { Out = BLEND_AlphaComposite; return true; }
    else if (In == TEXT("AlphaHoldout"))   { Out = BLEND_AlphaHoldout;   return true; }
    return false;
}

static bool McpParseShadingModel(const FString& In, EMaterialShadingModel& Out)
{
    if      (In == TEXT("Unlit"))             { Out = MSM_Unlit;             return true; }
    else if (In == TEXT("DefaultLit"))        { Out = MSM_DefaultLit;        return true; }
    else if (In == TEXT("Subsurface"))        { Out = MSM_Subsurface;        return true; }
    else if (In == TEXT("PreintegratedSkin")) { Out = MSM_PreintegratedSkin; return true; }
    else if (In == TEXT("ClearCoat"))         { Out = MSM_ClearCoat;         return true; }
    else if (In == TEXT("SubsurfaceProfile")) { Out = MSM_SubsurfaceProfile; return true; }
    else if (In == TEXT("TwoSidedFoliage"))   { Out = MSM_TwoSidedFoliage;   return true; }
    else if (In == TEXT("Hair"))              { Out = MSM_Hair;              return true; }
    else if (In == TEXT("Cloth"))             { Out = MSM_Cloth;             return true; }
    else if (In == TEXT("Eye"))               { Out = MSM_Eye;               return true; }
    else if (In == TEXT("SingleLayerWater"))  { Out = MSM_SingleLayerWater;  return true; }
    else if (In == TEXT("ThinTranslucent"))   { Out = MSM_ThinTranslucent;   return true; }
    return false;
}

static bool McpParseMaterialDomain(const FString& In, EMaterialDomain& Out)
{
    if      (In == TEXT("Surface"))               { Out = MD_Surface;               return true; }
    else if (In == TEXT("DeferredDecal"))         { Out = MD_DeferredDecal;         return true; }
    else if (In == TEXT("LightFunction"))         { Out = MD_LightFunction;         return true; }
    else if (In == TEXT("Volume"))                { Out = MD_Volume;                return true; }
    else if (In == TEXT("PostProcess"))           { Out = MD_PostProcess;           return true; }
    else if (In == TEXT("UI"))                    { Out = MD_UI;                    return true; }
    else if (In == TEXT("RuntimeVirtualTexture")) { Out = MD_RuntimeVirtualTexture; return true; }
    return false;
}

// Per-item value reader: pull "value" from Item. Returns false (and stamps
// OutRow with INVALID_ARGUMENT) when the field is missing entirely.
static bool McpReadValueField(
    const TSharedPtr<FJsonObject>& Item, TSharedPtr<FJsonObject>& OutRow,
    TSharedPtr<FJsonValue>& OutValue)
{
    OutValue.Reset();
    if (Item.IsValid())
    {
        OutValue = Item->TryGetField(TEXT("value"));
    }
    if (!OutValue.IsValid() || OutValue->Type == EJson::Null)
    {
        McpStampPropsError(OutRow, TEXT("'value' is required"), TEXT("INVALID_ARGUMENT"));
        return false;
    }
    return true;
}

// What the per-item worker does for one of the five property setters.
// The body mutates the Material under a transaction and stamps the row.
using FMcpPropsApplyFn = TFunction<bool(UMaterial* Material,
    const TSharedPtr<FJsonValue>& Value, const TSharedPtr<FJsonObject>& Item,
    TSharedPtr<FJsonObject>& OutRow)>;

// Per-item driver. Loads the material, opens a transaction, calls Apply, then
// runs PostEditChangeProperty + MarkPackageDirty + (optional) save.
static void McpRunPropsItem(
    const TSharedPtr<FJsonObject>& Item, TSharedPtr<FJsonObject>& OutRow,
    bool bDefaultSave, const TCHAR* TransactionLabel,
    const FMcpPropsApplyFn& ApplyFn)
{
    FString AssetPath;
    UMaterial* Material = McpLoadMaterialForPropsItem(Item, OutRow, AssetPath);
    if (!Material) return; // OutRow already stamped

    TSharedPtr<FJsonValue> ValueField;
    if (!McpReadValueField(Item, OutRow, ValueField)) return;

    FScopedTransaction Tx(NSLOCTEXT("McpAutomationBridge", "McpSetMaterialTopLevelProp", "MCP set material property"));
    Material->Modify();

    if (!ApplyFn(Material, ValueField, Item, OutRow))
    {
        // ApplyFn stamped the error row.
        return;
    }

    // Use a synthetic property-change event so the editor reacts (recompiles
    // if needed). No specific FProperty needed here; the empty event matches
    // the legacy single-item handlers' behaviour.
    FPropertyChangedEvent EmptyEvent(nullptr);
    Material->PostEditChangeProperty(EmptyEvent);
    Material->MarkPackageDirty();

    bool bSave = bDefaultSave;
    if (Item.IsValid())
    {
        Item->TryGetBoolField(TEXT("save"), bSave);
    }
    if (bSave)
    {
        McpSafeAssetSave(Material);
    }

    OutRow->SetBoolField(TEXT("success"), true);
    (void)TransactionLabel; // currently unused; kept for future telemetry hooks
}

// Read top-level "save" override. If absent, defaults to true (matching the
// legacy single-item set_blend_mode behaviour).
static bool McpReadTopLevelSave(const TSharedPtr<FJsonObject>& Payload)
{
    bool bSave = true;
    if (Payload.IsValid())
    {
        Payload->TryGetBoolField(TEXT("save"), bSave);
    }
    return bSave;
}

// Shared driver. Reads payload->items[] and applies the supplied per-item
// worker, building the final {success, results: [...]} response.
static bool McpHandle_PropsBatch(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
    const TCHAR* ActionLabel, const TCHAR* TransactionLabel,
    const FMcpPropsApplyFn& ApplyFn)
{
    if (!Sub) return true;
    if (!Payload.IsValid())
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("Invalid payload"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* ItemsArr = nullptr;
    if (!Payload->TryGetArrayField(TEXT("items"), ItemsArr) || !ItemsArr || ItemsArr->Num() == 0)
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("'items' is required and must be a non-empty array of item objects."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const bool bDefaultSave = McpReadTopLevelSave(Payload);

    TArray<TSharedPtr<FJsonValue>> Results;
    Results.Reserve(ItemsArr->Num());
    int32 SuccessCount = 0;

    for (const TSharedPtr<FJsonValue>& V : *ItemsArr)
    {
        const TSharedPtr<FJsonObject>* ItemPtr = nullptr;
        TSharedPtr<FJsonObject> Item;
        if (V.IsValid() && V->TryGetObject(ItemPtr) && ItemPtr) Item = *ItemPtr;
        if (!Item.IsValid()) Item = MakeShared<FJsonObject>();

        TSharedPtr<FJsonObject> Row;
        McpRunPropsItem(Item, Row, bDefaultSave, TransactionLabel, ApplyFn);
        if (!Row.IsValid())
        {
            Row = McpMakePropsRow(TEXT(""), false);
            McpStampPropsError(Row,
                TEXT("Internal error: worker did not produce a result row."),
                TEXT("INTERNAL_ERROR"));
        }
        bool bRowOk = false;
        Row->TryGetBoolField(TEXT("success"), bRowOk);
        if (bRowOk) ++SuccessCount;
        Results.Add(MakeShared<FJsonValueObject>(Row));
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetArrayField(TEXT("results"), Results);
    Sub->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("%s: %d/%d succeeded."),
            ActionLabel, SuccessCount, ItemsArr->Num()),
        Resp);
    return true;
}

// ---------------------------------------------------------------------------
// Per-action Apply lambdas.
// ---------------------------------------------------------------------------

static bool McpApply_BlendMode(UMaterial* Material,
    const TSharedPtr<FJsonValue>& Value, const TSharedPtr<FJsonObject>& /*Item*/,
    TSharedPtr<FJsonObject>& OutRow)
{
    if (Value->Type != EJson::String)
    {
        McpStampPropsError(OutRow,
            TEXT("'value' must be a string for set_blend_modes."),
            TEXT("INVALID_ARGUMENT"));
        return false;
    }
    EBlendMode Parsed;
    const FString S = Value->AsString();
    if (!McpParseBlendMode(S, Parsed))
    {
        McpStampPropsError(OutRow,
            FString::Printf(TEXT("Invalid blendMode '%s'. Valid values: Opaque, Masked, Translucent, Additive, Modulate, AlphaComposite, AlphaHoldout"), *S),
            TEXT("INVALID_ENUM_VALUE"));
        return false;
    }
    Material->BlendMode = Parsed;
    return true;
}

static bool McpApply_ShadingModel(UMaterial* Material,
    const TSharedPtr<FJsonValue>& Value, const TSharedPtr<FJsonObject>& /*Item*/,
    TSharedPtr<FJsonObject>& OutRow)
{
    if (Value->Type != EJson::String)
    {
        McpStampPropsError(OutRow,
            TEXT("'value' must be a string for set_shading_models."),
            TEXT("INVALID_ARGUMENT"));
        return false;
    }
    EMaterialShadingModel Parsed;
    const FString S = Value->AsString();
    if (!McpParseShadingModel(S, Parsed))
    {
        McpStampPropsError(OutRow,
            FString::Printf(TEXT("Invalid shadingModel '%s'. Valid values: Unlit, DefaultLit, Subsurface, PreintegratedSkin, ClearCoat, SubsurfaceProfile, TwoSidedFoliage, Hair, Cloth, Eye, SingleLayerWater, ThinTranslucent"), *S),
            TEXT("INVALID_ENUM_VALUE"));
        return false;
    }
    // Material->SetShadingModel handles both bUsesShadingModelFromMaterialExpression
    // path and the simple case; identical to the legacy single-item handler.
    Material->SetShadingModel(Parsed);
    return true;
}

static bool McpApply_MaterialDomain(UMaterial* Material,
    const TSharedPtr<FJsonValue>& Value, const TSharedPtr<FJsonObject>& /*Item*/,
    TSharedPtr<FJsonObject>& OutRow)
{
    if (Value->Type != EJson::String)
    {
        McpStampPropsError(OutRow,
            TEXT("'value' must be a string for set_material_domains."),
            TEXT("INVALID_ARGUMENT"));
        return false;
    }
    EMaterialDomain Parsed;
    const FString S = Value->AsString();
    if (!McpParseMaterialDomain(S, Parsed))
    {
        McpStampPropsError(OutRow,
            FString::Printf(TEXT("Invalid materialDomain '%s'. Valid values: Surface, DeferredDecal, LightFunction, Volume, PostProcess, UI, RuntimeVirtualTexture"), *S),
            TEXT("INVALID_ENUM_VALUE"));
        return false;
    }
    Material->MaterialDomain = Parsed;
    return true;
}

static bool McpApply_MaterialAttributesMode(UMaterial* Material,
    const TSharedPtr<FJsonValue>& Value, const TSharedPtr<FJsonObject>& /*Item*/,
    TSharedPtr<FJsonObject>& OutRow)
{
    if (Value->Type != EJson::Boolean)
    {
        McpStampPropsError(OutRow,
            TEXT("'value' must be a boolean for set_material_attributes_modes."),
            TEXT("INVALID_ARGUMENT"));
        return false;
    }
    Material->bUseMaterialAttributes = Value->AsBool();
    return true;
}

static bool McpApply_TwoSided(UMaterial* Material,
    const TSharedPtr<FJsonValue>& Value, const TSharedPtr<FJsonObject>& /*Item*/,
    TSharedPtr<FJsonObject>& OutRow)
{
    if (Value->Type != EJson::Boolean)
    {
        McpStampPropsError(OutRow,
            TEXT("'value' must be a boolean for set_two_sided_flags."),
            TEXT("INVALID_ARGUMENT"));
        return false;
    }
    Material->TwoSided = Value->AsBool() ? 1 : 0;
    return true;
}

#endif // WITH_EDITOR
} // namespace

// ---------------------------------------------------------------------------
// External handlers (resolved via 'extern bool ...' from the dispatch site).
// ---------------------------------------------------------------------------

bool McpHandle_SetBlendModes(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_PropsBatch(Sub, RequestId, Payload, Socket,
        TEXT("set_blend_modes"), TEXT("McpSetBlendMode"), &McpApply_BlendMode);
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_SetShadingModels(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_PropsBatch(Sub, RequestId, Payload, Socket,
        TEXT("set_shading_models"), TEXT("McpSetShadingModel"), &McpApply_ShadingModel);
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_SetMaterialDomains(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_PropsBatch(Sub, RequestId, Payload, Socket,
        TEXT("set_material_domains"), TEXT("McpSetMaterialDomain"), &McpApply_MaterialDomain);
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_SetMaterialAttributesModes(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_PropsBatch(Sub, RequestId, Payload, Socket,
        TEXT("set_material_attributes_modes"), TEXT("McpSetMaterialAttributesMode"),
        &McpApply_MaterialAttributesMode);
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_SetTwoSidedFlags(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_PropsBatch(Sub, RequestId, Payload, Socket,
        TEXT("set_two_sided_flags"), TEXT("McpSetTwoSided"), &McpApply_TwoSided);
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}
