// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_Material_FunctionInstanceParameters.cpp
//
// Task F.2 - typed setters + unified getters/resetters/clearers for material
// function instance parameters.
//
// External handlers exposed by this TU:
//   - set_material_function_instance_scalar_parameters
//   - set_material_function_instance_vector_parameters
//   - set_material_function_instance_texture_parameters
//   - set_material_function_instance_static_switch_parameters
//   - get_material_function_instance_parameters
//   - reset_material_function_instance_parameters
//   - clear_material_function_instance_parameters
//
// UMaterialEditingLibrary has no MFI-specific Set*ParameterValue helpers, so
// each typed setter directly mutates the corresponding override array on the
// MFI and calls UpdateParameterSet(). Static switch overrides live on
// UMaterialFunctionInstance::StaticSwitchParameterValues with a per-entry
// bOverride flag (mirroring the MIC static-parameter pattern).
//
// MFI parameter inheritance: MFI -> Parent (Material/MFI) -> ... Reading
// 'parentValue' uses the parent's GetParameterOverrideValue lookup chain
// (MFI inherits from UMaterialFunctionInterface).
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

#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInstance.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialParameters.h"
#include "Engine/Texture.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"

#endif // WITH_EDITOR

DEFINE_LOG_CATEGORY_STATIC(LogMcpFunctionInstanceParams, Log, All);

namespace
{
#if WITH_EDITOR

// Single-item response row helper. Mirrors the MIC variant in F.1.
static TSharedPtr<FJsonObject> McpMakeMfiItemResult(
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

static bool McpIsEngineAssetMfi(const FString& AssetPath)
{
    return AssetPath.StartsWith(TEXT("/Engine/")) || AssetPath.StartsWith(TEXT("/EnginePlugins/"));
}

static UMaterialFunctionInstance* McpLoadFunctionInstance(
    FString& AssetPath, FString& OutError, FString& OutErrorCode)
{
    if (AssetPath.IsEmpty())
    {
        OutError = TEXT("assetPath is required");
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        return nullptr;
    }
    if (McpIsEngineAssetMfi(AssetPath))
    {
        OutError = FString::Printf(
            TEXT("Asset path '%s' is under engine content. Copy to /Game first."),
            *AssetPath);
        OutErrorCode = TEXT("ENGINE_ASSET_BLOCKED");
        return nullptr;
    }
    AssetPath = SanitizeProjectRelativePath(AssetPath);
    UMaterialFunctionInstance* MFI = LoadObject<UMaterialFunctionInstance>(nullptr, *AssetPath);
    if (!MFI)
    {
        OutError = FString::Printf(TEXT("Could not load UMaterialFunctionInstance '%s'."), *AssetPath);
        OutErrorCode = TEXT("ASSET_NOT_FOUND");
        return nullptr;
    }
    return MFI;
}

static bool McpReadLinearColorMfi(const TSharedPtr<FJsonValue>& Val, FLinearColor& Out)
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

// Walks the base function's expressions to determine whether a parameter of
// the given typed-name exists. UMaterialEditingLibrary's parameter-name
// helpers operate on UMaterialInterface, not function instances, so we
// inspect the base function's expression list directly.
//
// Returns true if a UMaterialExpression*Parameter with the matching name is
// declared on the resolved base function.
template <typename TParam>
static bool McpBaseFunctionHasParam(UMaterialFunctionInstance* MFI, FName ParamName)
{
    if (!MFI) return false;
    UMaterialFunction* Base = MFI->GetBaseFunction();
    if (!Base) return false;
    const TArray<TObjectPtr<UMaterialExpression>>& Exprs = Base->GetExpressionCollection().Expressions;
    for (const TObjectPtr<UMaterialExpression>& E : Exprs)
    {
        if (TParam* P = Cast<TParam>(E.Get()))
        {
            if (P->ParameterName == ParamName) return true;
        }
    }
    return false;
}

// Static-switch parameter discovery covers both UMaterialExpressionStaticBoolParameter
// and UMaterialExpressionStaticSwitchParameter (the engine treats both as
// "static switch" overrides on MIC; for MFI we follow the same convention).
static bool McpBaseFunctionHasStaticSwitchParam(UMaterialFunctionInstance* MFI, FName ParamName)
{
    if (!MFI) return false;
    UMaterialFunction* Base = MFI->GetBaseFunction();
    if (!Base) return false;
    const TArray<TObjectPtr<UMaterialExpression>>& Exprs = Base->GetExpressionCollection().Expressions;
    for (const TObjectPtr<UMaterialExpression>& E : Exprs)
    {
        if (UMaterialExpressionStaticBoolParameter* B = Cast<UMaterialExpressionStaticBoolParameter>(E.Get()))
        {
            if (B->ParameterName == ParamName) return true;
        }
        if (UMaterialExpressionStaticSwitchParameter* S = Cast<UMaterialExpressionStaticSwitchParameter>(E.Get()))
        {
            if (S->ParameterName == ParamName) return true;
        }
    }
    return false;
}

// Tries to read a parent value of the requested type via MFI->Parent's
// GetParameterOverrideValue. Returns true if the parent supplied a value.
static bool McpReadMfiParentScalar(UMaterialFunctionInstance* MFI, FName ParamName, float& Out)
{
    Out = 0.0f;
    if (!MFI || !MFI->Parent) return false;
    FMaterialParameterMetadata Meta;
    if (!MFI->Parent->GetParameterOverrideValue(EMaterialParameterType::Scalar, ParamName, Meta))
    {
        return false;
    }
    Out = Meta.Value.AsScalar();
    return true;
}

static bool McpReadMfiParentVector(UMaterialFunctionInstance* MFI, FName ParamName, FLinearColor& Out)
{
    Out = FLinearColor::Black;
    if (!MFI || !MFI->Parent) return false;
    FMaterialParameterMetadata Meta;
    if (!MFI->Parent->GetParameterOverrideValue(EMaterialParameterType::Vector, ParamName, Meta))
    {
        return false;
    }
    Out = Meta.Value.AsLinearColor();
    return true;
}

static bool McpReadMfiParentTexture(UMaterialFunctionInstance* MFI, FName ParamName, UTexture*& Out)
{
    Out = nullptr;
    if (!MFI || !MFI->Parent) return false;
    FMaterialParameterMetadata Meta;
    if (!MFI->Parent->GetParameterOverrideValue(EMaterialParameterType::Texture, ParamName, Meta))
    {
        return false;
    }
    Out = Meta.Value.Texture;
    return true;
}

static bool McpReadMfiParentStaticSwitch(UMaterialFunctionInstance* MFI, FName ParamName, bool& Out)
{
    Out = false;
    if (!MFI || !MFI->Parent) return false;
    FMaterialParameterMetadata Meta;
    if (!MFI->Parent->GetParameterOverrideValue(EMaterialParameterType::StaticSwitch, ParamName, Meta))
    {
        return false;
    }
    Out = Meta.Value.AsStaticSwitch();
    return true;
}

// Effective value resolution for the get_* response. Walks: MFI override
// (if any) -> parent value (recursive) -> default.
static float McpEffectiveScalar(UMaterialFunctionInstance* MFI, FName ParamName, bool& bOverridden)
{
    bOverridden = false;
    if (!MFI) return 0.0f;
    for (const FScalarParameterValue& V : MFI->ScalarParameterValues)
    {
        if (V.ParameterInfo.Name == ParamName)
        {
            bOverridden = true;
            return V.ParameterValue;
        }
    }
    float Parent = 0.0f;
    McpReadMfiParentScalar(MFI, ParamName, Parent);
    return Parent;
}

static FLinearColor McpEffectiveVector(UMaterialFunctionInstance* MFI, FName ParamName, bool& bOverridden)
{
    bOverridden = false;
    if (!MFI) return FLinearColor::Black;
    for (const FVectorParameterValue& V : MFI->VectorParameterValues)
    {
        if (V.ParameterInfo.Name == ParamName)
        {
            bOverridden = true;
            return V.ParameterValue;
        }
    }
    FLinearColor Parent = FLinearColor::Black;
    McpReadMfiParentVector(MFI, ParamName, Parent);
    return Parent;
}

static UTexture* McpEffectiveTexture(UMaterialFunctionInstance* MFI, FName ParamName, bool& bOverridden)
{
    bOverridden = false;
    if (!MFI) return nullptr;
    for (const FTextureParameterValue& V : MFI->TextureParameterValues)
    {
        if (V.ParameterInfo.Name == ParamName)
        {
            bOverridden = true;
            return V.ParameterValue.Get();
        }
    }
    UTexture* Parent = nullptr;
    McpReadMfiParentTexture(MFI, ParamName, Parent);
    return Parent;
}

static bool McpEffectiveStaticSwitch(UMaterialFunctionInstance* MFI, FName ParamName, bool& bOverridden)
{
    bOverridden = false;
    if (!MFI) return false;
    for (const FStaticSwitchParameter& V : MFI->StaticSwitchParameterValues)
    {
        if (V.ParameterInfo.Name == ParamName && V.bOverride)
        {
            bOverridden = true;
            return V.Value;
        }
    }
    bool Parent = false;
    McpReadMfiParentStaticSwitch(MFI, ParamName, Parent);
    return Parent;
}

// Build a single parameters[] entry with the F-spec shape.
static TSharedPtr<FJsonObject> McpMfiBuildParamRow_Scalar(
    UMaterialFunctionInstance* MFI, const FName& ParamName)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("name"), ParamName.ToString());
    Row->SetStringField(TEXT("type"), TEXT("scalar"));
    bool bOverridden = false;
    const float Eff = McpEffectiveScalar(MFI, ParamName, bOverridden);
    Row->SetNumberField(TEXT("value"), Eff);
    Row->SetBoolField(TEXT("isOverridden"), bOverridden);
    float ParentVal = 0.0f;
    McpReadMfiParentScalar(MFI, ParamName, ParentVal);
    Row->SetNumberField(TEXT("parentValue"), ParentVal);
    return Row;
}

static TSharedPtr<FJsonObject> McpMfiBuildParamRow_Vector(
    UMaterialFunctionInstance* MFI, const FName& ParamName)
{
    auto MakeColor = [](const FLinearColor& C) -> TSharedPtr<FJsonObject>
    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetNumberField(TEXT("r"), C.R);
        O->SetNumberField(TEXT("g"), C.G);
        O->SetNumberField(TEXT("b"), C.B);
        O->SetNumberField(TEXT("a"), C.A);
        return O;
    };
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("name"), ParamName.ToString());
    Row->SetStringField(TEXT("type"), TEXT("vector"));
    bool bOverridden = false;
    const FLinearColor Eff = McpEffectiveVector(MFI, ParamName, bOverridden);
    Row->SetObjectField(TEXT("value"), MakeColor(Eff));
    Row->SetBoolField(TEXT("isOverridden"), bOverridden);
    FLinearColor ParentVal = FLinearColor::Black;
    McpReadMfiParentVector(MFI, ParamName, ParentVal);
    Row->SetObjectField(TEXT("parentValue"), MakeColor(ParentVal));
    return Row;
}

static TSharedPtr<FJsonObject> McpMfiBuildParamRow_Texture(
    UMaterialFunctionInstance* MFI, const FName& ParamName)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("name"), ParamName.ToString());
    Row->SetStringField(TEXT("type"), TEXT("texture"));
    bool bOverridden = false;
    UTexture* Eff = McpEffectiveTexture(MFI, ParamName, bOverridden);
    Row->SetStringField(TEXT("value"), Eff ? Eff->GetPathName() : TEXT(""));
    Row->SetBoolField(TEXT("isOverridden"), bOverridden);
    UTexture* ParentTex = nullptr;
    McpReadMfiParentTexture(MFI, ParamName, ParentTex);
    Row->SetStringField(TEXT("parentValue"), ParentTex ? ParentTex->GetPathName() : TEXT(""));
    return Row;
}

static TSharedPtr<FJsonObject> McpMfiBuildParamRow_StaticSwitch(
    UMaterialFunctionInstance* MFI, const FName& ParamName)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("name"), ParamName.ToString());
    Row->SetStringField(TEXT("type"), TEXT("staticSwitch"));
    bool bOverridden = false;
    const bool bEff = McpEffectiveStaticSwitch(MFI, ParamName, bOverridden);
    Row->SetBoolField(TEXT("value"), bEff);
    Row->SetBoolField(TEXT("isOverridden"), bOverridden);
    bool bParent = false;
    McpReadMfiParentStaticSwitch(MFI, ParamName, bParent);
    Row->SetBoolField(TEXT("parentValue"), bParent);
    return Row;
}

// Enumerate parameter names declared on the resolved base function.
static void McpCollectFunctionParameterNames(
    UMaterialFunctionInstance* MFI,
    TArray<FName>& OutScalarNames,
    TArray<FName>& OutVectorNames,
    TArray<FName>& OutTextureNames,
    TArray<FName>& OutStaticSwitchNames)
{
    OutScalarNames.Reset();
    OutVectorNames.Reset();
    OutTextureNames.Reset();
    OutStaticSwitchNames.Reset();
    if (!MFI) return;
    UMaterialFunction* Base = MFI->GetBaseFunction();
    if (!Base) return;
    const TArray<TObjectPtr<UMaterialExpression>>& Exprs = Base->GetExpressionCollection().Expressions;
    for (const TObjectPtr<UMaterialExpression>& E : Exprs)
    {
        if (UMaterialExpressionScalarParameter* P = Cast<UMaterialExpressionScalarParameter>(E.Get()))
        {
            OutScalarNames.AddUnique(P->ParameterName);
            continue;
        }
        if (UMaterialExpressionVectorParameter* P = Cast<UMaterialExpressionVectorParameter>(E.Get()))
        {
            OutVectorNames.AddUnique(P->ParameterName);
            continue;
        }
        if (UMaterialExpressionTextureSampleParameter* P = Cast<UMaterialExpressionTextureSampleParameter>(E.Get()))
        {
            OutTextureNames.AddUnique(P->ParameterName);
            continue;
        }
        if (UMaterialExpressionStaticBoolParameter* P = Cast<UMaterialExpressionStaticBoolParameter>(E.Get()))
        {
            OutStaticSwitchNames.AddUnique(P->ParameterName);
            continue;
        }
        if (UMaterialExpressionStaticSwitchParameter* P = Cast<UMaterialExpressionStaticSwitchParameter>(E.Get()))
        {
            OutStaticSwitchNames.AddUnique(P->ParameterName);
            continue;
        }
    }
}

// Collect the full parameters[] list for one MFI.
static TArray<TSharedPtr<FJsonValue>> McpMfiBuildAllParamRows(UMaterialFunctionInstance* MFI)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    TArray<FName> Scalars, Vectors, Textures, Statics;
    McpCollectFunctionParameterNames(MFI, Scalars, Vectors, Textures, Statics);
    for (const FName& N : Scalars)
    {
        Out.Add(MakeShared<FJsonValueObject>(McpMfiBuildParamRow_Scalar(MFI, N)));
    }
    for (const FName& N : Vectors)
    {
        Out.Add(MakeShared<FJsonValueObject>(McpMfiBuildParamRow_Vector(MFI, N)));
    }
    for (const FName& N : Textures)
    {
        Out.Add(MakeShared<FJsonValueObject>(McpMfiBuildParamRow_Texture(MFI, N)));
    }
    for (const FName& N : Statics)
    {
        Out.Add(MakeShared<FJsonValueObject>(McpMfiBuildParamRow_StaticSwitch(MFI, N)));
    }
    return Out;
}

// Per-item dispatch helper for the four typed setters.
static TSharedPtr<FJsonObject> McpApplyMfiTypedSetter(
    const TSharedPtr<FJsonObject>& Item,
    const FString& ValueKind /* "scalar" | "vector" | "texture" | "staticSwitch" */)
{
    FString AssetPath, ParameterName;
    if (!Item.IsValid())
    {
        return McpMakeMfiItemResult(TEXT(""), TEXT(""), false,
            TEXT("Item is not an object"), TEXT("INVALID_ARGUMENT"));
    }
    Item->TryGetStringField(TEXT("assetPath"), AssetPath);
    Item->TryGetStringField(TEXT("parameterName"), ParameterName);

    if (ParameterName.IsEmpty())
    {
        return McpMakeMfiItemResult(AssetPath, ParameterName, false,
            TEXT("parameterName is required"), TEXT("INVALID_ARGUMENT"));
    }

    FString LoadErr, LoadCode;
    UMaterialFunctionInstance* MFI = McpLoadFunctionInstance(AssetPath, LoadErr, LoadCode);
    if (!MFI)
    {
        return McpMakeMfiItemResult(AssetPath, ParameterName, false, LoadErr, LoadCode);
    }

    const FName PName(*ParameterName);
    FString FailMsg, FailCode;
    bool bApplied = false;

    FScopedTransaction Tx(NSLOCTEXT("McpAutomationBridge", "McpSetMfiParam",
        "MCP set material function instance parameter"));
    MFI->Modify();

    const FMaterialParameterInfo Info(PName);

    if (ValueKind == TEXT("scalar"))
    {
        const TSharedPtr<FJsonValue> ValueField = Item->TryGetField(TEXT("value"));
        if (!ValueField.IsValid() || ValueField->Type != EJson::Number)
        {
            FailMsg = TEXT("value must be a number for scalar parameter");
            FailCode = TEXT("INVALID_ARGUMENT");
        }
        else if (!McpBaseFunctionHasParam<UMaterialExpressionScalarParameter>(MFI, PName))
        {
            FailMsg = FString::Printf(TEXT("Scalar parameter '%s' not declared on base function."), *ParameterName);
            FailCode = TEXT("PARAMETER_NOT_FOUND");
        }
        else
        {
            const float V = (float)ValueField->AsNumber();
            bool bFound = false;
            for (FScalarParameterValue& Existing : MFI->ScalarParameterValues)
            {
                if (Existing.ParameterInfo.Name == PName)
                {
                    Existing.ParameterValue = V;
                    bFound = true;
                    break;
                }
            }
            if (!bFound)
            {
                FScalarParameterValue NP;
                NP.ParameterInfo = Info;
                NP.ParameterValue = V;
                MFI->ScalarParameterValues.Add(NP);
            }
            bApplied = true;
        }
    }
    else if (ValueKind == TEXT("vector"))
    {
        const TSharedPtr<FJsonValue> ValueField = Item->TryGetField(TEXT("value"));
        FLinearColor Color;
        if (!McpReadLinearColorMfi(ValueField, Color))
        {
            FailMsg = TEXT("value must be an object {r,g,b,a} for vector parameter");
            FailCode = TEXT("INVALID_ARGUMENT");
        }
        else if (!McpBaseFunctionHasParam<UMaterialExpressionVectorParameter>(MFI, PName))
        {
            FailMsg = FString::Printf(TEXT("Vector parameter '%s' not declared on base function."), *ParameterName);
            FailCode = TEXT("PARAMETER_NOT_FOUND");
        }
        else
        {
            bool bFound = false;
            for (FVectorParameterValue& Existing : MFI->VectorParameterValues)
            {
                if (Existing.ParameterInfo.Name == PName)
                {
                    Existing.ParameterValue = Color;
                    bFound = true;
                    break;
                }
            }
            if (!bFound)
            {
                FVectorParameterValue NP;
                NP.ParameterInfo = Info;
                NP.ParameterValue = Color;
                MFI->VectorParameterValues.Add(NP);
            }
            bApplied = true;
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
        else if (!McpBaseFunctionHasParam<UMaterialExpressionTextureSampleParameter>(MFI, PName))
        {
            FailMsg = FString::Printf(TEXT("Texture parameter '%s' not declared on base function."), *ParameterName);
            FailCode = TEXT("PARAMETER_NOT_FOUND");
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
                bool bFound = false;
                for (FTextureParameterValue& Existing : MFI->TextureParameterValues)
                {
                    if (Existing.ParameterInfo.Name == PName)
                    {
                        Existing.ParameterValue = Tex;
                        bFound = true;
                        break;
                    }
                }
                if (!bFound)
                {
                    FTextureParameterValue NP;
                    NP.ParameterInfo = Info;
                    NP.ParameterValue = Tex;
                    MFI->TextureParameterValues.Add(NP);
                }
                bApplied = true;
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
        else if (!McpBaseFunctionHasStaticSwitchParam(MFI, PName))
        {
            FailMsg = FString::Printf(TEXT("StaticSwitch parameter '%s' not declared on base function."), *ParameterName);
            FailCode = TEXT("PARAMETER_NOT_FOUND");
        }
        else
        {
            const bool V = ValueField->AsBool();
            bool bFound = false;
            for (FStaticSwitchParameter& Existing : MFI->StaticSwitchParameterValues)
            {
                if (Existing.ParameterInfo.Name == PName)
                {
                    Existing.Value = V;
                    Existing.bOverride = true;
                    bFound = true;
                    break;
                }
            }
            if (!bFound)
            {
                FStaticSwitchParameter NP;
                NP.ParameterInfo = Info;
                NP.Value = V;
                NP.bOverride = true;
                MFI->StaticSwitchParameterValues.Add(NP);
            }
            bApplied = true;
        }
    }
    else
    {
        FailMsg = FString::Printf(TEXT("Unknown valueKind '%s'"), *ValueKind);
        FailCode = TEXT("INTERNAL_ERROR");
    }

    if (bApplied)
    {
        MFI->UpdateParameterSet();
        MFI->MarkPackageDirty();
        return McpMakeMfiItemResult(AssetPath, ParameterName, true, FString(), FString());
    }
    return McpMakeMfiItemResult(AssetPath, ParameterName, false, FailMsg, FailCode);
}

static bool McpHandle_TypedSetterBatchMfi(
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
        TSharedPtr<FJsonObject> Row = McpApplyMfiTypedSetter(Item, ValueKind);
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
extern bool McpHandle_SetMaterialFunctionInstanceScalarParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
extern bool McpHandle_SetMaterialFunctionInstanceVectorParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
extern bool McpHandle_SetMaterialFunctionInstanceTextureParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
extern bool McpHandle_SetMaterialFunctionInstanceStaticSwitchParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
extern bool McpHandle_GetMaterialFunctionInstanceParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
extern bool McpHandle_ResetMaterialFunctionInstanceParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
extern bool McpHandle_ClearMaterialFunctionInstanceParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);

bool McpHandle_SetMaterialFunctionInstanceScalarParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_TypedSetterBatchMfi(Sub, RequestId, Payload, Socket, TEXT("scalar"));
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_SetMaterialFunctionInstanceVectorParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_TypedSetterBatchMfi(Sub, RequestId, Payload, Socket, TEXT("vector"));
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_SetMaterialFunctionInstanceTextureParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_TypedSetterBatchMfi(Sub, RequestId, Payload, Socket, TEXT("texture"));
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_SetMaterialFunctionInstanceStaticSwitchParameters(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_TypedSetterBatchMfi(Sub, RequestId, Payload, Socket, TEXT("staticSwitch"));
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_GetMaterialFunctionInstanceParameters(
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
        UMaterialFunctionInstance* MFI = McpLoadFunctionInstance(AssetPath, LoadErr, LoadCode);
        if (!MFI)
        {
            Results.Add(MakeShared<FJsonValueObject>(
                McpMakeMfiItemResult(AssetPath, FString(), false, LoadErr, LoadCode)));
            continue;
        }

        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("assetPath"), AssetPath);
        Row->SetBoolField(TEXT("success"), true);
        Row->SetStringField(TEXT("parentPath"),
            MFI->Parent ? MFI->Parent->GetPathName() : TEXT(""));
        UMaterialFunction* BaseFn = MFI->GetBaseFunction();
        Row->SetStringField(TEXT("baseFunctionPath"),
            BaseFn ? BaseFn->GetPathName() : TEXT(""));
        Row->SetArrayField(TEXT("parameters"), McpMfiBuildAllParamRows(MFI));
        Results.Add(MakeShared<FJsonValueObject>(Row));
        ++SuccessCount;
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetArrayField(TEXT("results"), Results);
    Sub->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Inspected %d/%d function instance(s)."),
            SuccessCount, Paths.Num()),
        Resp);
    return true;
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_ResetMaterialFunctionInstanceParameters(
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
            Results.Add(MakeShared<FJsonValueObject>(McpMakeMfiItemResult(
                AssetPath, ParameterName, false,
                TEXT("parameterName is required"), TEXT("INVALID_ARGUMENT"))));
            continue;
        }

        FString LoadErr, LoadCode;
        UMaterialFunctionInstance* MFI = McpLoadFunctionInstance(AssetPath, LoadErr, LoadCode);
        if (!MFI)
        {
            Results.Add(MakeShared<FJsonValueObject>(McpMakeMfiItemResult(
                AssetPath, ParameterName, false, LoadErr, LoadCode)));
            continue;
        }

        FScopedTransaction Tx(NSLOCTEXT("McpAutomationBridge", "McpResetMfiParam",
            "MCP reset material function instance parameter"));
        MFI->Modify();

        const FName PN(*ParameterName);
        const int32 ScalarRemoved = MFI->ScalarParameterValues.RemoveAll(
            [&PN](const FScalarParameterValue& V){ return V.ParameterInfo.Name == PN; });
        const int32 VectorRemoved = MFI->VectorParameterValues.RemoveAll(
            [&PN](const FVectorParameterValue& V){ return V.ParameterInfo.Name == PN; });
        const int32 TextureRemoved = MFI->TextureParameterValues.RemoveAll(
            [&PN](const FTextureParameterValue& V){ return V.ParameterInfo.Name == PN; });
        const int32 StaticRemoved = MFI->StaticSwitchParameterValues.RemoveAll(
            [&PN](const FStaticSwitchParameter& V){ return V.ParameterInfo.Name == PN; });

        const int32 TotalRemoved = ScalarRemoved + VectorRemoved + TextureRemoved + StaticRemoved;
        if (TotalRemoved == 0)
        {
            Results.Add(MakeShared<FJsonValueObject>(McpMakeMfiItemResult(
                AssetPath, ParameterName, false,
                FString::Printf(TEXT("No override found for parameter '%s'."), *ParameterName),
                TEXT("OVERRIDE_NOT_FOUND"))));
            continue;
        }

        MFI->UpdateParameterSet();
        MFI->MarkPackageDirty();

        TSharedPtr<FJsonObject> Row = McpMakeMfiItemResult(AssetPath, ParameterName, true, FString(), FString());
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

bool McpHandle_ClearMaterialFunctionInstanceParameters(
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
        UMaterialFunctionInstance* MFI = McpLoadFunctionInstance(AssetPath, LoadErr, LoadCode);
        if (!MFI)
        {
            Results.Add(MakeShared<FJsonValueObject>(McpMakeMfiItemResult(
                AssetPath, FString(), false, LoadErr, LoadCode)));
            continue;
        }

        FScopedTransaction Tx(NSLOCTEXT("McpAutomationBridge", "McpClearMfiParams",
            "MCP clear material function instance parameters"));
        MFI->Modify();

        const int32 ClearedCount =
            MFI->ScalarParameterValues.Num() +
            MFI->VectorParameterValues.Num() +
            MFI->TextureParameterValues.Num() +
            MFI->StaticSwitchParameterValues.Num();

        // No engine helper for MFI-clear; mirror the per-array empty pattern
        // used by the legacy single-action handler and call UpdateParameterSet
        // to refresh the parameter cache.
        MFI->ScalarParameterValues.Empty();
        MFI->VectorParameterValues.Empty();
        MFI->TextureParameterValues.Empty();
        MFI->StaticSwitchParameterValues.Empty();
        // DoubleVector / Font / RVT / SVT / TextureCollection / ParameterCollection /
        // StaticComponentMask are explicitly out of scope per spec sec 9 (typed
        // setters cover scalar/vector/texture/staticSwitch only). Leave them
        // alone here so a future extension can clear them with awareness.
        MFI->UpdateParameterSet();
        MFI->MarkPackageDirty();

        TSharedPtr<FJsonObject> Row = McpMakeMfiItemResult(AssetPath, FString(), true, FString(), FString());
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
