// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_Material_FunctionAuthoring.cpp
//
// Task C.6 - function authoring + function calls.
//
// Handlers in this TU:
//   - add_function_inputs       (UMaterialExpressionFunctionInput, function-only)
//   - add_function_outputs      (UMaterialExpressionFunctionOutput, function-only)
//   - update_function_inputs    (preserves stable Id GUID)
//   - update_function_outputs   (preserves stable Id GUID)
//   - add_material_function_calls
//   - update_material_function_calls (re-bind with pin-layout diff + onPinRemoved)
//
// Plan: docs/superpowers/plans/2026-05-07-mcp-material-tools-redesign.md
// Spec: docs/superpowers/specs/2026-05-07-mcp-material-tools-redesign-design.md (sections 7.10 / 7.11)
//
// Shared types/helpers come from McpAutomationBridge_Material_Internal.h
// (extracted from Material_GraphWrites.cpp during C.5).

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridge_Material_Internal.h"

#if WITH_EDITOR

#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "MaterialExpressionIO.h"
#include "ScopedTransaction.h"

#endif // WITH_EDITOR

DEFINE_LOG_CATEGORY_STATIC(LogMcpFunctionAuthoring, Log, All);

namespace
{
#if WITH_EDITOR

using McpMaterialInternal::FMcpNodeValidationError;
using McpMaterialInternal::McpFormatNodeError;
using McpMaterialInternal::McpFormatIdentifierForDisplay;
using McpMaterialInternal::McpResolveUpdateIdentifier;

// Allowed inputType values for add_function_inputs / update_function_inputs.
// Mirrors EFunctionInputType, restricted to the public allow-list per spec.
static const TArray<FString>& McpAllowedFunctionInputTypes()
{
    static const TArray<FString> S = {
        TEXT("Float1"), TEXT("Float2"), TEXT("Float3"), TEXT("Float4"),
        TEXT("Texture2D"), TEXT("TextureCube"),
        TEXT("Bool"), TEXT("MaterialAttributes")
    };
    return S;
}

// Maps the wire string name to EFunctionInputType. Returns FunctionInput_MAX on miss.
static EFunctionInputType McpParseFunctionInputType(const FString& S)
{
    if (S == TEXT("Float1"))             return FunctionInput_Scalar;
    if (S == TEXT("Float2"))             return FunctionInput_Vector2;
    if (S == TEXT("Float3"))             return FunctionInput_Vector3;
    if (S == TEXT("Float4"))             return FunctionInput_Vector4;
    if (S == TEXT("Texture2D"))          return FunctionInput_Texture2D;
    if (S == TEXT("TextureCube"))        return FunctionInput_TextureCube;
    if (S == TEXT("Bool"))               return FunctionInput_StaticBool;
    if (S == TEXT("MaterialAttributes")) return FunctionInput_MaterialAttributes;
    return FunctionInput_MAX;
}

// Builds an INVALID_ENUM_VALUE error citing the allowed list.
static void McpFillEnumError_FunctionAuthoring(
    int32 Index, const FString& LocalId, const FString& Field,
    const FString& Got, const TArray<FString>& Allowed,
    FMcpNodeValidationError& Out)
{
    Out.Index = Index;
    Out.LocalId = LocalId;
    Out.Field = Field;
    Out.Code = TEXT("INVALID_ENUM_VALUE");
    FString Joined;
    for (int32 i = 0; i < Allowed.Num(); ++i)
    {
        if (i > 0) Joined += TEXT(", ");
        Joined += Allowed[i];
    }
    Out.Message = FString::Printf(
        TEXT("'%s' is not a valid value for %s. Allowed: %s"),
        *Got, *Field, *Joined);
}

// Applies caller-provided previewValue onto the FunctionInput's PreviewValue field.
// previewValue can be: a number (treated as X for Float1), or an object {x,y,z,w},
// or {value: ...}. For Bool we accept {value: bool}. Texture/MaterialAttributes
// types ignore the field.
static void McpApplyPreviewValue(
    UMaterialExpressionFunctionInput* Expr,
    const TSharedPtr<FJsonValue>& PreviewVal)
{
    if (!Expr || !PreviewVal.IsValid()) return;

    const EFunctionInputType T = static_cast<EFunctionInputType>(Expr->InputType);

    auto ReadNumberFromJson = [](const TSharedPtr<FJsonValue>& V, double Default) -> double
    {
        if (!V.IsValid()) return Default;
        if (V->Type == EJson::Number) return V->AsNumber();
        if (V->Type == EJson::Boolean) return V->AsBool() ? 1.0 : 0.0;
        return Default;
    };

    if (T == FunctionInput_Scalar)
    {
        if (PreviewVal->Type == EJson::Number)
        {
            Expr->PreviewValue.X = (float)PreviewVal->AsNumber();
            return;
        }
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        if (PreviewVal->TryGetObject(Obj) && Obj && Obj->IsValid())
        {
            const TSharedPtr<FJsonValue> Field = (*Obj)->TryGetField(TEXT("value"));
            Expr->PreviewValue.X = (float)ReadNumberFromJson(Field, 0.0);
        }
        return;
    }
    if (T == FunctionInput_Vector2 || T == FunctionInput_Vector3 || T == FunctionInput_Vector4)
    {
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        if (PreviewVal->TryGetObject(Obj) && Obj && Obj->IsValid())
        {
            Expr->PreviewValue.X = (float)ReadNumberFromJson((*Obj)->TryGetField(TEXT("x")), 0.0);
            Expr->PreviewValue.Y = (float)ReadNumberFromJson((*Obj)->TryGetField(TEXT("y")), 0.0);
            if (T == FunctionInput_Vector3 || T == FunctionInput_Vector4)
            {
                Expr->PreviewValue.Z = (float)ReadNumberFromJson((*Obj)->TryGetField(TEXT("z")), 0.0);
            }
            if (T == FunctionInput_Vector4)
            {
                Expr->PreviewValue.W = (float)ReadNumberFromJson((*Obj)->TryGetField(TEXT("w")), 0.0);
            }
        }
        return;
    }
    if (T == FunctionInput_StaticBool)
    {
        // For Bool: {value: bool} or bare bool/number
        if (PreviewVal->Type == EJson::Boolean)
        {
            Expr->PreviewValue.X = PreviewVal->AsBool() ? 1.0f : 0.0f;
            return;
        }
        if (PreviewVal->Type == EJson::Number)
        {
            Expr->PreviewValue.X = PreviewVal->AsNumber() != 0.0 ? 1.0f : 0.0f;
            return;
        }
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        if (PreviewVal->TryGetObject(Obj) && Obj && Obj->IsValid())
        {
            const TSharedPtr<FJsonValue> V = (*Obj)->TryGetField(TEXT("value"));
            if (V.IsValid())
            {
                Expr->PreviewValue.X = ReadNumberFromJson(V, 0.0) != 0.0 ? 1.0f : 0.0f;
            }
        }
        return;
    }
    // Texture2D / TextureCube / MaterialAttributes: ignore (no preview value)
}

// Owner must be a UMaterialFunction (subclass OK). UMaterialFunctionInstance is
// already rejected upstream via Owner.bReadOnly. Returns false + emits error if
// the owner is a UMaterial.
static bool McpRequireFunctionOwner(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    const FMcpMaterialGraphOwner& Owner,
    const FString& SubAction)
{
    if (Owner.Kind != EMcpMaterialGraphOwnerKind::MaterialFunction)
    {
        Sub->SendAutomationError(Socket, RequestId,
            FString::Printf(
                TEXT("%s requires a UMaterialFunction owner; got %s"),
                *SubAction,
                Owner.Asset ? *Owner.Asset->GetClass()->GetName() : TEXT("<null>")),
            TEXT("UNSUPPORTED_OPERATION"));
        return false;
    }
    return true;
}

#endif // WITH_EDITOR
} // namespace

// =============================================================================
// External entry: McpHandle_AddFunctionInputs (Task C.6)
//
// Payload: { assetPath, inputs: [...], save?: bool }
// Only valid when assetPath is a UMaterialFunction. All-or-nothing batch.
// =============================================================================
extern bool McpHandle_AddFunctionInputs(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool McpHandle_AddFunctionInputs(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    if (!Sub || !Payload.IsValid())
    {
        if (Sub) Sub->SendAutomationError(Socket, RequestId,
            TEXT("Invalid payload"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("assetPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (AssetPath.StartsWith(TEXT("/Engine/")) || AssetPath.StartsWith(TEXT("/EnginePlugins/")))
    {
        Sub->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Asset path '%s' is under engine content. Copy to /Game first."), *AssetPath),
            TEXT("ENGINE_ASSET_BLOCKED"));
        return true;
    }

    FMcpMaterialGraphOwner Owner;
    FString OwnerErr;
    if (!McpResolveMaterialGraphOwner(AssetPath, Owner, OwnerErr) || Owner.bReadOnly)
    {
        const FString Code = OwnerErr.Contains(TEXT("not found"))
            ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE");
        Sub->SendAutomationError(Socket, RequestId,
            OwnerErr.IsEmpty() ? TEXT("Cannot mutate this asset") : OwnerErr, Code);
        return true;
    }

    if (!McpRequireFunctionOwner(Sub, RequestId, Socket, Owner, TEXT("add_function_inputs")))
    {
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* InputsArr = nullptr;
    if (!Payload->TryGetArrayField(TEXT("inputs"), InputsArr) || !InputsArr || InputsArr->Num() == 0)
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("inputs[] is required, minimum length 1"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Phase A: validate every item; collect duplicate-name warnings.
    TSet<FString>                    SeenLocalIds;
    TSet<FString>                    SeenInputNames;     // duplicates within batch
    TArray<FMcpNodeValidationError>  NodeErrors;
    TArray<FString>                  DuplicateNameWarnings;

    // Existing input names on the function (for collision warnings).
    TSet<FString> ExistingInputNames;
    {
        const TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressions(Owner);
        if (Exprs)
        {
            for (UMaterialExpression* Expr : *Exprs)
            {
                if (UMaterialExpressionFunctionInput* In = Cast<UMaterialExpressionFunctionInput>(Expr))
                {
                    ExistingInputNames.Add(In->InputName.ToString());
                }
            }
        }
    }

    for (int32 i = 0; i < InputsArr->Num(); ++i)
    {
        FMcpNodeValidationError E;
        E.Index = i;

        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!(*InputsArr)[i].IsValid() || !(*InputsArr)[i]->TryGetObject(ObjPtr) || !ObjPtr || !ObjPtr->IsValid())
        {
            E.Code = TEXT("INVALID_NODE_SPEC");
            E.Field = TEXT("inputs[]");
            E.Message = TEXT("Item is not an object");
            NodeErrors.Add(E);
            continue;
        }
        const TSharedPtr<FJsonObject>& Item = *ObjPtr;

        FString LocalId;
        Item->TryGetStringField(TEXT("localId"), LocalId);
        E.LocalId = LocalId;
        if (!LocalId.IsEmpty())
        {
            if (SeenLocalIds.Contains(LocalId))
            {
                E.Field = TEXT("localId");
                E.Code = TEXT("LOCAL_ID_DUPLICATE");
                E.Message = FString::Printf(
                    TEXT("localId '%s' duplicates an earlier item in this batch"), *LocalId);
                NodeErrors.Add(E);
                continue;
            }
            SeenLocalIds.Add(LocalId);
        }

        FString InputName;
        if (!Item->TryGetStringField(TEXT("inputName"), InputName) || InputName.IsEmpty())
        {
            E.Field = TEXT("inputName");
            E.Code = TEXT("INVALID_ARGUMENT");
            E.Message = TEXT("'inputName' is required and must be a non-empty string");
            NodeErrors.Add(E);
            continue;
        }

        FString InputType;
        if (!Item->TryGetStringField(TEXT("inputType"), InputType) || InputType.IsEmpty())
        {
            E.Field = TEXT("inputType");
            E.Code = TEXT("INVALID_ARGUMENT");
            E.Message = TEXT("'inputType' is required and must be a non-empty string");
            NodeErrors.Add(E);
            continue;
        }
        if (McpParseFunctionInputType(InputType) == FunctionInput_MAX)
        {
            McpFillEnumError_FunctionAuthoring(i, LocalId, TEXT("inputType"),
                InputType, McpAllowedFunctionInputTypes(), E);
            NodeErrors.Add(E);
            continue;
        }

        // Duplicate-name detection (warning, not error).
        if (SeenInputNames.Contains(InputName))
        {
            DuplicateNameWarnings.Add(FString::Printf(
                TEXT("Duplicate inputName '%s' across this batch"), *InputName));
        }
        else
        {
            SeenInputNames.Add(InputName);
        }
        if (ExistingInputNames.Contains(InputName))
        {
            DuplicateNameWarnings.Add(FString::Printf(
                TEXT("inputName '%s' collides with an existing function input"), *InputName));
        }
    }

    if (NodeErrors.Num() > 0)
    {
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField  (TEXT("success"), false);
        Resp->SetStringField(TEXT("errorCode"), TEXT("VALIDATION_FAILED"));
        Resp->SetStringField(TEXT("message"),
            FString::Printf(TEXT("Batch rejected; 0 mutations applied. %d node errors."),
                NodeErrors.Num()));
        TArray<TSharedPtr<FJsonValue>> ErrorsJson;
        for (const auto& E : NodeErrors) ErrorsJson.Add(McpFormatNodeError(E));
        Resp->SetArrayField(TEXT("errors"), ErrorsJson);
        Sub->SendAutomationResponse(Socket, RequestId, false,
            TEXT("validation failed"), Resp, TEXT("VALIDATION_FAILED"));
        return true;
    }

    // Phase B: apply
    FScopedTransaction Tx(NSLOCTEXT("McpAutomationBridge",
        "McpAddFunctionInputs", "MCP add_function_inputs"));
    McpModifyMaterialGraphOwnerForTransaction(Owner);

    UObject* MaterialOuter = Owner.GraphSource ? Owner.GraphSource : Owner.Asset;
    TArray<TSharedPtr<FJsonObject>> Mappings;

    for (int32 i = 0; i < InputsArr->Num(); ++i)
    {
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!(*InputsArr)[i].IsValid() || !(*InputsArr)[i]->TryGetObject(ObjPtr) || !ObjPtr || !ObjPtr->IsValid())
        {
            continue;
        }
        const TSharedPtr<FJsonObject>& Item = *ObjPtr;

        UMaterialExpressionFunctionInput* Expr = NewObject<UMaterialExpressionFunctionInput>(
            MaterialOuter, UMaterialExpressionFunctionInput::StaticClass(), NAME_None, RF_Transactional);
        if (!Expr) continue;

        FString InputName;
        Item->TryGetStringField(TEXT("inputName"), InputName);
        Expr->InputName = FName(*InputName);

        FString InputType;
        Item->TryGetStringField(TEXT("inputType"), InputType);
        Expr->InputType = McpParseFunctionInputType(InputType);

        FString Description;
        if (Item->TryGetStringField(TEXT("description"), Description))
        {
            Expr->Description = Description;
        }

        bool bUsePreviewAsDefault = false;
        if (Item->TryGetBoolField(TEXT("usePreviewValueAsDefault"), bUsePreviewAsDefault))
        {
            Expr->bUsePreviewValueAsDefault = bUsePreviewAsDefault ? 1 : 0;
        }

        double SortPriority = 0;
        if (Item->TryGetNumberField(TEXT("sortPriority"), SortPriority))
        {
            Expr->SortPriority = (int32)SortPriority;
        }

        // previewValue is shaped by inputType; helper handles each variant.
        if (const TSharedPtr<FJsonValue> PV = Item->TryGetField(TEXT("previewValue")))
        {
            McpApplyPreviewValue(Expr, PV);
        }

        double X = 0, Y = 0;
        Item->TryGetNumberField(TEXT("x"), X);
        Item->TryGetNumberField(TEXT("y"), Y);
        Expr->MaterialExpressionEditorX = (int32)X;
        Expr->MaterialExpressionEditorY = (int32)Y;

        // Stable Id - referenced by call sites via FFunctionExpressionInput::ExpressionInputId.
        // Generated fresh on add; preserved on update.
        Expr->Id = FGuid::NewGuid();
        Expr->MaterialExpressionGuid = FGuid::NewGuid();

        TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressionsMutable(Owner);
        if (Exprs) Exprs->Add(Expr);

        FString LocalId;
        Item->TryGetStringField(TEXT("localId"), LocalId);

        TSharedPtr<FJsonObject> M = MakeShared<FJsonObject>();
        M->SetStringField(TEXT("localId"),         LocalId);
        M->SetStringField(TEXT("nodeId"),          Expr->GetName());
        M->SetStringField(TEXT("expressionGuid"),  Expr->MaterialExpressionGuid.ToString());
        M->SetNumberField(TEXT("expressionIndex"), Exprs ? (Exprs->Num() - 1) : 0);
        M->SetStringField(TEXT("functionInputId"), Expr->Id.ToString());
        Mappings.Add(M);
    }

    FString RebuildErr;
    McpRebuildMaterialGraphOwner(Owner, RebuildErr);

    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    bool bSaved = false;
    if (bSave)
    {
        bSaved = McpSafeAssetSave(Owner.Asset);
        if (!bSaved)
        {
            Tx.Cancel();
            Sub->SendAutomationError(Socket, RequestId,
                TEXT("Save failed; transaction rolled back"), TEXT("APPLY_FAILED"));
            return true;
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField  (TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetNumberField(TEXT("inputsCreated"), InputsArr->Num());
    Resp->SetBoolField  (TEXT("saved"), bSaved);

    TArray<TSharedPtr<FJsonValue>> MappingsJson;
    for (const auto& M : Mappings) MappingsJson.Add(MakeShared<FJsonValueObject>(M));
    Resp->SetArrayField(TEXT("mappings"), MappingsJson);

    if (DuplicateNameWarnings.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> WarnJson;
        for (const FString& W : DuplicateNameWarnings)
        {
            TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
            O->SetStringField(TEXT("code"),    TEXT("DUPLICATE_INPUT_NAME"));
            O->SetStringField(TEXT("message"), W);
            WarnJson.Add(MakeShared<FJsonValueObject>(O));
        }
        Resp->SetArrayField(TEXT("warnings"), WarnJson);
    }

    Sub->SendAutomationResponse(Socket, RequestId, true,
        TEXT("add_function_inputs succeeded"), Resp, FString());
    return true;
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("add_function_inputs requires editor build"), TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =============================================================================
// External entry: McpHandle_AddFunctionOutputs (Task C.6)
//
// Payload: { assetPath, outputs: [...], save?: bool }
// Note: outputType is silently ignored - UE's UMaterialExpressionFunctionOutput
// does not carry an explicit output-type field; the type is derived from what is
// wired to its 'A' input. Documented choice: silent ignore (per spec §7.10).
// =============================================================================
extern bool McpHandle_AddFunctionOutputs(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool McpHandle_AddFunctionOutputs(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    if (!Sub || !Payload.IsValid())
    {
        if (Sub) Sub->SendAutomationError(Socket, RequestId,
            TEXT("Invalid payload"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("assetPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (AssetPath.StartsWith(TEXT("/Engine/")) || AssetPath.StartsWith(TEXT("/EnginePlugins/")))
    {
        Sub->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Asset path '%s' is under engine content. Copy to /Game first."), *AssetPath),
            TEXT("ENGINE_ASSET_BLOCKED"));
        return true;
    }

    FMcpMaterialGraphOwner Owner;
    FString OwnerErr;
    if (!McpResolveMaterialGraphOwner(AssetPath, Owner, OwnerErr) || Owner.bReadOnly)
    {
        const FString Code = OwnerErr.Contains(TEXT("not found"))
            ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE");
        Sub->SendAutomationError(Socket, RequestId,
            OwnerErr.IsEmpty() ? TEXT("Cannot mutate this asset") : OwnerErr, Code);
        return true;
    }

    if (!McpRequireFunctionOwner(Sub, RequestId, Socket, Owner, TEXT("add_function_outputs")))
    {
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* OutputsArr = nullptr;
    if (!Payload->TryGetArrayField(TEXT("outputs"), OutputsArr) || !OutputsArr || OutputsArr->Num() == 0)
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("outputs[] is required, minimum length 1"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Phase A: validate every item
    TSet<FString>                    SeenLocalIds;
    TSet<FString>                    SeenOutputNames;
    TArray<FMcpNodeValidationError>  NodeErrors;
    TArray<FString>                  DuplicateNameWarnings;

    TSet<FString> ExistingOutputNames;
    {
        const TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressions(Owner);
        if (Exprs)
        {
            for (UMaterialExpression* Expr : *Exprs)
            {
                if (UMaterialExpressionFunctionOutput* Out = Cast<UMaterialExpressionFunctionOutput>(Expr))
                {
                    ExistingOutputNames.Add(Out->OutputName.ToString());
                }
            }
        }
    }

    for (int32 i = 0; i < OutputsArr->Num(); ++i)
    {
        FMcpNodeValidationError E;
        E.Index = i;

        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!(*OutputsArr)[i].IsValid() || !(*OutputsArr)[i]->TryGetObject(ObjPtr) || !ObjPtr || !ObjPtr->IsValid())
        {
            E.Code = TEXT("INVALID_NODE_SPEC");
            E.Field = TEXT("outputs[]");
            E.Message = TEXT("Item is not an object");
            NodeErrors.Add(E);
            continue;
        }
        const TSharedPtr<FJsonObject>& Item = *ObjPtr;

        FString LocalId;
        Item->TryGetStringField(TEXT("localId"), LocalId);
        E.LocalId = LocalId;
        if (!LocalId.IsEmpty())
        {
            if (SeenLocalIds.Contains(LocalId))
            {
                E.Field = TEXT("localId");
                E.Code = TEXT("LOCAL_ID_DUPLICATE");
                E.Message = FString::Printf(
                    TEXT("localId '%s' duplicates an earlier item in this batch"), *LocalId);
                NodeErrors.Add(E);
                continue;
            }
            SeenLocalIds.Add(LocalId);
        }

        FString OutputName;
        if (!Item->TryGetStringField(TEXT("outputName"), OutputName) || OutputName.IsEmpty())
        {
            E.Field = TEXT("outputName");
            E.Code = TEXT("INVALID_ARGUMENT");
            E.Message = TEXT("'outputName' is required and must be a non-empty string");
            NodeErrors.Add(E);
            continue;
        }

        if (SeenOutputNames.Contains(OutputName))
        {
            DuplicateNameWarnings.Add(FString::Printf(
                TEXT("Duplicate outputName '%s' across this batch"), *OutputName));
        }
        else
        {
            SeenOutputNames.Add(OutputName);
        }
        if (ExistingOutputNames.Contains(OutputName))
        {
            DuplicateNameWarnings.Add(FString::Printf(
                TEXT("outputName '%s' collides with an existing function output"), *OutputName));
        }
    }

    if (NodeErrors.Num() > 0)
    {
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField  (TEXT("success"), false);
        Resp->SetStringField(TEXT("errorCode"), TEXT("VALIDATION_FAILED"));
        Resp->SetStringField(TEXT("message"),
            FString::Printf(TEXT("Batch rejected; 0 mutations applied. %d node errors."),
                NodeErrors.Num()));
        TArray<TSharedPtr<FJsonValue>> ErrorsJson;
        for (const auto& E : NodeErrors) ErrorsJson.Add(McpFormatNodeError(E));
        Resp->SetArrayField(TEXT("errors"), ErrorsJson);
        Sub->SendAutomationResponse(Socket, RequestId, false,
            TEXT("validation failed"), Resp, TEXT("VALIDATION_FAILED"));
        return true;
    }

    // Phase B: apply
    FScopedTransaction Tx(NSLOCTEXT("McpAutomationBridge",
        "McpAddFunctionOutputs", "MCP add_function_outputs"));
    McpModifyMaterialGraphOwnerForTransaction(Owner);

    UObject* MaterialOuter = Owner.GraphSource ? Owner.GraphSource : Owner.Asset;
    TArray<TSharedPtr<FJsonObject>> Mappings;

    for (int32 i = 0; i < OutputsArr->Num(); ++i)
    {
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!(*OutputsArr)[i].IsValid() || !(*OutputsArr)[i]->TryGetObject(ObjPtr) || !ObjPtr || !ObjPtr->IsValid())
        {
            continue;
        }
        const TSharedPtr<FJsonObject>& Item = *ObjPtr;

        UMaterialExpressionFunctionOutput* Expr = NewObject<UMaterialExpressionFunctionOutput>(
            MaterialOuter, UMaterialExpressionFunctionOutput::StaticClass(), NAME_None, RF_Transactional);
        if (!Expr) continue;

        FString OutputName;
        Item->TryGetStringField(TEXT("outputName"), OutputName);
        Expr->OutputName = FName(*OutputName);

        FString Description;
        if (Item->TryGetStringField(TEXT("description"), Description))
        {
            Expr->Description = Description;
        }

        double SortPriority = 0;
        if (Item->TryGetNumberField(TEXT("sortPriority"), SortPriority))
        {
            Expr->SortPriority = (int32)SortPriority;
        }

        double X = 0, Y = 0;
        Item->TryGetNumberField(TEXT("x"), X);
        Item->TryGetNumberField(TEXT("y"), Y);
        Expr->MaterialExpressionEditorX = (int32)X;
        Expr->MaterialExpressionEditorY = (int32)Y;

        Expr->Id = FGuid::NewGuid();
        Expr->MaterialExpressionGuid = FGuid::NewGuid();

        TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressionsMutable(Owner);
        if (Exprs) Exprs->Add(Expr);

        FString LocalId;
        Item->TryGetStringField(TEXT("localId"), LocalId);

        TSharedPtr<FJsonObject> M = MakeShared<FJsonObject>();
        M->SetStringField(TEXT("localId"),          LocalId);
        M->SetStringField(TEXT("nodeId"),           Expr->GetName());
        M->SetStringField(TEXT("expressionGuid"),   Expr->MaterialExpressionGuid.ToString());
        M->SetNumberField(TEXT("expressionIndex"),  Exprs ? (Exprs->Num() - 1) : 0);
        M->SetStringField(TEXT("functionOutputId"), Expr->Id.ToString());
        Mappings.Add(M);
    }

    FString RebuildErr;
    McpRebuildMaterialGraphOwner(Owner, RebuildErr);

    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    bool bSaved = false;
    if (bSave)
    {
        bSaved = McpSafeAssetSave(Owner.Asset);
        if (!bSaved)
        {
            Tx.Cancel();
            Sub->SendAutomationError(Socket, RequestId,
                TEXT("Save failed; transaction rolled back"), TEXT("APPLY_FAILED"));
            return true;
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField  (TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetNumberField(TEXT("outputsCreated"), OutputsArr->Num());
    Resp->SetBoolField  (TEXT("saved"), bSaved);

    TArray<TSharedPtr<FJsonValue>> MappingsJson;
    for (const auto& M : Mappings) MappingsJson.Add(MakeShared<FJsonValueObject>(M));
    Resp->SetArrayField(TEXT("mappings"), MappingsJson);

    if (DuplicateNameWarnings.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> WarnJson;
        for (const FString& W : DuplicateNameWarnings)
        {
            TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
            O->SetStringField(TEXT("code"),    TEXT("DUPLICATE_INPUT_NAME"));
            O->SetStringField(TEXT("message"), W);
            WarnJson.Add(MakeShared<FJsonValueObject>(O));
        }
        Resp->SetArrayField(TEXT("warnings"), WarnJson);
    }

    Sub->SendAutomationResponse(Socket, RequestId, true,
        TEXT("add_function_outputs succeeded"), Resp, FString());
    return true;
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("add_function_outputs requires editor build"), TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =============================================================================
// update_function_inputs / update_function_outputs (Task C.6)
//
// Per-item: { identifier, ...applicableFields }. Identifier resolution via
// shared McpResolveUpdateIdentifier; resolved expression must be the matching
// UMaterialExpressionFunctionInput / UMaterialExpressionFunctionOutput class.
//
// Important! 'Id' GUID is preserved. Changing Id would invalidate every call
// site that stores it as ExpressionInputId / ExpressionOutputId.
// =============================================================================

extern bool McpHandle_UpdateFunctionInputs(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool McpHandle_UpdateFunctionInputs(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    if (!Sub || !Payload.IsValid())
    {
        if (Sub) Sub->SendAutomationError(Socket, RequestId,
            TEXT("Invalid payload"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("assetPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (AssetPath.StartsWith(TEXT("/Engine/")) || AssetPath.StartsWith(TEXT("/EnginePlugins/")))
    {
        Sub->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Asset path '%s' is under engine content. Copy to /Game first."), *AssetPath),
            TEXT("ENGINE_ASSET_BLOCKED"));
        return true;
    }

    FMcpMaterialGraphOwner Owner;
    FString OwnerErr;
    if (!McpResolveMaterialGraphOwner(AssetPath, Owner, OwnerErr) || Owner.bReadOnly)
    {
        const FString Code = OwnerErr.Contains(TEXT("not found"))
            ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE");
        Sub->SendAutomationError(Socket, RequestId,
            OwnerErr.IsEmpty() ? TEXT("Cannot mutate this asset") : OwnerErr, Code);
        return true;
    }

    if (!McpRequireFunctionOwner(Sub, RequestId, Socket, Owner, TEXT("update_function_inputs")))
    {
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* InputsArr = nullptr;
    if (!Payload->TryGetArrayField(TEXT("updates"), InputsArr) || !InputsArr || InputsArr->Num() == 0)
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("updates[] is required, minimum length 1"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    struct FResolved
    {
        UMaterialExpressionFunctionInput* Expr = nullptr;
        TSharedPtr<FJsonObject>           Item;
        FString                           IdentifierDisplay;
        TArray<FString>                   RequestedFields;
    };
    TArray<FResolved>                Resolved;
    TArray<FMcpNodeValidationError>  NodeErrors;
    Resolved.Reserve(InputsArr->Num());

    for (int32 i = 0; i < InputsArr->Num(); ++i)
    {
        FResolved R;
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!(*InputsArr)[i].IsValid() || !(*InputsArr)[i]->TryGetObject(ObjPtr) || !ObjPtr || !ObjPtr->IsValid())
        {
            FMcpNodeValidationError E;
            E.Index = i;
            E.Code = TEXT("INVALID_NODE_SPEC");
            E.Field = TEXT("inputs[]");
            E.Message = TEXT("Update spec is not an object");
            NodeErrors.Add(E);
            Resolved.Add(R);
            continue;
        }
        R.Item = *ObjPtr;

        const TSharedPtr<FJsonValue> IdentifierJson = R.Item->TryGetField(TEXT("identifier"));
        R.IdentifierDisplay = McpFormatIdentifierForDisplay(IdentifierJson);

        FMcpNodeValidationError IdErr;
        IdErr.Index = i;
        UMaterialExpression* Expr = nullptr;
        if (!McpResolveUpdateIdentifier(Owner, IdentifierJson, Expr, IdErr))
        {
            NodeErrors.Add(IdErr);
            Resolved.Add(R);
            continue;
        }

        UMaterialExpressionFunctionInput* AsInput = Cast<UMaterialExpressionFunctionInput>(Expr);
        if (!AsInput)
        {
            FMcpNodeValidationError E;
            E.Index = i;
            E.Code = TEXT("INVALID_NODE_TYPE");
            E.Field = TEXT("identifier");
            E.Message = FString::Printf(
                TEXT("update_function_inputs expected MaterialExpressionFunctionInput; got %s"),
                Expr ? *Expr->GetClass()->GetName() : TEXT("<null>"));
            NodeErrors.Add(E);
            Resolved.Add(R);
            continue;
        }
        R.Expr = AsInput;

        // inputType enum check
        FString IT;
        if (R.Item->TryGetStringField(TEXT("inputType"), IT) && !IT.IsEmpty())
        {
            if (McpParseFunctionInputType(IT) == FunctionInput_MAX)
            {
                FMcpNodeValidationError E;
                McpFillEnumError_FunctionAuthoring(i, FString(), TEXT("inputType"),
                    IT, McpAllowedFunctionInputTypes(), E);
                NodeErrors.Add(E);
                Resolved.Add(R);
                continue;
            }
        }

        // Capture requested user-facing fields
        static const TSet<FString> Meta = { TEXT("identifier") };
        for (const auto& Pair : R.Item->Values)
        {
            if (Meta.Contains(Pair.Key)) continue;
            R.RequestedFields.Add(Pair.Key);
        }

        Resolved.Add(R);
    }

    if (NodeErrors.Num() > 0)
    {
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField  (TEXT("success"), false);
        Resp->SetStringField(TEXT("errorCode"), TEXT("VALIDATION_FAILED"));
        Resp->SetStringField(TEXT("message"),
            FString::Printf(TEXT("Batch rejected; 0 mutations applied. %d node errors."),
                NodeErrors.Num()));
        TArray<TSharedPtr<FJsonValue>> ErrorsJson;
        for (const auto& E : NodeErrors) ErrorsJson.Add(McpFormatNodeError(E));
        Resp->SetArrayField(TEXT("errors"), ErrorsJson);
        Sub->SendAutomationResponse(Socket, RequestId, false,
            TEXT("validation failed"), Resp, TEXT("VALIDATION_FAILED"));
        return true;
    }

    // Phase B: apply
    FScopedTransaction Tx(NSLOCTEXT("McpAutomationBridge",
        "McpUpdateFunctionInputs", "MCP update_function_inputs"));
    McpModifyMaterialGraphOwnerForTransaction(Owner);

    TArray<TSharedPtr<FJsonObject>> ResultsJson;

    for (const FResolved& R : Resolved)
    {
        if (!R.Expr || !R.Item.IsValid()) continue;
        R.Expr->Modify();

        // Important! R.Expr->Id is preserved (never regenerated on update).
        FString S;
        if (R.Item->TryGetStringField(TEXT("inputName"), S))
        {
            R.Expr->InputName = FName(*S);
        }
        if (R.Item->TryGetStringField(TEXT("inputType"), S) && !S.IsEmpty())
        {
            R.Expr->InputType = McpParseFunctionInputType(S);
        }
        if (R.Item->TryGetStringField(TEXT("description"), S))
        {
            R.Expr->Description = S;
        }
        if (R.Item->TryGetStringField(TEXT("desc"), S))
        {
            R.Expr->Desc = S;
        }
        bool b;
        if (R.Item->TryGetBoolField(TEXT("usePreviewValueAsDefault"), b))
        {
            R.Expr->bUsePreviewValueAsDefault = b ? 1 : 0;
        }
        double Num;
        if (R.Item->TryGetNumberField(TEXT("sortPriority"), Num))
        {
            R.Expr->SortPriority = (int32)Num;
        }
        if (R.Item->TryGetNumberField(TEXT("x"), Num))
        {
            R.Expr->MaterialExpressionEditorX = (int32)Num;
        }
        if (R.Item->TryGetNumberField(TEXT("y"), Num))
        {
            R.Expr->MaterialExpressionEditorY = (int32)Num;
        }
        if (const TSharedPtr<FJsonValue> PV = R.Item->TryGetField(TEXT("previewValue")))
        {
            McpApplyPreviewValue(R.Expr, PV);
        }

        TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("identifier"),     R.IdentifierDisplay);
        Item->SetStringField(TEXT("expressionName"), R.Expr->GetName());
        Item->SetStringField(TEXT("expressionGuid"), R.Expr->MaterialExpressionGuid.ToString());
        Item->SetStringField(TEXT("functionInputId"), R.Expr->Id.ToString());
        TArray<TSharedPtr<FJsonValue>> Fields;
        for (const FString& F : R.RequestedFields) Fields.Add(MakeShared<FJsonValueString>(F));
        Item->SetArrayField(TEXT("fieldsUpdated"), Fields);
        ResultsJson.Add(Item);
    }

    FString RebuildErr;
    McpRebuildMaterialGraphOwner(Owner, RebuildErr);

    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    bool bSaved = false;
    if (bSave)
    {
        bSaved = McpSafeAssetSave(Owner.Asset);
        if (!bSaved)
        {
            Tx.Cancel();
            Sub->SendAutomationError(Socket, RequestId,
                TEXT("Save failed; transaction rolled back"), TEXT("APPLY_FAILED"));
            return true;
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField  (TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetNumberField(TEXT("inputsUpdated"), Resolved.Num());
    Resp->SetBoolField  (TEXT("saved"), bSaved);

    TArray<TSharedPtr<FJsonValue>> ResultsArr;
    for (const auto& R : ResultsJson) ResultsArr.Add(MakeShared<FJsonValueObject>(R));
    Resp->SetArrayField(TEXT("results"), ResultsArr);

    Sub->SendAutomationResponse(Socket, RequestId, true,
        TEXT("update_function_inputs succeeded"), Resp, FString());
    return true;
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("update_function_inputs requires editor build"), TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

extern bool McpHandle_UpdateFunctionOutputs(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool McpHandle_UpdateFunctionOutputs(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    if (!Sub || !Payload.IsValid())
    {
        if (Sub) Sub->SendAutomationError(Socket, RequestId,
            TEXT("Invalid payload"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("assetPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (AssetPath.StartsWith(TEXT("/Engine/")) || AssetPath.StartsWith(TEXT("/EnginePlugins/")))
    {
        Sub->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Asset path '%s' is under engine content. Copy to /Game first."), *AssetPath),
            TEXT("ENGINE_ASSET_BLOCKED"));
        return true;
    }

    FMcpMaterialGraphOwner Owner;
    FString OwnerErr;
    if (!McpResolveMaterialGraphOwner(AssetPath, Owner, OwnerErr) || Owner.bReadOnly)
    {
        const FString Code = OwnerErr.Contains(TEXT("not found"))
            ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE");
        Sub->SendAutomationError(Socket, RequestId,
            OwnerErr.IsEmpty() ? TEXT("Cannot mutate this asset") : OwnerErr, Code);
        return true;
    }

    if (!McpRequireFunctionOwner(Sub, RequestId, Socket, Owner, TEXT("update_function_outputs")))
    {
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* OutputsArr = nullptr;
    if (!Payload->TryGetArrayField(TEXT("updates"), OutputsArr) || !OutputsArr || OutputsArr->Num() == 0)
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("updates[] is required, minimum length 1"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    struct FResolved
    {
        UMaterialExpressionFunctionOutput* Expr = nullptr;
        TSharedPtr<FJsonObject>            Item;
        FString                            IdentifierDisplay;
        TArray<FString>                    RequestedFields;
    };
    TArray<FResolved>                Resolved;
    TArray<FMcpNodeValidationError>  NodeErrors;
    Resolved.Reserve(OutputsArr->Num());

    for (int32 i = 0; i < OutputsArr->Num(); ++i)
    {
        FResolved R;
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!(*OutputsArr)[i].IsValid() || !(*OutputsArr)[i]->TryGetObject(ObjPtr) || !ObjPtr || !ObjPtr->IsValid())
        {
            FMcpNodeValidationError E;
            E.Index = i;
            E.Code = TEXT("INVALID_NODE_SPEC");
            E.Field = TEXT("outputs[]");
            E.Message = TEXT("Update spec is not an object");
            NodeErrors.Add(E);
            Resolved.Add(R);
            continue;
        }
        R.Item = *ObjPtr;

        const TSharedPtr<FJsonValue> IdentifierJson = R.Item->TryGetField(TEXT("identifier"));
        R.IdentifierDisplay = McpFormatIdentifierForDisplay(IdentifierJson);

        FMcpNodeValidationError IdErr;
        IdErr.Index = i;
        UMaterialExpression* Expr = nullptr;
        if (!McpResolveUpdateIdentifier(Owner, IdentifierJson, Expr, IdErr))
        {
            NodeErrors.Add(IdErr);
            Resolved.Add(R);
            continue;
        }

        UMaterialExpressionFunctionOutput* AsOut = Cast<UMaterialExpressionFunctionOutput>(Expr);
        if (!AsOut)
        {
            FMcpNodeValidationError E;
            E.Index = i;
            E.Code = TEXT("INVALID_NODE_TYPE");
            E.Field = TEXT("identifier");
            E.Message = FString::Printf(
                TEXT("update_function_outputs expected MaterialExpressionFunctionOutput; got %s"),
                Expr ? *Expr->GetClass()->GetName() : TEXT("<null>"));
            NodeErrors.Add(E);
            Resolved.Add(R);
            continue;
        }
        R.Expr = AsOut;

        static const TSet<FString> Meta = { TEXT("identifier") };
        for (const auto& Pair : R.Item->Values)
        {
            if (Meta.Contains(Pair.Key)) continue;
            R.RequestedFields.Add(Pair.Key);
        }

        Resolved.Add(R);
    }

    if (NodeErrors.Num() > 0)
    {
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField  (TEXT("success"), false);
        Resp->SetStringField(TEXT("errorCode"), TEXT("VALIDATION_FAILED"));
        Resp->SetStringField(TEXT("message"),
            FString::Printf(TEXT("Batch rejected; 0 mutations applied. %d node errors."),
                NodeErrors.Num()));
        TArray<TSharedPtr<FJsonValue>> ErrorsJson;
        for (const auto& E : NodeErrors) ErrorsJson.Add(McpFormatNodeError(E));
        Resp->SetArrayField(TEXT("errors"), ErrorsJson);
        Sub->SendAutomationResponse(Socket, RequestId, false,
            TEXT("validation failed"), Resp, TEXT("VALIDATION_FAILED"));
        return true;
    }

    FScopedTransaction Tx(NSLOCTEXT("McpAutomationBridge",
        "McpUpdateFunctionOutputs", "MCP update_function_outputs"));
    McpModifyMaterialGraphOwnerForTransaction(Owner);

    TArray<TSharedPtr<FJsonObject>> ResultsJson;

    for (const FResolved& R : Resolved)
    {
        if (!R.Expr || !R.Item.IsValid()) continue;
        R.Expr->Modify();

        // Important! R.Expr->Id is preserved.
        FString S;
        if (R.Item->TryGetStringField(TEXT("outputName"), S))
        {
            R.Expr->OutputName = FName(*S);
        }
        if (R.Item->TryGetStringField(TEXT("description"), S))
        {
            R.Expr->Description = S;
        }
        if (R.Item->TryGetStringField(TEXT("desc"), S))
        {
            R.Expr->Desc = S;
        }
        double Num;
        if (R.Item->TryGetNumberField(TEXT("sortPriority"), Num))
        {
            R.Expr->SortPriority = (int32)Num;
        }
        if (R.Item->TryGetNumberField(TEXT("x"), Num))
        {
            R.Expr->MaterialExpressionEditorX = (int32)Num;
        }
        if (R.Item->TryGetNumberField(TEXT("y"), Num))
        {
            R.Expr->MaterialExpressionEditorY = (int32)Num;
        }
        // outputType is silently ignored (no field on UMaterialExpressionFunctionOutput).

        TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("identifier"),      R.IdentifierDisplay);
        Item->SetStringField(TEXT("expressionName"),  R.Expr->GetName());
        Item->SetStringField(TEXT("expressionGuid"),  R.Expr->MaterialExpressionGuid.ToString());
        Item->SetStringField(TEXT("functionOutputId"), R.Expr->Id.ToString());
        TArray<TSharedPtr<FJsonValue>> Fields;
        for (const FString& F : R.RequestedFields) Fields.Add(MakeShared<FJsonValueString>(F));
        Item->SetArrayField(TEXT("fieldsUpdated"), Fields);
        ResultsJson.Add(Item);
    }

    FString RebuildErr;
    McpRebuildMaterialGraphOwner(Owner, RebuildErr);

    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    bool bSaved = false;
    if (bSave)
    {
        bSaved = McpSafeAssetSave(Owner.Asset);
        if (!bSaved)
        {
            Tx.Cancel();
            Sub->SendAutomationError(Socket, RequestId,
                TEXT("Save failed; transaction rolled back"), TEXT("APPLY_FAILED"));
            return true;
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField  (TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetNumberField(TEXT("outputsUpdated"), Resolved.Num());
    Resp->SetBoolField  (TEXT("saved"), bSaved);

    TArray<TSharedPtr<FJsonValue>> ResultsArr;
    for (const auto& R : ResultsJson) ResultsArr.Add(MakeShared<FJsonValueObject>(R));
    Resp->SetArrayField(TEXT("results"), ResultsArr);

    Sub->SendAutomationResponse(Socket, RequestId, true,
        TEXT("update_function_outputs succeeded"), Resp, FString());
    return true;
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("update_function_outputs requires editor build"), TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =============================================================================
// add_material_function_calls (Task C.6)
//
// Adds UMaterialExpressionMaterialFunctionCall nodes pointing at a
// UMaterialFunctionInterface. Owner can be UMaterial OR UMaterialFunction.
// =============================================================================
extern bool McpHandle_AddMaterialFunctionCalls(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool McpHandle_AddMaterialFunctionCalls(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    if (!Sub || !Payload.IsValid())
    {
        if (Sub) Sub->SendAutomationError(Socket, RequestId,
            TEXT("Invalid payload"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("assetPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (AssetPath.StartsWith(TEXT("/Engine/")) || AssetPath.StartsWith(TEXT("/EnginePlugins/")))
    {
        Sub->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Asset path '%s' is under engine content. Copy to /Game first."), *AssetPath),
            TEXT("ENGINE_ASSET_BLOCKED"));
        return true;
    }

    FMcpMaterialGraphOwner Owner;
    FString OwnerErr;
    if (!McpResolveMaterialGraphOwner(AssetPath, Owner, OwnerErr) || Owner.bReadOnly)
    {
        const FString Code = OwnerErr.Contains(TEXT("not found"))
            ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE");
        Sub->SendAutomationError(Socket, RequestId,
            OwnerErr.IsEmpty() ? TEXT("Cannot mutate this asset") : OwnerErr, Code);
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* CallsArr = nullptr;
    if (!Payload->TryGetArrayField(TEXT("calls"), CallsArr) || !CallsArr || CallsArr->Num() == 0)
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("calls[] is required, minimum length 1"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Phase A: validate every item; resolve every functionPath up front.
    TSet<FString>                    SeenLocalIds;
    TArray<FMcpNodeValidationError>  NodeErrors;
    TArray<UMaterialFunctionInterface*> ResolvedFunctions;
    ResolvedFunctions.SetNum(CallsArr->Num());

    for (int32 i = 0; i < CallsArr->Num(); ++i)
    {
        FMcpNodeValidationError E;
        E.Index = i;

        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!(*CallsArr)[i].IsValid() || !(*CallsArr)[i]->TryGetObject(ObjPtr) || !ObjPtr || !ObjPtr->IsValid())
        {
            E.Code = TEXT("INVALID_NODE_SPEC");
            E.Field = TEXT("calls[]");
            E.Message = TEXT("Item is not an object");
            NodeErrors.Add(E);
            continue;
        }
        const TSharedPtr<FJsonObject>& Item = *ObjPtr;

        FString LocalId;
        Item->TryGetStringField(TEXT("localId"), LocalId);
        E.LocalId = LocalId;
        if (!LocalId.IsEmpty())
        {
            if (SeenLocalIds.Contains(LocalId))
            {
                E.Field = TEXT("localId");
                E.Code = TEXT("LOCAL_ID_DUPLICATE");
                E.Message = FString::Printf(
                    TEXT("localId '%s' duplicates an earlier item in this batch"), *LocalId);
                NodeErrors.Add(E);
                continue;
            }
            SeenLocalIds.Add(LocalId);
        }

        FString FunctionPath;
        if (!Item->TryGetStringField(TEXT("functionPath"), FunctionPath) || FunctionPath.IsEmpty())
        {
            E.Field = TEXT("functionPath");
            E.Code = TEXT("INVALID_ARGUMENT");
            E.Message = TEXT("'functionPath' is required and must be a non-empty string");
            NodeErrors.Add(E);
            continue;
        }

        UMaterialFunctionInterface* Loaded =
            LoadObject<UMaterialFunctionInterface>(nullptr, *FunctionPath);
        if (!Loaded)
        {
            E.Field = TEXT("functionPath");
            E.Code = TEXT("FUNCTION_NOT_FOUND");
            E.Message = FString::Printf(
                TEXT("Material function '%s' not found"), *FunctionPath);
            NodeErrors.Add(E);
            continue;
        }
        ResolvedFunctions[i] = Loaded;
    }

    if (NodeErrors.Num() > 0)
    {
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField  (TEXT("success"), false);
        Resp->SetStringField(TEXT("errorCode"), TEXT("VALIDATION_FAILED"));
        Resp->SetStringField(TEXT("message"),
            FString::Printf(TEXT("Batch rejected; 0 mutations applied. %d node errors."),
                NodeErrors.Num()));
        TArray<TSharedPtr<FJsonValue>> ErrorsJson;
        for (const auto& E : NodeErrors) ErrorsJson.Add(McpFormatNodeError(E));
        Resp->SetArrayField(TEXT("errors"), ErrorsJson);
        Sub->SendAutomationResponse(Socket, RequestId, false,
            TEXT("validation failed"), Resp, TEXT("VALIDATION_FAILED"));
        return true;
    }

    // Phase B: apply
    FScopedTransaction Tx(NSLOCTEXT("McpAutomationBridge",
        "McpAddMaterialFunctionCalls", "MCP add_material_function_calls"));
    McpModifyMaterialGraphOwnerForTransaction(Owner);

    UObject* MaterialOuter = Owner.GraphSource ? Owner.GraphSource : Owner.Asset;
    TArray<TSharedPtr<FJsonObject>> Mappings;

    for (int32 i = 0; i < CallsArr->Num(); ++i)
    {
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!(*CallsArr)[i].IsValid() || !(*CallsArr)[i]->TryGetObject(ObjPtr) || !ObjPtr || !ObjPtr->IsValid())
        {
            continue;
        }
        const TSharedPtr<FJsonObject>& Item = *ObjPtr;
        UMaterialFunctionInterface* Func = ResolvedFunctions[i];
        if (!Func) continue;

        UMaterialExpressionMaterialFunctionCall* Call =
            NewObject<UMaterialExpressionMaterialFunctionCall>(
                MaterialOuter,
                UMaterialExpressionMaterialFunctionCall::StaticClass(),
                NAME_None, RF_Transactional);
        if (!Call) continue;

        // SetMaterialFunction populates FunctionInputs[]/FunctionOutputs[] from
        // the loaded interface (mirrors the editor's "drop function" path).
        Call->SetMaterialFunction(Func);

        FString Desc;
        if (Item->TryGetStringField(TEXT("desc"), Desc))
        {
            Call->Desc = Desc;
        }

        double X = 0, Y = 0;
        Item->TryGetNumberField(TEXT("x"), X);
        Item->TryGetNumberField(TEXT("y"), Y);
        Call->MaterialExpressionEditorX = (int32)X;
        Call->MaterialExpressionEditorY = (int32)Y;
        Call->MaterialExpressionGuid = FGuid::NewGuid();

        TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressionsMutable(Owner);
        if (Exprs) Exprs->Add(Call);

        FString LocalId;
        Item->TryGetStringField(TEXT("localId"), LocalId);

        TSharedPtr<FJsonObject> M = MakeShared<FJsonObject>();
        M->SetStringField(TEXT("localId"),         LocalId);
        M->SetStringField(TEXT("nodeId"),          Call->GetName());
        M->SetStringField(TEXT("expressionGuid"),  Call->MaterialExpressionGuid.ToString());
        M->SetNumberField(TEXT("expressionIndex"), Exprs ? (Exprs->Num() - 1) : 0);
        M->SetStringField(TEXT("functionPath"),    Func->GetPathName());
        M->SetStringField(TEXT("functionName"),    Func->GetName());
        Mappings.Add(M);
    }

    FString RebuildErr;
    McpRebuildMaterialGraphOwner(Owner, RebuildErr);

    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    bool bSaved = false;
    if (bSave)
    {
        bSaved = McpSafeAssetSave(Owner.Asset);
        if (!bSaved)
        {
            Tx.Cancel();
            Sub->SendAutomationError(Socket, RequestId,
                TEXT("Save failed; transaction rolled back"), TEXT("APPLY_FAILED"));
            return true;
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField  (TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetNumberField(TEXT("callsCreated"), CallsArr->Num());
    Resp->SetBoolField  (TEXT("saved"), bSaved);

    TArray<TSharedPtr<FJsonValue>> MappingsJson;
    for (const auto& M : Mappings) MappingsJson.Add(MakeShared<FJsonValueObject>(M));
    Resp->SetArrayField(TEXT("mappings"), MappingsJson);

    Sub->SendAutomationResponse(Socket, RequestId, true,
        TEXT("add_material_function_calls succeeded"), Resp, FString());
    return true;
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("add_material_function_calls requires editor build"), TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =============================================================================
// update_material_function_calls (Task C.6)
//
// Per-update: { identifier, functionPath?, onPinRemoved?, x?, y?, desc? }
// On re-bind: load new function, diff pin layout vs old function. Match by Id
// (GUID) where possible, else by name. Removed pins with live wires are
// handled per onPinRemoved (default: "break", per spec §7.11 - this differs
// from update_custom_expressions where the default is "preserve").
// =============================================================================
extern bool McpHandle_UpdateMaterialFunctionCalls(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

namespace
{
#if WITH_EDITOR

// Snapshot of a function's pin list, captured before SetMaterialFunction()
// rebuilds FunctionInputs[]/FunctionOutputs[]. Used to diff against the new
// layout so we can find pins that disappeared.
struct FMcpFuncPinSnapshot
{
    FGuid   Id;
    FName   Name;
    int32   Index = INDEX_NONE; // index in FunctionInputs[] or FunctionOutputs[]
};

// Captures pin layout from a function call (pre-rebind state).
static void McpSnapshotFunctionCallPins(
    UMaterialExpressionMaterialFunctionCall* Call,
    TArray<FMcpFuncPinSnapshot>& OutInputs,
    TArray<FMcpFuncPinSnapshot>& OutOutputs)
{
    OutInputs.Reset();
    OutOutputs.Reset();
    if (!Call) return;
    for (int32 i = 0; i < Call->FunctionInputs.Num(); ++i)
    {
        const FFunctionExpressionInput& In = Call->FunctionInputs[i];
        FMcpFuncPinSnapshot S;
        S.Id    = In.ExpressionInputId;
        S.Name  = In.ExpressionInput ? In.ExpressionInput->InputName : NAME_None;
        S.Index = i;
        OutInputs.Add(S);
    }
    for (int32 i = 0; i < Call->FunctionOutputs.Num(); ++i)
    {
        const FFunctionExpressionOutput& Out = Call->FunctionOutputs[i];
        FMcpFuncPinSnapshot S;
        S.Id    = Out.ExpressionOutputId;
        S.Name  = Out.ExpressionOutput ? Out.ExpressionOutput->OutputName : NAME_None;
        S.Index = i;
        OutOutputs.Add(S);
    }
}

// Builds a {Id -> match status} for old-vs-new pin layouts. A pin is
// considered "kept" when there is a new pin with the same Id, or (fallback)
// the same non-None name.
static bool McpPinKeptInNewLayout(
    const FMcpFuncPinSnapshot& Old,
    const TArray<FMcpFuncPinSnapshot>& New)
{
    if (Old.Id.IsValid())
    {
        for (const FMcpFuncPinSnapshot& N : New)
        {
            if (N.Id == Old.Id) return true;
        }
    }
    if (!Old.Name.IsNone())
    {
        for (const FMcpFuncPinSnapshot& N : New)
        {
            if (N.Name == Old.Name) return true;
        }
    }
    return false;
}

// Per-item broken connection record.
struct FMcpFuncCallBroken
{
    FString IdentifierDisplay;
    FString PinName;
    FString Direction;   // "incoming" or "outgoing"
    FString OtherNode;
    FString OtherPin;

    UMaterialExpressionMaterialFunctionCall* OurCall = nullptr;
    int32                                    OurInputIndex = INDEX_NONE;
    int32                                    OurOutputIndex = INDEX_NONE;
};

// Per-item resolved update record.
struct FMcpFuncCallUpdate
{
    int32                                    Index = INDEX_NONE;
    UMaterialExpressionMaterialFunctionCall* Call = nullptr;
    TSharedPtr<FJsonObject>                  Item;
    FString                                  IdentifierDisplay;
    UMaterialFunctionInterface*              NewFunction = nullptr; // null if no re-bind
    bool                                     bBreak = true;         // default per spec
    TArray<FString>                          RequestedFields;
    TArray<FMcpFuncCallBroken>               Broken;
};

// Walks the graph and finds every FExpressionInput on every other expression
// whose Expression == OurCall and OutputIndex == RemovedOutputIndex. Returns
// outgoing-broken-connection descriptors.
static void McpCollectOutgoingForRemovedFuncOutput(
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpressionMaterialFunctionCall* OurCall,
    int32 RemovedOutputIndex,
    const FString& RemovedOutputName,
    const FString& IdentifierDisplay,
    TArray<FMcpFuncCallBroken>& OutBroken)
{
    const TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressions(Owner);
    if (!Exprs) return;

    for (UMaterialExpression* Other : *Exprs)
    {
        if (!Other || Other == OurCall) continue;
        for (FExpressionInputIterator It(Other); It; ++It)
        {
            if (!It.Input) continue;
            if (It->Expression == OurCall && It->OutputIndex == RemovedOutputIndex)
            {
                FMcpFuncCallBroken B;
                B.IdentifierDisplay = IdentifierDisplay;
                B.PinName = RemovedOutputName;
                B.Direction = TEXT("outgoing");
                B.OtherNode = Other->GetName();
                B.OtherPin = Other->GetInputName(It.Index).ToString();
                B.OurCall = OurCall;
                B.OurOutputIndex = RemovedOutputIndex;
                OutBroken.Add(B);
            }
        }
    }
}

#endif // WITH_EDITOR
} // namespace

bool McpHandle_UpdateMaterialFunctionCalls(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    if (!Sub || !Payload.IsValid())
    {
        if (Sub) Sub->SendAutomationError(Socket, RequestId,
            TEXT("Invalid payload"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("assetPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (AssetPath.StartsWith(TEXT("/Engine/")) || AssetPath.StartsWith(TEXT("/EnginePlugins/")))
    {
        Sub->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Asset path '%s' is under engine content. Copy to /Game first."), *AssetPath),
            TEXT("ENGINE_ASSET_BLOCKED"));
        return true;
    }

    FMcpMaterialGraphOwner Owner;
    FString OwnerErr;
    if (!McpResolveMaterialGraphOwner(AssetPath, Owner, OwnerErr) || Owner.bReadOnly)
    {
        const FString Code = OwnerErr.Contains(TEXT("not found"))
            ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE");
        Sub->SendAutomationError(Socket, RequestId,
            OwnerErr.IsEmpty() ? TEXT("Cannot mutate this asset") : OwnerErr, Code);
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* CallsArr = nullptr;
    if (!Payload->TryGetArrayField(TEXT("updates"), CallsArr) || !CallsArr || CallsArr->Num() == 0)
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("updates[] is required, minimum length 1"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Phase A: resolve each identifier, validate body, capture broken
    // connection list when a re-bind would drop pins with live wires.
    TArray<FMcpFuncCallUpdate>      Resolved;
    TArray<FMcpNodeValidationError> NodeErrors;
    Resolved.Reserve(CallsArr->Num());

    for (int32 i = 0; i < CallsArr->Num(); ++i)
    {
        FMcpFuncCallUpdate U;
        U.Index = i;

        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!(*CallsArr)[i].IsValid() || !(*CallsArr)[i]->TryGetObject(ObjPtr) || !ObjPtr || !ObjPtr->IsValid())
        {
            FMcpNodeValidationError E;
            E.Index = i;
            E.Code = TEXT("INVALID_NODE_SPEC");
            E.Field = TEXT("calls[]");
            E.Message = TEXT("Update spec is not an object");
            NodeErrors.Add(E);
            Resolved.Add(U);
            continue;
        }
        U.Item = *ObjPtr;

        const TSharedPtr<FJsonValue> IdentifierJson = U.Item->TryGetField(TEXT("identifier"));
        U.IdentifierDisplay = McpFormatIdentifierForDisplay(IdentifierJson);

        FMcpNodeValidationError IdErr;
        IdErr.Index = i;
        UMaterialExpression* Expr = nullptr;
        if (!McpResolveUpdateIdentifier(Owner, IdentifierJson, Expr, IdErr))
        {
            NodeErrors.Add(IdErr);
            Resolved.Add(U);
            continue;
        }

        UMaterialExpressionMaterialFunctionCall* AsCall =
            Cast<UMaterialExpressionMaterialFunctionCall>(Expr);
        if (!AsCall)
        {
            FMcpNodeValidationError E;
            E.Index = i;
            // Spec §7.11 specifies WRONG_NODE_TYPE here (intentional divergence
            // from update_material_nodes / update_custom_expressions, which use
            // INVALID_NODE_TYPE).
            E.Code = TEXT("WRONG_NODE_TYPE");
            E.Field = TEXT("identifier");
            E.Message = FString::Printf(
                TEXT("update_material_function_calls expected MaterialExpressionMaterialFunctionCall; got %s"),
                Expr ? *Expr->GetClass()->GetName() : TEXT("<null>"));
            NodeErrors.Add(E);
            Resolved.Add(U);
            continue;
        }
        U.Call = AsCall;

        // onPinRemoved: default "break" per spec §7.11
        FString PinPolicy = TEXT("break");
        U.Item->TryGetStringField(TEXT("onPinRemoved"), PinPolicy);
        if (!PinPolicy.Equals(TEXT("preserve")) && !PinPolicy.Equals(TEXT("break")))
        {
            FMcpNodeValidationError E;
            McpFillEnumError_FunctionAuthoring(i, FString(), TEXT("onPinRemoved"),
                PinPolicy, TArray<FString>{ TEXT("preserve"), TEXT("break") }, E);
            NodeErrors.Add(E);
            Resolved.Add(U);
            continue;
        }
        U.bBreak = PinPolicy.Equals(TEXT("break"));

        // Optional re-bind
        FString FunctionPath;
        const bool bHasNewFunc = U.Item->TryGetStringField(TEXT("functionPath"), FunctionPath)
            && !FunctionPath.IsEmpty();
        if (bHasNewFunc)
        {
            UMaterialFunctionInterface* NewFunc =
                LoadObject<UMaterialFunctionInterface>(nullptr, *FunctionPath);
            if (!NewFunc)
            {
                FMcpNodeValidationError E;
                E.Index = i;
                E.Field = TEXT("functionPath");
                E.Code = TEXT("FUNCTION_NOT_FOUND");
                E.Message = FString::Printf(
                    TEXT("Material function '%s' not found"), *FunctionPath);
                NodeErrors.Add(E);
                Resolved.Add(U);
                continue;
            }
            U.NewFunction = NewFunc;

            // Snapshot current pin layout. Then probe what the new function
            // would expose by spinning a transient call to query its layout
            // without disturbing the live one - simpler: ask the function
            // interface directly.
            TArray<FMcpFuncPinSnapshot> OldInputs, OldOutputs;
            McpSnapshotFunctionCallPins(AsCall, OldInputs, OldOutputs);

            TArray<FFunctionExpressionInput>  NewFuncInputs;
            TArray<FFunctionExpressionOutput> NewFuncOutputs;
            NewFunc->GetInputsAndOutputs(NewFuncInputs, NewFuncOutputs);

            TArray<FMcpFuncPinSnapshot> NewInputs;
            for (int32 k = 0; k < NewFuncInputs.Num(); ++k)
            {
                FMcpFuncPinSnapshot S;
                S.Id    = NewFuncInputs[k].ExpressionInputId;
                S.Name  = NewFuncInputs[k].ExpressionInput
                            ? NewFuncInputs[k].ExpressionInput->InputName : NAME_None;
                S.Index = k;
                NewInputs.Add(S);
            }
            TArray<FMcpFuncPinSnapshot> NewOutputs;
            for (int32 k = 0; k < NewFuncOutputs.Num(); ++k)
            {
                FMcpFuncPinSnapshot S;
                S.Id    = NewFuncOutputs[k].ExpressionOutputId;
                S.Name  = NewFuncOutputs[k].ExpressionOutput
                            ? NewFuncOutputs[k].ExpressionOutput->OutputName : NAME_None;
                S.Index = k;
                NewOutputs.Add(S);
            }

            // Removed input pins -> live incoming connection
            bool bAborted = false;
            for (const FMcpFuncPinSnapshot& OldIn : OldInputs)
            {
                if (McpPinKeptInNewLayout(OldIn, NewInputs)) continue;

                if (!AsCall->FunctionInputs.IsValidIndex(OldIn.Index)) continue;
                const FExpressionInput& In = AsCall->FunctionInputs[OldIn.Index].Input;
                if (In.Expression == nullptr) continue;

                if (!U.bBreak)
                {
                    FMcpNodeValidationError E;
                    E.Index = i;
                    E.Field = FString::Printf(TEXT("functionInputs[].%s"), *OldIn.Name.ToString());
                    E.Code = TEXT("LIVE_CONNECTIONS_ON_REMOVED_PIN");
                    E.Message = FString::Printf(
                        TEXT("Re-bind would remove input pin '%s' which has a live incoming connection from '%s'; pass onPinRemoved='break' to nullify"),
                        *OldIn.Name.ToString(),
                        *In.Expression->GetName());
                    NodeErrors.Add(E);
                    bAborted = true;
                    break;
                }

                FMcpFuncCallBroken B;
                B.IdentifierDisplay = U.IdentifierDisplay;
                B.PinName = OldIn.Name.ToString();
                B.Direction = TEXT("incoming");
                B.OtherNode = In.Expression->GetName();
                const TArray<FExpressionOutput>& Outs = In.Expression->GetOutputs();
                if (Outs.IsValidIndex(In.OutputIndex))
                {
                    const FExpressionOutput& O = Outs[In.OutputIndex];
                    if (!O.OutputName.IsNone()) B.OtherPin = O.OutputName.ToString();
                }
                B.OurCall = AsCall;
                B.OurInputIndex = OldIn.Index;
                U.Broken.Add(B);
            }
            if (bAborted) { Resolved.Add(U); continue; }

            // Removed output pins -> live outgoing connections
            for (const FMcpFuncPinSnapshot& OldOut : OldOutputs)
            {
                if (McpPinKeptInNewLayout(OldOut, NewOutputs)) continue;

                TArray<FMcpFuncCallBroken> Outgoing;
                McpCollectOutgoingForRemovedFuncOutput(Owner, AsCall,
                    OldOut.Index, OldOut.Name.ToString(), U.IdentifierDisplay, Outgoing);
                if (Outgoing.Num() == 0) continue;

                if (!U.bBreak)
                {
                    FMcpNodeValidationError E;
                    E.Index = i;
                    E.Field = FString::Printf(TEXT("functionOutputs[].%s"), *OldOut.Name.ToString());
                    E.Code = TEXT("LIVE_CONNECTIONS_ON_REMOVED_PIN");
                    E.Message = FString::Printf(
                        TEXT("Re-bind would remove output pin '%s' with %d live outgoing connection(s); pass onPinRemoved='break' to nullify"),
                        *OldOut.Name.ToString(), Outgoing.Num());
                    NodeErrors.Add(E);
                    bAborted = true;
                    break;
                }
                U.Broken.Append(Outgoing);
            }
            if (bAborted) { Resolved.Add(U); continue; }
        }

        // Capture requested user-facing fields
        static const TSet<FString> Meta = {
            TEXT("identifier"), TEXT("onPinRemoved")
        };
        for (const auto& Pair : U.Item->Values)
        {
            if (Meta.Contains(Pair.Key)) continue;
            U.RequestedFields.Add(Pair.Key);
        }

        Resolved.Add(U);
    }

    if (NodeErrors.Num() > 0)
    {
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField  (TEXT("success"), false);
        Resp->SetStringField(TEXT("errorCode"), TEXT("VALIDATION_FAILED"));
        Resp->SetStringField(TEXT("message"),
            FString::Printf(TEXT("Batch rejected; 0 mutations applied. %d node errors."),
                NodeErrors.Num()));
        TArray<TSharedPtr<FJsonValue>> ErrorsJson;
        for (const auto& E : NodeErrors) ErrorsJson.Add(McpFormatNodeError(E));
        Resp->SetArrayField(TEXT("errors"), ErrorsJson);
        Sub->SendAutomationResponse(Socket, RequestId, false,
            TEXT("validation failed"), Resp, TEXT("VALIDATION_FAILED"));
        return true;
    }

    // Phase B: apply
    FScopedTransaction Tx(NSLOCTEXT("McpAutomationBridge",
        "McpUpdateMaterialFunctionCalls", "MCP update_material_function_calls"));
    McpModifyMaterialGraphOwnerForTransaction(Owner);

    TArray<TSharedPtr<FJsonObject>> ResultsJson;
    TArray<TSharedPtr<FJsonObject>> BrokenJson;

    for (const FMcpFuncCallUpdate& U : Resolved)
    {
        if (!U.Call || !U.Item.IsValid()) continue;
        U.Call->Modify();

        // Apply broken-connection nullifications first (the input wires sit
        // inside FunctionInputs[].Input which SetMaterialFunction will rebuild,
        // so nullify them on incoming-direction by walking via OldInputIndex
        // BEFORE the rebuild. Outgoing-direction wires live on OTHER expressions
        // and are unaffected by the rebuild, so we can nullify them either
        // before or after.
        for (const FMcpFuncCallBroken& B : U.Broken)
        {
            if (B.Direction == TEXT("incoming"))
            {
                if (U.Call->FunctionInputs.IsValidIndex(B.OurInputIndex))
                {
                    FExpressionInput& In = U.Call->FunctionInputs[B.OurInputIndex].Input;
                    In.Expression = nullptr;
                    In.OutputIndex = 0;
                }
            }
            else
            {
                const TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressions(Owner);
                if (!Exprs) continue;
                for (UMaterialExpression* Other : *Exprs)
                {
                    if (!Other || Other == U.Call) continue;
                    if (Other->GetName() != B.OtherNode) continue;
                    Other->Modify();
                    for (FExpressionInputIterator It(Other); It; ++It)
                    {
                        if (!It.Input) continue;
                        if (It->Expression == U.Call && It->OutputIndex == B.OurOutputIndex)
                        {
                            It->Expression = nullptr;
                            It->OutputIndex = 0;
                        }
                    }
                }
            }

            TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
            O->SetStringField(TEXT("identifier"), B.IdentifierDisplay);
            O->SetStringField(TEXT("pinName"),    B.PinName);
            O->SetStringField(TEXT("direction"),  B.Direction);
            O->SetStringField(TEXT("otherNode"),  B.OtherNode);
            O->SetStringField(TEXT("otherPin"),   B.OtherPin);
            BrokenJson.Add(O);
        }

        // Re-bind via SetMaterialFunctionEx so UE preserves wires by name where
        // possible; we already nullified the wires that the diff said would be
        // dropped, so the SetMaterialFunction* result is consistent with our
        // connectionsBroken[] report.
        if (U.NewFunction)
        {
            UMaterialFunctionInterface* OldFunc = U.Call->MaterialFunction;
            U.Call->SetMaterialFunctionEx(OldFunc, U.NewFunction);
        }

        FString S;
        if (U.Item->TryGetStringField(TEXT("desc"), S))
        {
            U.Call->Desc = S;
        }
        double Num;
        if (U.Item->TryGetNumberField(TEXT("x"), Num))
        {
            U.Call->MaterialExpressionEditorX = (int32)Num;
        }
        if (U.Item->TryGetNumberField(TEXT("y"), Num))
        {
            U.Call->MaterialExpressionEditorY = (int32)Num;
        }

        TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("identifier"),     U.IdentifierDisplay);
        Item->SetStringField(TEXT("expressionName"), U.Call->GetName());
        Item->SetStringField(TEXT("expressionGuid"), U.Call->MaterialExpressionGuid.ToString());
        if (U.Call->MaterialFunction)
        {
            Item->SetStringField(TEXT("functionPath"), U.Call->MaterialFunction->GetPathName());
            Item->SetStringField(TEXT("functionName"), U.Call->MaterialFunction->GetName());
        }
        TArray<TSharedPtr<FJsonValue>> Fields;
        for (const FString& F : U.RequestedFields) Fields.Add(MakeShared<FJsonValueString>(F));
        Item->SetArrayField(TEXT("fieldsUpdated"), Fields);
        ResultsJson.Add(Item);
    }

    FString RebuildErr;
    McpRebuildMaterialGraphOwner(Owner, RebuildErr);

    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    bool bSaved = false;
    if (bSave)
    {
        bSaved = McpSafeAssetSave(Owner.Asset);
        if (!bSaved)
        {
            Tx.Cancel();
            Sub->SendAutomationError(Socket, RequestId,
                TEXT("Save failed; transaction rolled back"), TEXT("APPLY_FAILED"));
            return true;
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField  (TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetNumberField(TEXT("callsUpdated"), Resolved.Num());
    Resp->SetBoolField  (TEXT("saved"), bSaved);

    TArray<TSharedPtr<FJsonValue>> ResultsArr;
    for (const auto& R : ResultsJson) ResultsArr.Add(MakeShared<FJsonValueObject>(R));
    Resp->SetArrayField(TEXT("results"), ResultsArr);

    TArray<TSharedPtr<FJsonValue>> BrokenArr;
    for (const auto& B : BrokenJson) BrokenArr.Add(MakeShared<FJsonValueObject>(B));
    Resp->SetArrayField(TEXT("connectionsBroken"), BrokenArr);

    Sub->SendAutomationResponse(Socket, RequestId, true,
        TEXT("update_material_function_calls succeeded"), Resp, FString());
    return true;
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("update_material_function_calls requires editor build"), TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =============================================================================
// Test-only forwarders for unit tests of validation helpers.
// =============================================================================
namespace McpFunctionAuthoringForTests
{
#if WITH_EDITOR
    EFunctionInputType ParseFunctionInputType(const FString& S)
    {
        return McpParseFunctionInputType(S);
    }

    void ApplyPreviewValue(
        UMaterialExpressionFunctionInput* Expr,
        const TSharedPtr<FJsonValue>& PreviewVal)
    {
        McpApplyPreviewValue(Expr, PreviewVal);
    }
#endif
}
