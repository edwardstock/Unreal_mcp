// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_Material_InstanceParameters.cpp
//
// Task F.1 - typed setters + unified getters/resetters/clearers for material
// instance parameters.
//
// External handlers exposed by this TU:
//   - set_material_instance_scalar_parameters
//   - set_material_instance_vector_parameters
//   - set_material_instance_texture_parameters
//   - set_material_instance_static_switch_parameters
//   - get_material_instance_parameters
//   - reset_material_instance_parameters
//   - clear_material_instance_parameters
//
// All batch endpoints follow partial-success-per-item: an error on one item
// never aborts the rest; each item gets a {success, error?, errorCode?}
// row in the response 'results' array.
//
// Plan: docs/superpowers/plans/2026-05-07-mcp-material-tools-redesign.md
// Spec: docs/superpowers/specs/2026-05-07-mcp-material-tools-redesign-design.md (sec 9)

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR

#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "MaterialEditingLibrary.h"
#include "Engine/Texture.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"

#endif // WITH_EDITOR

DEFINE_LOG_CATEGORY_STATIC(LogMcpInstanceParams, Log, All);

namespace
{
#if WITH_EDITOR

// Single-item response row helper. Keys are stable wire-format identifiers
// the caller can correlate with their input items.
static TSharedPtr<FJsonObject> McpMakeItemResult(
    const FString& AssetPath, const FString& ParameterName,
    bool bSuccess, const FString& Error, const FString& ErrorCode)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("assetPath"), AssetPath);
    if (!ParameterName.IsEmpty())
    {
        Row->SetStringField(TEXT("parameterName"), ParameterName);
    }
    Row->SetBoolField(TEXT("success"), bSuccess);
    if (!bSuccess)
    {
        Row->SetStringField(TEXT("error"), Error);
        if (!ErrorCode.IsEmpty())
        {
            Row->SetStringField(TEXT("errorCode"), ErrorCode);
        }
    }
    return Row;
}

static bool McpIsEngineAsset(const FString& AssetPath)
{
    return AssetPath.StartsWith(TEXT("/Engine/")) || AssetPath.StartsWith(TEXT("/EnginePlugins/"));
}

// Loads MIC for a wire path. Sets OutError on failure; returns nullptr.
static UMaterialInstanceConstant* McpLoadMaterialInstance(
    UMcpAutomationBridgeSubsystem* Sub, FString& AssetPath,
    FString& OutError, FString& OutErrorCode)
{
    if (AssetPath.IsEmpty())
    {
        OutError = TEXT("assetPath is required");
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        return nullptr;
    }
    if (McpIsEngineAsset(AssetPath))
    {
        OutError = FString::Printf(
            TEXT("Asset path '%s' is under engine content. Copy to /Game first."),
            *AssetPath);
        OutErrorCode = TEXT("ENGINE_ASSET_BLOCKED");
        return nullptr;
    }
    AssetPath = SanitizeProjectRelativePath(AssetPath);
    UMaterialInstanceConstant* MIC = LoadObject<UMaterialInstanceConstant>(nullptr, *AssetPath);
    if (!MIC)
    {
        OutError = FString::Printf(TEXT("Could not load UMaterialInstanceConstant '%s'."), *AssetPath);
        OutErrorCode = TEXT("ASSET_NOT_FOUND");
        return nullptr;
    }
    return MIC;
}

// Returns true if the named parameter has an explicit override on the MIC
// (not just inherited from the parent).
static bool McpMicHasOverride(
    UMaterialInstanceConstant* MIC, FName ParamName, const FString& Type)
{
    if (!MIC) return false;
    if (Type == TEXT("scalar"))
    {
        for (const auto& V : MIC->ScalarParameterValues)
            if (V.ParameterInfo.Name == ParamName) return true;
        return false;
    }
    if (Type == TEXT("vector"))
    {
        for (const auto& V : MIC->VectorParameterValues)
            if (V.ParameterInfo.Name == ParamName) return true;
        return false;
    }
    if (Type == TEXT("texture"))
    {
        for (const auto& V : MIC->TextureParameterValues)
            if (V.ParameterInfo.Name == ParamName) return true;
        return false;
    }
    if (Type == TEXT("staticSwitch"))
    {
        for (const auto& V : MIC->GetStaticParameters().StaticSwitchParameters)
            if (V.ParameterInfo.Name == ParamName && V.bOverride) return true;
        return false;
    }
    return false;
}

// Reads {r,g,b,a} from a JSON value field; missing components default per
// LinearColor convention (rgb=0, a=1). Returns true if the source object was
// usable.
static bool McpReadLinearColor(const TSharedPtr<FJsonValue>& Val, FLinearColor& Out)
{
    Out = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
    if (!Val.IsValid()) return false;
    const TSharedPtr<FJsonObject>* Obj = nullptr;
    if (!Val->TryGetObject(Obj) || !Obj || !Obj->IsValid()) return false;
    double R = 0, G = 0, B = 0, A = 1;
    (*Obj)->TryGetNumberField(TEXT("r"), R);
    (*Obj)->TryGetNumberField(TEXT("g"), G);
    (*Obj)->TryGetNumberField(TEXT("b"), B);
    (*Obj)->TryGetNumberField(TEXT("a"), A);
    Out = FLinearColor((float)R, (float)G, (float)B, (float)A);
    return true;
}

// Per-item dispatch helper for the four typed setters. Returns the result
// row JSON for the item, never null. Path is treated as in/out so the
// caller can read back the sanitized version.
static TSharedPtr<FJsonObject> McpApplyTypedSetter(
    UMcpAutomationBridgeSubsystem* Sub,
    const TSharedPtr<FJsonObject>& Item,
    const FString& ValueKind /* "scalar" | "vector" | "texture" | "staticSwitch" */)
{
    FString AssetPath, ParameterName;
    if (!Item.IsValid())
    {
        return McpMakeItemResult(TEXT(""), TEXT(""), false,
            TEXT("Item is not an object"), TEXT("INVALID_ARGUMENT"));
    }
    Item->TryGetStringField(TEXT("assetPath"), AssetPath);
    Item->TryGetStringField(TEXT("parameterName"), ParameterName);

    if (ParameterName.IsEmpty())
    {
        return McpMakeItemResult(AssetPath, ParameterName, false,
            TEXT("parameterName is required"), TEXT("INVALID_ARGUMENT"));
    }

    FString LoadErr, LoadCode;
    UMaterialInstanceConstant* MIC = McpLoadMaterialInstance(Sub, AssetPath, LoadErr, LoadCode);
    if (!MIC)
    {
        return McpMakeItemResult(AssetPath, ParameterName, false, LoadErr, LoadCode);
    }

    const FName PName(*ParameterName);
    bool bApplied = false;
    FString FailMsg, FailCode;

    FScopedTransaction Tx(NSLOCTEXT("McpAutomationBridge", "McpSetMicParam",
        "MCP set material instance parameter"));
    MIC->Modify();

    if (ValueKind == TEXT("scalar"))
    {
        const TSharedPtr<FJsonValue> ValueField = Item->TryGetField(TEXT("value"));
        if (!ValueField.IsValid() || ValueField->Type != EJson::Number)
        {
            FailMsg = TEXT("value must be a number for scalar parameter");
            FailCode = TEXT("INVALID_ARGUMENT");
        }
        else
        {
            const float V = (float)ValueField->AsNumber();
            bApplied = UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(
                MIC, PName, V);
            if (!bApplied)
            {
                FailMsg = FString::Printf(TEXT("Parameter '%s' not found on this instance's material."), *ParameterName);
                FailCode = TEXT("PARAMETER_NOT_FOUND");
            }
        }
    }
    else if (ValueKind == TEXT("vector"))
    {
        const TSharedPtr<FJsonValue> ValueField = Item->TryGetField(TEXT("value"));
        FLinearColor Color;
        if (!McpReadLinearColor(ValueField, Color))
        {
            FailMsg = TEXT("value must be an object {r,g,b,a} for vector parameter");
            FailCode = TEXT("INVALID_ARGUMENT");
        }
        else
        {
            bApplied = UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(
                MIC, PName, Color);
            if (!bApplied)
            {
                FailMsg = FString::Printf(TEXT("Parameter '%s' not found on this instance's material."), *ParameterName);
                FailCode = TEXT("PARAMETER_NOT_FOUND");
            }
        }
    }
    else if (ValueKind == TEXT("texture"))
    {
        FString TexPath;
        const TSharedPtr<FJsonValue> ValueField = Item->TryGetField(TEXT("value"));
        if (ValueField.IsValid() && ValueField->Type == EJson::String)
        {
            TexPath = ValueField->AsString();
        }
        if (TexPath.IsEmpty())
        {
            FailMsg = TEXT("value must be a non-empty texture asset path string");
            FailCode = TEXT("INVALID_ARGUMENT");
        }
        else
        {
            const FString Sanitized = SanitizeProjectRelativePath(TexPath);
            UTexture* Tex = LoadObject<UTexture>(nullptr, *Sanitized);
            if (!Tex)
            {
                FailMsg = FString::Printf(TEXT("Could not load texture '%s'."), *TexPath);
                FailCode = TEXT("ASSET_NOT_FOUND");
            }
            else
            {
                bApplied = UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(
                    MIC, PName, Tex);
                if (!bApplied)
                {
                    FailMsg = FString::Printf(TEXT("Parameter '%s' not found on this instance's material."), *ParameterName);
                    FailCode = TEXT("PARAMETER_NOT_FOUND");
                }
            }
        }
    }
    else if (ValueKind == TEXT("staticSwitch"))
    {
        const TSharedPtr<FJsonValue> ValueField = Item->TryGetField(TEXT("value"));
        if (!ValueField.IsValid() || ValueField->Type != EJson::Boolean)
        {
            FailMsg = TEXT("value must be a boolean for staticSwitch parameter");
            FailCode = TEXT("INVALID_ARGUMENT");
        }
        else
        {
            const bool V = ValueField->AsBool();
            bApplied = UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(
                MIC, PName, V);
            if (!bApplied)
            {
                FailMsg = FString::Printf(TEXT("Parameter '%s' not found on this instance's material."), *ParameterName);
                FailCode = TEXT("PARAMETER_NOT_FOUND");
            }
        }
    }
    else
    {
        FailMsg = FString::Printf(TEXT("Unknown valueKind '%s'"), *ValueKind);
        FailCode = TEXT("INTERNAL_ERROR");
    }

    if (bApplied)
    {
        MIC->MarkPackageDirty();
        return McpMakeItemResult(AssetPath, ParameterName, true, FString(), FString());
    }
    return McpMakeItemResult(AssetPath, ParameterName, false, FailMsg, FailCode);
}

// Builds a parameters[] entry for get_material_instance_parameters.
// 'type' is the wire-string type ("scalar" | "vector" | "texture" | "staticSwitch").
// 'parentValue' reports the immediate parent's value via the parameter
// inheritance chain.
static TSharedPtr<FJsonObject> McpBuildParamRow_Scalar(
    UMaterialInstanceConstant* MIC, const FName& ParamName)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("name"), ParamName.ToString());
    Row->SetStringField(TEXT("type"), TEXT("scalar"));
    const FMaterialParameterInfo Info(ParamName);

    float Eff = 0.0f;
    MIC->GetScalarParameterValue(Info, Eff);
    Row->SetNumberField(TEXT("value"), Eff);

    Row->SetBoolField(TEXT("isOverridden"),
        McpMicHasOverride(MIC, ParamName, TEXT("scalar")));

    float ParentVal = 0.0f;
    if (MIC->Parent)
    {
        MIC->Parent->GetScalarParameterValue(Info, ParentVal);
    }
    Row->SetNumberField(TEXT("parentValue"), ParentVal);
    return Row;
}

static TSharedPtr<FJsonObject> McpBuildParamRow_Vector(
    UMaterialInstanceConstant* MIC, const FName& ParamName)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("name"), ParamName.ToString());
    Row->SetStringField(TEXT("type"), TEXT("vector"));
    const FMaterialParameterInfo Info(ParamName);

    auto MakeColor = [](const FLinearColor& C) -> TSharedPtr<FJsonObject>
    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetNumberField(TEXT("r"), C.R);
        O->SetNumberField(TEXT("g"), C.G);
        O->SetNumberField(TEXT("b"), C.B);
        O->SetNumberField(TEXT("a"), C.A);
        return O;
    };

    FLinearColor Eff = FLinearColor::Black;
    MIC->GetVectorParameterValue(Info, Eff);
    Row->SetObjectField(TEXT("value"), MakeColor(Eff));

    Row->SetBoolField(TEXT("isOverridden"),
        McpMicHasOverride(MIC, ParamName, TEXT("vector")));

    FLinearColor ParentVal = FLinearColor::Black;
    if (MIC->Parent)
    {
        MIC->Parent->GetVectorParameterValue(Info, ParentVal);
    }
    Row->SetObjectField(TEXT("parentValue"), MakeColor(ParentVal));
    return Row;
}

static TSharedPtr<FJsonObject> McpBuildParamRow_Texture(
    UMaterialInstanceConstant* MIC, const FName& ParamName)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("name"), ParamName.ToString());
    Row->SetStringField(TEXT("type"), TEXT("texture"));
    const FMaterialParameterInfo Info(ParamName);

    UTexture* Eff = nullptr;
    MIC->GetTextureParameterValue(Info, Eff);
    Row->SetStringField(TEXT("value"), Eff ? Eff->GetPathName() : TEXT(""));

    Row->SetBoolField(TEXT("isOverridden"),
        McpMicHasOverride(MIC, ParamName, TEXT("texture")));

    UTexture* ParentTex = nullptr;
    if (MIC->Parent)
    {
        MIC->Parent->GetTextureParameterValue(Info, ParentTex);
    }
    Row->SetStringField(TEXT("parentValue"), ParentTex ? ParentTex->GetPathName() : TEXT(""));
    return Row;
}

static TSharedPtr<FJsonObject> McpBuildParamRow_StaticSwitch(
    UMaterialInstanceConstant* MIC, const FName& ParamName)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("name"), ParamName.ToString());
    Row->SetStringField(TEXT("type"), TEXT("staticSwitch"));
    const FMaterialParameterInfo Info(ParamName);

    bool bEff = false;
    FGuid Guid;
    MIC->GetStaticSwitchParameterValue(Info, bEff, Guid);
    Row->SetBoolField(TEXT("value"), bEff);

    Row->SetBoolField(TEXT("isOverridden"),
        McpMicHasOverride(MIC, ParamName, TEXT("staticSwitch")));

    bool bParent = false;
    if (MIC->Parent)
    {
        FGuid PGuid;
        MIC->Parent->GetStaticSwitchParameterValue(Info, bParent, PGuid);
    }
    Row->SetBoolField(TEXT("parentValue"), bParent);
    return Row;
}

// Build the entire parameters[] for one MIC. Order: scalar, vector, texture,
// staticSwitch. Names enumerated via UMaterialEditingLibrary - covers the
// full visible parameter namespace (parent + this instance's overrides).
static TArray<TSharedPtr<FJsonValue>> McpBuildAllParamRows(
    UMaterialInstanceConstant* MIC)
{
    TArray<TSharedPtr<FJsonValue>> Out;

    TArray<FName> Names;
    UMaterialEditingLibrary::GetScalarParameterNames(MIC, Names);
    for (const FName& N : Names)
    {
        Out.Add(MakeShared<FJsonValueObject>(McpBuildParamRow_Scalar(MIC, N)));
    }

    Names.Reset();
    UMaterialEditingLibrary::GetVectorParameterNames(MIC, Names);
    for (const FName& N : Names)
    {
        Out.Add(MakeShared<FJsonValueObject>(McpBuildParamRow_Vector(MIC, N)));
    }

    Names.Reset();
    UMaterialEditingLibrary::GetTextureParameterNames(MIC, Names);
    for (const FName& N : Names)
    {
        Out.Add(MakeShared<FJsonValueObject>(McpBuildParamRow_Texture(MIC, N)));
    }

    Names.Reset();
    UMaterialEditingLibrary::GetStaticSwitchParameterNames(MIC, Names);
    for (const FName& N : Names)
    {
        Out.Add(MakeShared<FJsonValueObject>(McpBuildParamRow_StaticSwitch(MIC, N)));
    }

    return Out;
}

// Entry-point body for the four typed setters.
static bool McpHandle_TypedSetterBatch(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    const FString& ValueKind)
{
    if (!Sub || !Payload.IsValid())
    {
        if (Sub) Sub->SendAutomationError(Socket, RequestId,
            TEXT("Invalid payload"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* ItemsArr = nullptr;
    if (!Payload->TryGetArrayField(TEXT("items"), ItemsArr) || !ItemsArr)
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("items[] is required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> Results;
    Results.Reserve(ItemsArr->Num());
    int32 SuccessCount = 0;

    for (const TSharedPtr<FJsonValue>& Val : *ItemsArr)
    {
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        TSharedPtr<FJsonObject> Item;
        if (Val.IsValid() && Val->TryGetObject(ObjPtr) && ObjPtr) Item = *ObjPtr;
        TSharedPtr<FJsonObject> Row = McpApplyTypedSetter(Sub, Item, ValueKind);
        bool bRowOk = false;
        Row->TryGetBoolField(TEXT("success"), bRowOk);
        if (bRowOk) ++SuccessCount;
        Results.Add(MakeShared<FJsonValueObject>(Row));
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetArrayField(TEXT("results"), Results);
    Sub->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Applied %d/%d items."), SuccessCount, ItemsArr->Num()),
        Resp);
    return true;
}

#endif // WITH_EDITOR
} // namespace

// =============================================================================
// External entries
// =============================================================================
extern bool McpHandle_SetMaterialInstanceScalarParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
extern bool McpHandle_SetMaterialInstanceVectorParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
extern bool McpHandle_SetMaterialInstanceTextureParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
extern bool McpHandle_SetMaterialInstanceStaticSwitchParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
extern bool McpHandle_GetMaterialInstanceParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
extern bool McpHandle_ResetMaterialInstanceParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
extern bool McpHandle_ClearMaterialInstanceParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);

bool McpHandle_SetMaterialInstanceScalarParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_TypedSetterBatch(Sub, RequestId, Payload, Socket, TEXT("scalar"));
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_SetMaterialInstanceVectorParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_TypedSetterBatch(Sub, RequestId, Payload, Socket, TEXT("vector"));
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_SetMaterialInstanceTextureParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_TypedSetterBatch(Sub, RequestId, Payload, Socket, TEXT("texture"));
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_SetMaterialInstanceStaticSwitchParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_TypedSetterBatch(Sub, RequestId, Payload, Socket, TEXT("staticSwitch"));
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_GetMaterialInstanceParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    if (!Sub || !Payload.IsValid())
    {
        if (Sub) Sub->SendAutomationError(Socket, RequestId,
            TEXT("Invalid payload"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Accept either {assetPaths: [string,...]} or items: [{assetPath}].
    TArray<FString> Paths;
    const TArray<TSharedPtr<FJsonValue>>* AssetPathsArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArr) && AssetPathsArr)
    {
        for (const auto& V : *AssetPathsArr)
        {
            if (V.IsValid() && V->Type == EJson::String)
            {
                Paths.Add(V->AsString());
            }
        }
    }
    const TArray<TSharedPtr<FJsonValue>>* ItemsArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("items"), ItemsArr) && ItemsArr)
    {
        for (const auto& V : *ItemsArr)
        {
            const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
            if (V.IsValid() && V->TryGetObject(ObjPtr) && ObjPtr)
            {
                FString P;
                (*ObjPtr)->TryGetStringField(TEXT("assetPath"), P);
                if (!P.IsEmpty()) Paths.Add(P);
            }
        }
    }

    if (Paths.Num() == 0)
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("Provide either 'assetPaths' or 'items' (with assetPath each)."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> Results;
    Results.Reserve(Paths.Num());
    int32 SuccessCount = 0;

    for (FString AssetPath : Paths)
    {
        FString LoadErr, LoadCode;
        UMaterialInstanceConstant* MIC = McpLoadMaterialInstance(Sub, AssetPath, LoadErr, LoadCode);
        if (!MIC)
        {
            Results.Add(MakeShared<FJsonValueObject>(
                McpMakeItemResult(AssetPath, FString(), false, LoadErr, LoadCode)));
            continue;
        }

        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("assetPath"), AssetPath);
        Row->SetBoolField(TEXT("success"), true);
        Row->SetStringField(TEXT("parentPath"),
            MIC->Parent ? MIC->Parent->GetPathName() : TEXT(""));
        Row->SetArrayField(TEXT("parameters"), McpBuildAllParamRows(MIC));
        Results.Add(MakeShared<FJsonValueObject>(Row));
        ++SuccessCount;
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetArrayField(TEXT("results"), Results);
    Sub->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Inspected %d/%d material instance(s)."),
            SuccessCount, Paths.Num()),
        Resp);
    return true;
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_ResetMaterialInstanceParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    if (!Sub || !Payload.IsValid())
    {
        if (Sub) Sub->SendAutomationError(Socket, RequestId,
            TEXT("Invalid payload"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* ItemsArr = nullptr;
    if (!Payload->TryGetArrayField(TEXT("items"), ItemsArr) || !ItemsArr)
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("items[] is required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> Results;
    Results.Reserve(ItemsArr->Num());
    int32 SuccessCount = 0;

    for (const TSharedPtr<FJsonValue>& Val : *ItemsArr)
    {
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        TSharedPtr<FJsonObject> Item;
        if (Val.IsValid() && Val->TryGetObject(ObjPtr) && ObjPtr) Item = *ObjPtr;

        FString AssetPath, ParameterName;
        if (Item.IsValid())
        {
            Item->TryGetStringField(TEXT("assetPath"), AssetPath);
            Item->TryGetStringField(TEXT("parameterName"), ParameterName);
        }
        if (!Item.IsValid() || ParameterName.IsEmpty())
        {
            Results.Add(MakeShared<FJsonValueObject>(McpMakeItemResult(
                AssetPath, ParameterName, false,
                TEXT("parameterName is required"), TEXT("INVALID_ARGUMENT"))));
            continue;
        }

        FString LoadErr, LoadCode;
        UMaterialInstanceConstant* MIC = McpLoadMaterialInstance(Sub, AssetPath, LoadErr, LoadCode);
        if (!MIC)
        {
            Results.Add(MakeShared<FJsonValueObject>(McpMakeItemResult(
                AssetPath, ParameterName, false, LoadErr, LoadCode)));
            continue;
        }

        FScopedTransaction Tx(NSLOCTEXT("McpAutomationBridge", "McpResetMicParam",
            "MCP reset material instance parameter"));
        MIC->Modify();

        const FName PN(*ParameterName);
        const int32 ScalarRemoved = MIC->ScalarParameterValues.RemoveAll(
            [&PN](const FScalarParameterValue& V){ return V.ParameterInfo.Name == PN; });
        const int32 VectorRemoved = MIC->VectorParameterValues.RemoveAll(
            [&PN](const FVectorParameterValue& V){ return V.ParameterInfo.Name == PN; });
        const int32 TextureRemoved = MIC->TextureParameterValues.RemoveAll(
            [&PN](const FTextureParameterValue& V){ return V.ParameterInfo.Name == PN; });

        FStaticParameterSet StaticParams;
        MIC->GetStaticParameterValues(StaticParams);
        const int32 StaticRemoved = StaticParams.StaticSwitchParameters.RemoveAll(
            [&PN](const FStaticSwitchParameter& V){ return V.ParameterInfo.Name == PN; });
        if (StaticRemoved > 0)
        {
            MIC->UpdateStaticPermutation(StaticParams);
        }

        const int32 TotalRemoved = ScalarRemoved + VectorRemoved + TextureRemoved + StaticRemoved;
        if (TotalRemoved == 0)
        {
            Results.Add(MakeShared<FJsonValueObject>(McpMakeItemResult(
                AssetPath, ParameterName, false,
                FString::Printf(TEXT("No override found for parameter '%s'."), *ParameterName),
                TEXT("OVERRIDE_NOT_FOUND"))));
            continue;
        }

        UMaterialEditingLibrary::UpdateMaterialInstance(MIC);
        MIC->MarkPackageDirty();

        TSharedPtr<FJsonObject> Row = McpMakeItemResult(AssetPath, ParameterName, true, FString(), FString());
        Row->SetNumberField(TEXT("removedCount"), TotalRemoved);
        Results.Add(MakeShared<FJsonValueObject>(Row));
        ++SuccessCount;
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetArrayField(TEXT("results"), Results);
    Sub->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Reset %d/%d overrides."), SuccessCount, ItemsArr->Num()),
        Resp);
    return true;
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_ClearMaterialInstanceParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    if (!Sub || !Payload.IsValid())
    {
        if (Sub) Sub->SendAutomationError(Socket, RequestId,
            TEXT("Invalid payload"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* ItemsArr = nullptr;
    if (!Payload->TryGetArrayField(TEXT("items"), ItemsArr) || !ItemsArr)
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("items[] is required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> Results;
    Results.Reserve(ItemsArr->Num());
    int32 SuccessCount = 0;

    for (const TSharedPtr<FJsonValue>& Val : *ItemsArr)
    {
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        TSharedPtr<FJsonObject> Item;
        if (Val.IsValid() && Val->TryGetObject(ObjPtr) && ObjPtr) Item = *ObjPtr;

        FString AssetPath;
        if (Item.IsValid()) Item->TryGetStringField(TEXT("assetPath"), AssetPath);

        FString LoadErr, LoadCode;
        UMaterialInstanceConstant* MIC = McpLoadMaterialInstance(Sub, AssetPath, LoadErr, LoadCode);
        if (!MIC)
        {
            Results.Add(MakeShared<FJsonValueObject>(McpMakeItemResult(
                AssetPath, FString(), false, LoadErr, LoadCode)));
            continue;
        }

        FScopedTransaction Tx(NSLOCTEXT("McpAutomationBridge", "McpClearMicParams",
            "MCP clear material instance parameters"));
        MIC->Modify();

        const int32 ClearedCount =
            MIC->ScalarParameterValues.Num() +
            MIC->VectorParameterValues.Num() +
            MIC->TextureParameterValues.Num() +
            MIC->GetStaticParameters().StaticSwitchParameters.Num();

        // Use the engine helper to wipe ALL overrides plus statics. It also
        // calls UpdateStaticPermutation/UpdateMaterialInstance internally.
        UMaterialEditingLibrary::ClearAllMaterialInstanceParameters(MIC);
        MIC->MarkPackageDirty();

        TSharedPtr<FJsonObject> Row = McpMakeItemResult(AssetPath, FString(), true, FString(), FString());
        Row->SetNumberField(TEXT("clearedCount"), ClearedCount);
        Results.Add(MakeShared<FJsonValueObject>(Row));
        ++SuccessCount;
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetArrayField(TEXT("results"), Results);
    Sub->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Cleared overrides on %d/%d instance(s)."),
            SuccessCount, ItemsArr->Num()),
        Resp);
    return true;
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}
