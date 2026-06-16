// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_Material_Landscape.cpp
//
// Task F.6 - landscape-material handlers for the manage_material tool.
//
// External handlers exposed by this TU:
//   - add_landscape_layers              (batch; appends entries to the target
//                                        material's MaterialExpressionLandscapeLayerBlend.
//                                        Creates the blend expression on the
//                                        first call when the material has none.)
//   - configure_landscape_layer_blends  (batch; updates BlendType / PreviewWeight
//                                        on existing entries in the blend node.)
//   - get_landscape_material_context    (singular; introspects one material's
//                                        landscape blend node, listing its
//                                        layers.)
//
// Each per-item entry takes {assetPath, ...}. Engine content is rejected with
// "ENGINE_ASSET_BLOCKED". Per-item failures don't abort the batch; each item
// gets a row in the response 'results' array with {success, error?, errorCode?}.
//
// Note: this file's surface is materially smaller than the legacy
// `add_landscape_layer` (which created ULandscapeLayerInfoObject *assets*) and
// the legacy `get_landscape_material_context` (which keyed off ALandscape
// *actors*). The redesign spec scopes both around the assigned material's
// MaterialExpressionLandscapeLayerBlend node and uses {assetPath} as the key.
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
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"

#endif // WITH_EDITOR

DEFINE_LOG_CATEGORY_STATIC(LogMcpMaterialLandscape, Log, All);

namespace
{
#if WITH_EDITOR

// True for any path under engine content; per-item write is refused for these.
static bool McpLsIsEngineAsset(const FString& InPath)
{
    return InPath.StartsWith(TEXT("/Engine/")) || InPath.StartsWith(TEXT("/EnginePlugins/"));
}

// Result row scaffold. Every per-item row carries {assetPath, success}; failure
// rows additionally carry {error, errorCode}.
static TSharedPtr<FJsonObject> McpMakeLsRow(
    const FString& AssetPath, bool bSuccess)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("assetPath"), AssetPath);
    Row->SetBoolField(TEXT("success"), bSuccess);
    return Row;
}

static void McpStampLsError(
    TSharedPtr<FJsonObject>& Row, const FString& Message, const FString& Code)
{
    if (!Row.IsValid()) return;
    Row->SetBoolField(TEXT("success"), false);
    Row->SetStringField(TEXT("error"), Message);
    Row->SetStringField(TEXT("errorCode"), Code);
}

// Loads UMaterial for a wire path with engine-block + non-Material guards.
// Returns nullptr and stamps OutRow on failure.
static UMaterial* McpLoadMaterialForLsItem(
    const TSharedPtr<FJsonObject>& Item, TSharedPtr<FJsonObject>& OutRow,
    FString& OutAssetPath)
{
    OutAssetPath.Reset();
    if (!Item.IsValid())
    {
        OutRow = McpMakeLsRow(TEXT(""), false);
        McpStampLsError(OutRow, TEXT("Item is not an object"), TEXT("INVALID_ARGUMENT"));
        return nullptr;
    }

    Item->TryGetStringField(TEXT("assetPath"), OutAssetPath);
    OutRow = McpMakeLsRow(OutAssetPath, false);

    if (OutAssetPath.IsEmpty())
    {
        McpStampLsError(OutRow, TEXT("assetPath is required"), TEXT("INVALID_ARGUMENT"));
        return nullptr;
    }

    if (McpLsIsEngineAsset(OutAssetPath))
    {
        McpStampLsError(OutRow,
            FString::Printf(TEXT("Asset path '%s' is under engine content. Copy to /Game first."), *OutAssetPath),
            TEXT("ENGINE_ASSET_BLOCKED"));
        return nullptr;
    }

    const FString Sanitized = SanitizeProjectRelativePath(OutAssetPath);
    if (Sanitized.IsEmpty())
    {
        McpStampLsError(OutRow,
            FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *OutAssetPath),
            TEXT("INVALID_PATH"));
        return nullptr;
    }
    OutAssetPath = Sanitized;
    OutRow->SetStringField(TEXT("assetPath"), OutAssetPath);

    UObject* Probe = LoadObject<UObject>(nullptr, *OutAssetPath);
    if (!Probe)
    {
        McpStampLsError(OutRow,
            FString::Printf(TEXT("Could not load asset '%s'."), *OutAssetPath),
            TEXT("ASSET_NOT_FOUND"));
        return nullptr;
    }

    UMaterial* Material = Cast<UMaterial>(Probe);
    if (!Material)
    {
        McpStampLsError(OutRow,
            FString::Printf(TEXT("Asset '%s' is %s, expected UMaterial."),
                *OutAssetPath, *Probe->GetClass()->GetName()),
            TEXT("WRONG_ASSET_TYPE"));
        return nullptr;
    }

    return Material;
}

// Wire-string <-> ELandscapeLayerBlendType.
static bool McpParseLayerBlendType(const FString& In, ELandscapeLayerBlendType& Out)
{
    if      (In == TEXT("LB_WeightBlend") || In == TEXT("WeightBlend") || In == TEXT("Weight")) { Out = LB_WeightBlend; return true; }
    else if (In == TEXT("LB_AlphaBlend")  || In == TEXT("AlphaBlend")  || In == TEXT("Alpha"))  { Out = LB_AlphaBlend;  return true; }
    else if (In == TEXT("LB_HeightBlend") || In == TEXT("HeightBlend") || In == TEXT("Height")) { Out = LB_HeightBlend; return true; }
    return false;
}

static FString McpFormatLayerBlendType(ELandscapeLayerBlendType In)
{
    switch (In)
    {
    case LB_WeightBlend: return TEXT("LB_WeightBlend");
    case LB_AlphaBlend:  return TEXT("LB_AlphaBlend");
    case LB_HeightBlend: return TEXT("LB_HeightBlend");
    default:             return TEXT("Unknown");
    }
}

// Find the existing UMaterialExpressionLandscapeLayerBlend on a material, or
// nullptr if there isn't one yet. We pick the first occurrence; legacy graphs
// with multiple landscape blend nodes are rare and the spec keeps the surface
// minimal (one blend per material).
static UMaterialExpressionLandscapeLayerBlend* McpFindLandscapeBlend(UMaterial* Material)
{
    if (!Material) return nullptr;
    FMcpMaterialGraphOwner Owner;
    Owner.Asset       = Material;
    Owner.GraphSource = Material;
    Owner.Kind        = EMcpMaterialGraphOwnerKind::Material;
    const TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressions(Owner);
    if (!Exprs) return nullptr;
    for (UMaterialExpression* Expr : *Exprs)
    {
        if (UMaterialExpressionLandscapeLayerBlend* Blend = Cast<UMaterialExpressionLandscapeLayerBlend>(Expr))
        {
            return Blend;
        }
    }
    return nullptr;
}

// Find or create the blend expression; returns nullptr only on allocation
// failure (extremely unlikely). When created, the new node is appended to the
// material's expression collection.
static UMaterialExpressionLandscapeLayerBlend* McpFindOrCreateLandscapeBlend(UMaterial* Material)
{
    if (UMaterialExpressionLandscapeLayerBlend* Existing = McpFindLandscapeBlend(Material))
    {
        return Existing;
    }
    if (!Material) return nullptr;

    UMaterialExpressionLandscapeLayerBlend* NewBlend =
        NewObject<UMaterialExpressionLandscapeLayerBlend>(
            Material, UMaterialExpressionLandscapeLayerBlend::StaticClass(),
            NAME_None, RF_Transactional);
    if (!NewBlend) return nullptr;

#if WITH_EDITORONLY_DATA
    if (UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData())
    {
        EditorOnly->ExpressionCollection.Expressions.Add(NewBlend);
    }
#endif
    return NewBlend;
}

// JSON entry describing one layer (returned by get_landscape_material_context
// and embedded in add/configure success rows for the affected layer).
static TSharedPtr<FJsonObject> McpBuildLayerEntry(const FLayerBlendInput& Layer)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("name"), Layer.LayerName.ToString());
    Obj->SetStringField(TEXT("blendType"), McpFormatLayerBlendType(Layer.BlendType));
    Obj->SetNumberField(TEXT("previewWeight"), Layer.PreviewWeight);
    return Obj;
}

// Read top-level "save" override; defaults true.
static bool McpReadLsTopLevelSave(const TSharedPtr<FJsonObject>& Payload)
{
    bool bSave = true;
    if (Payload.IsValid())
    {
        Payload->TryGetBoolField(TEXT("save"), bSave);
    }
    return bSave;
}

// ---------------------------------------------------------------------------
// add_landscape_layers
// ---------------------------------------------------------------------------

static void McpRunAddLandscapeLayerItem(
    const TSharedPtr<FJsonObject>& Item, TSharedPtr<FJsonObject>& OutRow,
    bool bDefaultSave)
{
    FString AssetPath;
    UMaterial* Material = McpLoadMaterialForLsItem(Item, OutRow, AssetPath);
    if (!Material) return;

    FString LayerName;
    Item->TryGetStringField(TEXT("layerName"), LayerName);
    if (LayerName.IsEmpty())
    {
        McpStampLsError(OutRow, TEXT("'layerName' is required"), TEXT("INVALID_ARGUMENT"));
        return;
    }

    // Optional fields with sensible defaults.
    ELandscapeLayerBlendType BlendType = LB_WeightBlend;
    FString BlendTypeStr;
    if (Item->TryGetStringField(TEXT("layerBlendType"), BlendTypeStr) ||
        Item->TryGetStringField(TEXT("blendType"), BlendTypeStr))
    {
        if (!McpParseLayerBlendType(BlendTypeStr, BlendType))
        {
            McpStampLsError(OutRow,
                FString::Printf(TEXT("Invalid layerBlendType '%s'. Valid values: LB_WeightBlend, LB_AlphaBlend, LB_HeightBlend"), *BlendTypeStr),
                TEXT("INVALID_ENUM_VALUE"));
            return;
        }
    }

    double PreviewWeight = 0.0;
    Item->TryGetNumberField(TEXT("layerWeightStrength"), PreviewWeight);
    Item->TryGetNumberField(TEXT("previewWeight"), PreviewWeight); // accept either name

    // layerInfoPath is accepted for compatibility but the on-graph blend
    // expression has no slot for ULandscapeLayerInfoObject; we just echo it
    // back in the response so callers know it was received.
    FString LayerInfoPath;
    Item->TryGetStringField(TEXT("layerInfoPath"), LayerInfoPath);

    FScopedTransaction Tx(NSLOCTEXT("McpAutomationBridge", "McpAddLandscapeLayer", "MCP add landscape layer"));
    Material->Modify();

    UMaterialExpressionLandscapeLayerBlend* Blend = McpFindOrCreateLandscapeBlend(Material);
    if (!Blend)
    {
        McpStampLsError(OutRow,
            TEXT("Failed to allocate LandscapeLayerBlend expression."),
            TEXT("INTERNAL_ERROR"));
        return;
    }
    Blend->Modify();

    // Reject duplicate layer names; configure_landscape_layer_blends is the
    // proper path for editing an existing entry.
    const FName LayerFName(*LayerName);
    for (const FLayerBlendInput& Existing : Blend->Layers)
    {
        if (Existing.LayerName == LayerFName)
        {
            McpStampLsError(OutRow,
                FString::Printf(TEXT("Layer '%s' already exists on this material; use configure_landscape_layer_blends."), *LayerName),
                TEXT("LAYER_ALREADY_EXISTS"));
            return;
        }
    }

    FLayerBlendInput NewLayer;
    NewLayer.LayerName = LayerFName;
    NewLayer.BlendType = BlendType;
    NewLayer.PreviewWeight = static_cast<float>(PreviewWeight);
    Blend->Layers.Add(NewLayer);

    FPropertyChangedEvent EmptyEvent(nullptr);
    Blend->PostEditChangeProperty(EmptyEvent);
    Material->PostEditChangeProperty(EmptyEvent);
    Material->MarkPackageDirty();

    bool bSave = bDefaultSave;
    Item->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
        McpSafeAssetSave(Material);
    }

    OutRow->SetBoolField(TEXT("success"), true);
    OutRow->SetStringField(TEXT("layerName"), LayerName);
    OutRow->SetStringField(TEXT("blendType"), McpFormatLayerBlendType(BlendType));
    OutRow->SetNumberField(TEXT("previewWeight"), NewLayer.PreviewWeight);
    OutRow->SetNumberField(TEXT("layerCount"), Blend->Layers.Num());
    if (!LayerInfoPath.IsEmpty())
    {
        OutRow->SetStringField(TEXT("layerInfoPath"), LayerInfoPath);
    }
}

// ---------------------------------------------------------------------------
// configure_landscape_layer_blends
// ---------------------------------------------------------------------------

static void McpRunConfigureLandscapeLayerBlendItem(
    const TSharedPtr<FJsonObject>& Item, TSharedPtr<FJsonObject>& OutRow,
    bool bDefaultSave)
{
    FString AssetPath;
    UMaterial* Material = McpLoadMaterialForLsItem(Item, OutRow, AssetPath);
    if (!Material) return;

    FString LayerName;
    Item->TryGetStringField(TEXT("layerName"), LayerName);
    if (LayerName.IsEmpty())
    {
        McpStampLsError(OutRow, TEXT("'layerName' is required"), TEXT("INVALID_ARGUMENT"));
        return;
    }

    UMaterialExpressionLandscapeLayerBlend* Blend = McpFindLandscapeBlend(Material);
    if (!Blend)
    {
        McpStampLsError(OutRow,
            TEXT("Material has no LandscapeLayerBlend expression; call add_landscape_layers first."),
            TEXT("BLEND_NOT_FOUND"));
        return;
    }

    const FName LayerFName(*LayerName);
    int32 Index = INDEX_NONE;
    for (int32 I = 0; I < Blend->Layers.Num(); ++I)
    {
        if (Blend->Layers[I].LayerName == LayerFName) { Index = I; break; }
    }
    if (Index == INDEX_NONE)
    {
        McpStampLsError(OutRow,
            FString::Printf(TEXT("Layer '%s' not found on this material's LandscapeLayerBlend."), *LayerName),
            TEXT("LAYER_NOT_FOUND"));
        return;
    }

    // Both fields optional; only stamp INVALID_ARGUMENT if neither given.
    bool bAnyFieldGiven = false;
    ELandscapeLayerBlendType NewBlendType = Blend->Layers[Index].BlendType;
    {
        FString BlendTypeStr;
        if (Item->TryGetStringField(TEXT("blendType"), BlendTypeStr) ||
            Item->TryGetStringField(TEXT("layerBlendType"), BlendTypeStr))
        {
            if (!McpParseLayerBlendType(BlendTypeStr, NewBlendType))
            {
                McpStampLsError(OutRow,
                    FString::Printf(TEXT("Invalid blendType '%s'. Valid values: LB_WeightBlend, LB_AlphaBlend, LB_HeightBlend"), *BlendTypeStr),
                    TEXT("INVALID_ENUM_VALUE"));
                return;
            }
            bAnyFieldGiven = true;
        }
    }

    double NewPreviewWeight = Blend->Layers[Index].PreviewWeight;
    {
        if (Item->TryGetNumberField(TEXT("weightStrength"), NewPreviewWeight) ||
            Item->TryGetNumberField(TEXT("previewWeight"), NewPreviewWeight) ||
            Item->TryGetNumberField(TEXT("layerWeightStrength"), NewPreviewWeight))
        {
            bAnyFieldGiven = true;
        }
    }

    if (!bAnyFieldGiven)
    {
        McpStampLsError(OutRow,
            TEXT("At least one of blendType / weightStrength is required."),
            TEXT("INVALID_ARGUMENT"));
        return;
    }

    FScopedTransaction Tx(NSLOCTEXT("McpAutomationBridge", "McpConfigureLandscapeLayerBlend",
        "MCP configure landscape layer blend"));
    Material->Modify();
    Blend->Modify();

    Blend->Layers[Index].BlendType = NewBlendType;
    Blend->Layers[Index].PreviewWeight = static_cast<float>(NewPreviewWeight);

    FPropertyChangedEvent EmptyEvent(nullptr);
    Blend->PostEditChangeProperty(EmptyEvent);
    Material->PostEditChangeProperty(EmptyEvent);
    Material->MarkPackageDirty();

    bool bSave = bDefaultSave;
    Item->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
        McpSafeAssetSave(Material);
    }

    OutRow->SetBoolField(TEXT("success"), true);
    OutRow->SetStringField(TEXT("layerName"), LayerName);
    OutRow->SetStringField(TEXT("blendType"), McpFormatLayerBlendType(NewBlendType));
    OutRow->SetNumberField(TEXT("previewWeight"), Blend->Layers[Index].PreviewWeight);
}

// ---------------------------------------------------------------------------
// Shared per-item driver for the two batch actions.
// ---------------------------------------------------------------------------

using FMcpLsItemFn = TFunction<void(const TSharedPtr<FJsonObject>&,
    TSharedPtr<FJsonObject>&, bool /*bDefaultSave*/)>;

static bool McpHandle_LandscapeBatch(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
    const TCHAR* ActionLabel, const FMcpLsItemFn& ItemFn)
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

    const bool bDefaultSave = McpReadLsTopLevelSave(Payload);

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
        ItemFn(Item, Row, bDefaultSave);
        if (!Row.IsValid())
        {
            Row = McpMakeLsRow(TEXT(""), false);
            McpStampLsError(Row,
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

#endif // WITH_EDITOR
} // namespace

// ---------------------------------------------------------------------------
// External handlers (resolved via 'extern bool ...' from the dispatch site).
// ---------------------------------------------------------------------------

bool McpHandle_AddLandscapeLayers(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_LandscapeBatch(Sub, RequestId, Payload, Socket,
        TEXT("add_landscape_layers"), &McpRunAddLandscapeLayerItem);
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_ConfigureLandscapeLayerBlends(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_LandscapeBatch(Sub, RequestId, Payload, Socket,
        TEXT("configure_landscape_layer_blends"), &McpRunConfigureLandscapeLayerBlendItem);
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

// ---------------------------------------------------------------------------
// get_landscape_material_context (singular; one material per call).
// ---------------------------------------------------------------------------

bool McpHandle_GetLandscapeMaterialContext(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    if (!Sub) return true;
    if (!Payload.IsValid())
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("Invalid payload"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    if (AssetPath.IsEmpty())
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("'assetPath' is required."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (McpLsIsEngineAsset(AssetPath))
    {
        Sub->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Asset path '%s' is under engine content."), *AssetPath),
            TEXT("ENGINE_ASSET_BLOCKED"));
        return true;
    }
    {
        const FString Sanitized = SanitizeProjectRelativePath(AssetPath);
        if (Sanitized.IsEmpty())
        {
            Sub->SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                TEXT("INVALID_PATH"));
            return true;
        }
        AssetPath = Sanitized;
    }

    UObject* Probe = LoadObject<UObject>(nullptr, *AssetPath);
    if (!Probe)
    {
        Sub->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Could not load asset '%s'."), *AssetPath),
            TEXT("ASSET_NOT_FOUND"));
        return true;
    }
    UMaterial* Material = Cast<UMaterial>(Probe);
    if (!Material)
    {
        Sub->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Asset '%s' is %s, expected UMaterial."),
                *AssetPath, *Probe->GetClass()->GetName()),
            TEXT("WRONG_ASSET_TYPE"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);

    UMaterialExpressionLandscapeLayerBlend* Blend = McpFindLandscapeBlend(Material);
    Result->SetBoolField(TEXT("hasLandscapeLayerBlend"), Blend != nullptr);

    TArray<TSharedPtr<FJsonValue>> LayerArr;
    if (Blend)
    {
        Result->SetStringField(TEXT("expressionName"), Blend->GetName());
        Result->SetStringField(TEXT("expressionGuid"), Blend->MaterialExpressionGuid.ToString());
        for (const FLayerBlendInput& Layer : Blend->Layers)
        {
            LayerArr.Add(MakeShared<FJsonValueObject>(McpBuildLayerEntry(Layer)));
        }
    }
    Result->SetArrayField(TEXT("layers"), LayerArr);
    Result->SetNumberField(TEXT("layerCount"), LayerArr.Num());

    Sub->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Landscape material context for '%s' (%d layer%s)."),
            *AssetPath, LayerArr.Num(), LayerArr.Num() == 1 ? TEXT("") : TEXT("s")),
        Result);
    return true;
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}
