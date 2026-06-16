// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_Material_CustomExpressions.cpp
//
// Task C.5 - add_custom_expressions and update_custom_expressions transactional
// batch handlers for UMaterialExpressionCustom nodes.
//
// Plan: docs/superpowers/plans/2026-05-07-mcp-material-tools-redesign.md
// Spec: docs/superpowers/specs/2026-05-07-mcp-material-tools-redesign-design.md
//
// Shared types/helpers come from McpAutomationBridge_Material_Internal.h
// (extracted from Material_GraphWrites.cpp during this task).

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridge_Material_Internal.h"

#if WITH_EDITOR

#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "MaterialExpressionIO.h"
#include "MaterialValueType.h"
#include "ScopedTransaction.h"

#endif // WITH_EDITOR

DEFINE_LOG_CATEGORY_STATIC(LogMcpCustomExpr, Log, All);

namespace
{
#if WITH_EDITOR

using McpMaterialInternal::FMcpNodeValidationError;
using McpMaterialInternal::McpFormatNodeError;
using McpMaterialInternal::McpFormatIdentifierForDisplay;
using McpMaterialInternal::McpResolveUpdateIdentifier;

// Allowed values for the primary outputType / additional outputs[].outputType.
// MaterialAttributes is only valid for the primary outputType (UE's FCustomOutput
// does not support it), so we keep two lists.
static const TArray<FString>& McpAllowedPrimaryOutputTypes()
{
    static const TArray<FString> S = {
        TEXT("Float1"), TEXT("Float2"), TEXT("Float3"), TEXT("Float4"),
        TEXT("MaterialAttributes")
    };
    return S;
}

static const TArray<FString>& McpAllowedAdditionalOutputTypes()
{
    static const TArray<FString> S = {
        TEXT("Float1"), TEXT("Float2"), TEXT("Float3"), TEXT("Float4")
    };
    return S;
}

// Maps the wire string name to ECustomMaterialOutputType. Accepts both the
// short form (e.g. "Float2") and the full form (e.g. "CMOT_Float2") for
// backward compatibility with the legacy singular handler.
// Returns CMOT_MAX on miss; allow caller to detect unknown values.
static ECustomMaterialOutputType McpParseCustomOutputType(const FString& S)
{
    if (S == TEXT("Float1") || S == TEXT("CMOT_Float1")) return CMOT_Float1;
    if (S == TEXT("Float2") || S == TEXT("CMOT_Float2")) return CMOT_Float2;
    if (S == TEXT("Float3") || S == TEXT("CMOT_Float3")) return CMOT_Float3;
    if (S == TEXT("Float4") || S == TEXT("CMOT_Float4")) return CMOT_Float4;
    if (S == TEXT("MaterialAttributes")) return CMOT_MaterialAttributes;
    return CMOT_MAX;
}

// Builds a "INVALID_ENUM_VALUE" error citing the allowed list.
static void McpFillEnumError(
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

// Validates a single add_custom_expressions item. Required: code (non-empty).
// Optional: outputType (must be in allowed list), inputs[]/outputs[] entries
// must each have a non-empty name. localId is checked against InOutSeen for
// duplicates.
static bool McpValidateAddCustomItem(
    int32 Index,
    const TSharedPtr<FJsonObject>& Item,
    TSet<FString>& InOutSeenLocalIds,
    FMcpNodeValidationError& Out)
{
    Out = FMcpNodeValidationError();
    Out.Index = Index;

    if (!Item.IsValid())
    {
        Out.Field = TEXT("nodes[]");
        Out.Code = TEXT("INVALID_NODE_SPEC");
        Out.Message = TEXT("Item is not an object");
        return false;
    }

    FString LocalId;
    Item->TryGetStringField(TEXT("localId"), LocalId);
    Out.LocalId = LocalId;
    if (!LocalId.IsEmpty())
    {
        if (InOutSeenLocalIds.Contains(LocalId))
        {
            Out.Field = TEXT("localId");
            Out.Code = TEXT("LOCAL_ID_DUPLICATE");
            Out.Message = FString::Printf(
                TEXT("localId '%s' duplicates an earlier item in this batch"), *LocalId);
            return false;
        }
        InOutSeenLocalIds.Add(LocalId);
    }

    FString Code;
    if (!Item->TryGetStringField(TEXT("code"), Code) || Code.IsEmpty())
    {
        Out.Field = TEXT("code");
        Out.Code = TEXT("INVALID_ARGUMENT");
        Out.Message = TEXT("'code' is required and must be a non-empty string");
        return false;
    }

    // Primary outputType
    FString OutType;
    if (Item->TryGetStringField(TEXT("outputType"), OutType) && !OutType.IsEmpty())
    {
        if (McpParseCustomOutputType(OutType) == CMOT_MAX)
        {
            McpFillEnumError(Index, LocalId, TEXT("outputType"),
                OutType, McpAllowedPrimaryOutputTypes(), Out);
            return false;
        }
    }

    // inputs[]
    const TArray<TSharedPtr<FJsonValue>>* InputsArr = nullptr;
    if (Item->TryGetArrayField(TEXT("inputs"), InputsArr) && InputsArr)
    {
        for (int32 i = 0; i < InputsArr->Num(); ++i)
        {
            const TSharedPtr<FJsonObject>* Obj = nullptr;
            if (!(*InputsArr)[i].IsValid() || !(*InputsArr)[i]->TryGetObject(Obj) || !Obj || !Obj->IsValid())
            {
                Out.Field = FString::Printf(TEXT("inputs[%d]"), i);
                Out.Code = TEXT("INVALID_ARGUMENT");
                Out.Message = TEXT("inputs[] entry must be an object with a 'name' field");
                return false;
            }
            FString Name;
            (*Obj)->TryGetStringField(TEXT("name"), Name);
            if (Name.IsEmpty())
            {
                Out.Field = FString::Printf(TEXT("inputs[%d].name"), i);
                Out.Code = TEXT("INVALID_ARGUMENT");
                Out.Message = TEXT("inputs[] entry 'name' must be a non-empty string");
                return false;
            }
        }
    }

    // outputs[]
    const TArray<TSharedPtr<FJsonValue>>* OutputsArr = nullptr;
    if (Item->TryGetArrayField(TEXT("outputs"), OutputsArr) && OutputsArr)
    {
        for (int32 i = 0; i < OutputsArr->Num(); ++i)
        {
            const TSharedPtr<FJsonObject>* Obj = nullptr;
            if (!(*OutputsArr)[i].IsValid() || !(*OutputsArr)[i]->TryGetObject(Obj) || !Obj || !Obj->IsValid())
            {
                Out.Field = FString::Printf(TEXT("outputs[%d]"), i);
                Out.Code = TEXT("INVALID_ARGUMENT");
                Out.Message = TEXT("outputs[] entry must be an object with 'name' (and optional 'outputType')");
                return false;
            }
            FString Name;
            (*Obj)->TryGetStringField(TEXT("name"), Name);
            if (Name.IsEmpty())
            {
                Out.Field = FString::Printf(TEXT("outputs[%d].name"), i);
                Out.Code = TEXT("INVALID_ARGUMENT");
                Out.Message = TEXT("outputs[] entry 'name' must be a non-empty string");
                return false;
            }
            FString OType;
            if ((*Obj)->TryGetStringField(TEXT("outputType"), OType) && !OType.IsEmpty())
            {
                const ECustomMaterialOutputType Parsed = McpParseCustomOutputType(OType);
                // FCustomOutput does not support MaterialAttributes
                if (Parsed == CMOT_MAX || Parsed == CMOT_MaterialAttributes)
                {
                    McpFillEnumError(Index, LocalId,
                        FString::Printf(TEXT("outputs[%d].outputType"), i),
                        OType, McpAllowedAdditionalOutputTypes(), Out);
                    return false;
                }
            }
        }
    }

    return true;
}

// Applies parsed primary outputType (validated upstream) onto the expression.
// Defaults to CMOT_Float1 when absent.
static void McpApplyPrimaryOutputType(
    UMaterialExpressionCustom* Custom, const TSharedPtr<FJsonObject>& Item)
{
    FString S;
    if (Item->TryGetStringField(TEXT("outputType"), S) && !S.IsEmpty())
    {
        const ECustomMaterialOutputType Parsed = McpParseCustomOutputType(S);
        Custom->OutputType = (Parsed == CMOT_MAX) ? CMOT_Float1 : Parsed;
    }
    else
    {
        Custom->OutputType = CMOT_Float1;
    }
}

// Builds a fresh Inputs array from JSON. Caller does not need to clear first.
static void McpBuildInputs(
    const TArray<TSharedPtr<FJsonValue>>& InputsArr,
    TArray<FCustomInput>& Out)
{
    Out.Reset();
    for (const TSharedPtr<FJsonValue>& V : InputsArr)
    {
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        if (!V.IsValid() || !V->TryGetObject(Obj) || !Obj || !Obj->IsValid()) continue;
        FString Name;
        (*Obj)->TryGetStringField(TEXT("name"), Name);
        if (Name.IsEmpty()) continue;
        FCustomInput CI;
        CI.InputName = FName(*Name);
        Out.Add(CI);
    }
}

// Builds a fresh AdditionalOutputs array from JSON.
static void McpBuildAdditionalOutputs(
    const TArray<TSharedPtr<FJsonValue>>& OutputsArr,
    TArray<FCustomOutput>& Out)
{
    Out.Reset();
    for (const TSharedPtr<FJsonValue>& V : OutputsArr)
    {
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        if (!V.IsValid() || !V->TryGetObject(Obj) || !Obj || !Obj->IsValid()) continue;
        FString Name;
        (*Obj)->TryGetStringField(TEXT("name"), Name);
        if (Name.IsEmpty()) continue;
        FCustomOutput CO;
        CO.OutputName = FName(*Name);
        FString OType;
        if ((*Obj)->TryGetStringField(TEXT("outputType"), OType) && !OType.IsEmpty())
        {
            const ECustomMaterialOutputType Parsed = McpParseCustomOutputType(OType);
            CO.OutputType = (Parsed == CMOT_MAX || Parsed == CMOT_MaterialAttributes)
                ? CMOT_Float1 : Parsed;
        }
        else
        {
            CO.OutputType = CMOT_Float1;
        }
        Out.Add(CO);
    }
}

static void McpBuildAdditionalDefines(
    const TArray<TSharedPtr<FJsonValue>>& DefinesArr,
    TArray<FCustomDefine>& Out)
{
    Out.Reset();
    for (const TSharedPtr<FJsonValue>& V : DefinesArr)
    {
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        if (!V.IsValid() || !V->TryGetObject(Obj) || !Obj || !Obj->IsValid()) continue;
        FString Name, Value;
        (*Obj)->TryGetStringField(TEXT("name"),  Name);
        (*Obj)->TryGetStringField(TEXT("value"), Value);
        FCustomDefine CD;
        CD.DefineName  = Name;
        CD.DefineValue = Value;
        Out.Add(CD);
    }
}

static void McpBuildIncludePaths(
    const TArray<TSharedPtr<FJsonValue>>& IncArr,
    TArray<FString>& Out)
{
    Out.Reset();
    for (const TSharedPtr<FJsonValue>& V : IncArr)
    {
        if (!V.IsValid()) continue;
        FString S = V->AsString();
        if (!S.IsEmpty()) Out.Add(S);
    }
}

#endif // WITH_EDITOR
} // namespace

// =============================================================================
// External entry: McpHandle_AddCustomExpressions (Task C.5)
//
// Payload: { assetPath, nodes: [...], save?: bool }
// All-or-nothing batch; single rebuild + optional save with rollback.
// =============================================================================
extern bool McpHandle_AddCustomExpressions(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool McpHandle_AddCustomExpressions(
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

    const TArray<TSharedPtr<FJsonValue>>* NodesArr = nullptr;
    if (!Payload->TryGetArrayField(TEXT("nodes"), NodesArr) || !NodesArr || NodesArr->Num() == 0)
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("nodes[] is required, minimum length 1"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Phase A: validate every item
    TSet<FString>                   SeenLocalIds;
    TArray<FMcpNodeValidationError> NodeErrors;
    for (int32 i = 0; i < NodesArr->Num(); ++i)
    {
        TSharedPtr<FJsonObject> Item;
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if ((*NodesArr)[i].IsValid() && (*NodesArr)[i]->TryGetObject(ObjPtr) && ObjPtr && ObjPtr->IsValid())
        {
            Item = *ObjPtr;
        }
        FMcpNodeValidationError E;
        if (!McpValidateAddCustomItem(i, Item, SeenLocalIds, E))
        {
            NodeErrors.Add(E);
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
        "McpAddCustomExpressions", "MCP add_custom_expressions"));
    McpModifyMaterialGraphOwnerForTransaction(Owner);

    UObject* MaterialOuter = Owner.GraphSource ? Owner.GraphSource : Owner.Asset;
    TArray<TSharedPtr<FJsonObject>> Mappings;

    for (int32 i = 0; i < NodesArr->Num(); ++i)
    {
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!(*NodesArr)[i].IsValid() || !(*NodesArr)[i]->TryGetObject(ObjPtr) || !ObjPtr || !ObjPtr->IsValid())
        {
            continue;
        }
        const TSharedPtr<FJsonObject>& Item = *ObjPtr;

        UMaterialExpressionCustom* Custom = NewObject<UMaterialExpressionCustom>(
            MaterialOuter, UMaterialExpressionCustom::StaticClass(), NAME_None, RF_Transactional);
        if (!Custom) continue;

        FString Code;
        Item->TryGetStringField(TEXT("code"), Code);
        Custom->Code = Code;

        McpApplyPrimaryOutputType(Custom, Item);

        FString Description;
        if (Item->TryGetStringField(TEXT("description"), Description))
        {
            Custom->Description = Description;
        }

        double X = 0, Y = 0;
        Item->TryGetNumberField(TEXT("x"), X);
        Item->TryGetNumberField(TEXT("y"), Y);
        Custom->MaterialExpressionEditorX = (int32)X;
        Custom->MaterialExpressionEditorY = (int32)Y;
        Custom->MaterialExpressionGuid = FGuid::NewGuid();

        const TArray<TSharedPtr<FJsonValue>>* InputsArr = nullptr;
        if (Item->TryGetArrayField(TEXT("inputs"), InputsArr) && InputsArr)
        {
            McpBuildInputs(*InputsArr, Custom->Inputs);
        }
        const TArray<TSharedPtr<FJsonValue>>* OutputsArr = nullptr;
        if (Item->TryGetArrayField(TEXT("outputs"), OutputsArr) && OutputsArr)
        {
            McpBuildAdditionalOutputs(*OutputsArr, Custom->AdditionalOutputs);
        }
        const TArray<TSharedPtr<FJsonValue>>* DefinesArr = nullptr;
        if (Item->TryGetArrayField(TEXT("additionalDefines"), DefinesArr) && DefinesArr)
        {
            McpBuildAdditionalDefines(*DefinesArr, Custom->AdditionalDefines);
        }
        const TArray<TSharedPtr<FJsonValue>>* IncArr = nullptr;
        if (Item->TryGetArrayField(TEXT("includeFilePaths"), IncArr) && IncArr)
        {
            McpBuildIncludePaths(*IncArr, Custom->IncludeFilePaths);
        }

        // Required after pin array changes so UE rebuilds the dynamic pin set.
        Custom->RebuildOutputs();

        TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressionsMutable(Owner);
        if (Exprs) Exprs->Add(Custom);

        FString LocalId;
        Item->TryGetStringField(TEXT("localId"), LocalId);

        TSharedPtr<FJsonObject> M = MakeShared<FJsonObject>();
        M->SetStringField(TEXT("localId"),         LocalId);
        M->SetStringField(TEXT("nodeId"),          Custom->GetName());
        M->SetStringField(TEXT("expressionGuid"),  Custom->MaterialExpressionGuid.ToString());
        M->SetNumberField(TEXT("expressionIndex"), Exprs ? (Exprs->Num() - 1) : 0);
        Mappings.Add(M);
    }

    // Single rebuild after all items
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

    // Build success response
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField  (TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetNumberField(TEXT("nodesCreated"), NodesArr->Num());
    Resp->SetBoolField  (TEXT("saved"), bSaved);

    TArray<TSharedPtr<FJsonValue>> MappingsJson;
    for (const auto& M : Mappings) MappingsJson.Add(MakeShared<FJsonValueObject>(M));
    Resp->SetArrayField(TEXT("mappings"), MappingsJson);

    Sub->SendAutomationResponse(Socket, RequestId, true,
        TEXT("add_custom_expressions succeeded"), Resp, FString());
    return true;
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("add_custom_expressions requires editor build"), TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =============================================================================
// External entry: McpHandle_UpdateCustomExpressions (Task C.5)
//
// Per-item shape: { identifier, code?, inputs?, outputs?, additionalDefines?,
//   includeFilePaths?, description?, outputType?, x?, y?, onPinRemoved? }
// onPinRemoved: "preserve" (default) | "break".
//
// Pin-removal semantics:
// - "preserve" + live connections on a removed pin -> LIVE_CONNECTIONS_ON_REMOVED_PIN
//   (per-item validation error; abort the whole batch).
// - "break" -> nullify each connection and emit a connectionsBroken[] entry.
// =============================================================================

namespace
{
#if WITH_EDITOR

// Per-item connection-broken record (recorded during Phase A, applied in Phase B).
struct FMcpCustomBrokenConnection
{
    FString IdentifierDisplay;
    FString PinName;
    FString Direction;   // "incoming" or "outgoing"
    FString OtherNode;
    FString OtherPin;

    // Apply-time pointers/indices
    UMaterialExpressionCustom* OurCustom = nullptr;
    int32                      OurInputIndex = INDEX_NONE;  // for incoming on our removed input
    int32                      OurOutputIndex = INDEX_NONE; // for outgoing on our removed output
};

// Per-item output-index remap record (Option A): when a kept-name output's
// position in AdditionalOutputs[] changes, consumer expressions wired to the
// old OutputIndex must be rewritten to point at the new OutputIndex. This is
// NOT a broken connection - it is preserved-but-remapped.
struct FMcpCustomOutputRemap
{
    FString                    OutputName;
    int32                      OldOutputIndex = INDEX_NONE;  // 1 + old AdditionalOutputs index
    int32                      NewOutputIndex = INDEX_NONE;  // 1 + new AdditionalOutputs index
};

// Per-item resolved update record.
struct FMcpCustomUpdate
{
    int32                      Index = INDEX_NONE;
    UMaterialExpressionCustom* Custom = nullptr;
    TSharedPtr<FJsonObject>    Item;
    FString                    IdentifierDisplay;
    bool                       bBreak = false;            // onPinRemoved == "break"
    TArray<FString>            RequestedFields;
    TArray<FMcpCustomBrokenConnection> Broken;            // captured before apply
    TArray<FMcpCustomOutputRemap>      OutputRemaps;      // kept-name outputs that moved
};

// Walks the graph and finds every FExpressionInput on every other expression
// whose Expression matches OurExpr and OutputIndex matches RemovedOutputIndex.
// For each match, records an outgoing-broken-connection descriptor.
static void McpCollectOutgoingForRemovedOutput(
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpressionCustom* OurExpr,
    int32 RemovedOutputIndex,
    const FString& RemovedOutputName,
    const FString& IdentifierDisplay,
    TArray<FMcpCustomBrokenConnection>& OutBroken)
{
    const TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressions(Owner);
    if (!Exprs) return;

    for (UMaterialExpression* Other : *Exprs)
    {
        if (!Other || Other == OurExpr) continue;
        for (FExpressionInputIterator It(Other); It; ++It)
        {
            if (!It.Input) continue;
            if (It->Expression == OurExpr && It->OutputIndex == RemovedOutputIndex)
            {
                FMcpCustomBrokenConnection B;
                B.IdentifierDisplay = IdentifierDisplay;
                B.PinName = RemovedOutputName;
                B.Direction = TEXT("outgoing");
                B.OtherNode = Other->GetName();
                B.OtherPin = Other->GetInputName(It.Index).ToString();
                B.OurCustom = OurExpr;
                B.OurOutputIndex = RemovedOutputIndex;
                OutBroken.Add(B);
            }
        }
    }
}

// Validates a single update item's body against the resolved expression.
// Returns true if all checks pass (and fills OutItem.RequestedFields). Returns
// false and fills OutError on any miss. Fills OutItem.Broken with broken-
// connection descriptors when onPinRemoved == "break"; aborts (and emits a
// LIVE_CONNECTIONS_ON_REMOVED_PIN error) when onPinRemoved == "preserve" and
// any live connections would be lost.
static bool McpValidateUpdateCustomItem(
    int32 Index,
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpressionCustom* Custom,
    const TSharedPtr<FJsonObject>& Item,
    FMcpCustomUpdate& OutItem,
    FMcpNodeValidationError& OutError)
{
    OutError = FMcpNodeValidationError();
    OutError.Index = Index;

    // outputType (primary) - reuse the same enum check as add
    FString PrimaryOut;
    if (Item->TryGetStringField(TEXT("outputType"), PrimaryOut) && !PrimaryOut.IsEmpty())
    {
        if (McpParseCustomOutputType(PrimaryOut) == CMOT_MAX)
        {
            McpFillEnumError(Index, FString(), TEXT("outputType"),
                PrimaryOut, McpAllowedPrimaryOutputTypes(), OutError);
            return false;
        }
    }

    // outputs[] entries (each name non-empty, outputType in allowed)
    const TArray<TSharedPtr<FJsonValue>>* OutputsArr = nullptr;
    const bool bHasOutputs = Item->TryGetArrayField(TEXT("outputs"), OutputsArr) && OutputsArr;
    if (bHasOutputs)
    {
        for (int32 i = 0; i < OutputsArr->Num(); ++i)
        {
            const TSharedPtr<FJsonObject>* Obj = nullptr;
            if (!(*OutputsArr)[i].IsValid() || !(*OutputsArr)[i]->TryGetObject(Obj) || !Obj || !Obj->IsValid())
            {
                OutError.Field = FString::Printf(TEXT("outputs[%d]"), i);
                OutError.Code = TEXT("INVALID_ARGUMENT");
                OutError.Message = TEXT("outputs[] entry must be an object");
                return false;
            }
            FString Name;
            (*Obj)->TryGetStringField(TEXT("name"), Name);
            if (Name.IsEmpty())
            {
                OutError.Field = FString::Printf(TEXT("outputs[%d].name"), i);
                OutError.Code = TEXT("INVALID_ARGUMENT");
                OutError.Message = TEXT("outputs[] entry 'name' must be a non-empty string");
                return false;
            }
            FString OType;
            if ((*Obj)->TryGetStringField(TEXT("outputType"), OType) && !OType.IsEmpty())
            {
                const ECustomMaterialOutputType Parsed = McpParseCustomOutputType(OType);
                if (Parsed == CMOT_MAX || Parsed == CMOT_MaterialAttributes)
                {
                    McpFillEnumError(Index, FString(),
                        FString::Printf(TEXT("outputs[%d].outputType"), i),
                        OType, McpAllowedAdditionalOutputTypes(), OutError);
                    return false;
                }
            }
        }
    }

    // inputs[] entries
    const TArray<TSharedPtr<FJsonValue>>* InputsArr = nullptr;
    const bool bHasInputs = Item->TryGetArrayField(TEXT("inputs"), InputsArr) && InputsArr;
    if (bHasInputs)
    {
        for (int32 i = 0; i < InputsArr->Num(); ++i)
        {
            const TSharedPtr<FJsonObject>* Obj = nullptr;
            if (!(*InputsArr)[i].IsValid() || !(*InputsArr)[i]->TryGetObject(Obj) || !Obj || !Obj->IsValid())
            {
                OutError.Field = FString::Printf(TEXT("inputs[%d]"), i);
                OutError.Code = TEXT("INVALID_ARGUMENT");
                OutError.Message = TEXT("inputs[] entry must be an object");
                return false;
            }
            FString Name;
            (*Obj)->TryGetStringField(TEXT("name"), Name);
            if (Name.IsEmpty())
            {
                OutError.Field = FString::Printf(TEXT("inputs[%d].name"), i);
                OutError.Code = TEXT("INVALID_ARGUMENT");
                OutError.Message = TEXT("inputs[] entry 'name' must be a non-empty string");
                return false;
            }
        }
    }

    // onPinRemoved
    FString PinPolicy = TEXT("preserve");
    Item->TryGetStringField(TEXT("onPinRemoved"), PinPolicy);
    if (!PinPolicy.Equals(TEXT("preserve")) && !PinPolicy.Equals(TEXT("break")))
    {
        McpFillEnumError(Index, FString(), TEXT("onPinRemoved"),
            PinPolicy, TArray<FString>{ TEXT("preserve"), TEXT("break") }, OutError);
        return false;
    }
    OutItem.bBreak = PinPolicy.Equals(TEXT("break"));

    // Compute removed pins (compare existing names to new names) only when the
    // caller actually requested replacement.
    TArray<FName> NewInputNames;
    if (bHasInputs)
    {
        for (const TSharedPtr<FJsonValue>& V : *InputsArr)
        {
            const TSharedPtr<FJsonObject>* Obj = nullptr;
            if (!V.IsValid() || !V->TryGetObject(Obj) || !Obj || !Obj->IsValid()) continue;
            FString N;
            (*Obj)->TryGetStringField(TEXT("name"), N);
            NewInputNames.Add(FName(*N));
        }
    }
    TArray<FName> NewOutputNames;
    if (bHasOutputs)
    {
        for (const TSharedPtr<FJsonValue>& V : *OutputsArr)
        {
            const TSharedPtr<FJsonObject>* Obj = nullptr;
            if (!V.IsValid() || !V->TryGetObject(Obj) || !Obj || !Obj->IsValid()) continue;
            FString N;
            (*Obj)->TryGetStringField(TEXT("name"), N);
            NewOutputNames.Add(FName(*N));
        }
    }

    // Removed inputs -> live incoming connections
    if (bHasInputs)
    {
        for (int32 i = 0; i < Custom->Inputs.Num(); ++i)
        {
            const FCustomInput& CI = Custom->Inputs[i];
            const bool bKept = NewInputNames.Contains(CI.InputName);
            if (bKept) continue;
            const FExpressionInput& In = CI.Input;
            if (In.Expression == nullptr) continue;

            if (!OutItem.bBreak)
            {
                OutError.Field = FString::Printf(TEXT("inputs[].%s"), *CI.InputName.ToString());
                OutError.Code = TEXT("LIVE_CONNECTIONS_ON_REMOVED_PIN");
                OutError.Message = FString::Printf(
                    TEXT("Removed input '%s' has a live incoming connection from '%s'; pass onPinRemoved='break' to nullify, or keep the input"),
                    *CI.InputName.ToString(),
                    *In.Expression->GetName());
                return false;
            }

            FMcpCustomBrokenConnection B;
            B.IdentifierDisplay = OutItem.IdentifierDisplay;
            B.PinName = CI.InputName.ToString();
            B.Direction = TEXT("incoming");
            B.OtherNode = In.Expression->GetName();
            // Source-side pin name lives on the source expression's Outputs[OutputIndex]
            const TArray<FExpressionOutput>& Outs = In.Expression->GetOutputs();
            if (Outs.IsValidIndex(In.OutputIndex))
            {
                const FExpressionOutput& O = Outs[In.OutputIndex];
                if (!O.OutputName.IsNone()) B.OtherPin = O.OutputName.ToString();
            }
            B.OurCustom = Custom;
            B.OurInputIndex = i;
            OutItem.Broken.Add(B);
        }
    }

    // Removed outputs -> live outgoing connections.
    // UE indexes outputs as [primary] then AdditionalOutputs[0..]. We currently
    // only validate AdditionalOutputs replacement (the primary output is
    // governed by outputType, never removed via outputs[]).
    //
    // Option A remap: kept-name outputs whose position changes also affect
    // consumers (consumer's OutputIndex points at the old position). These are
    // captured as OutputRemaps and rewritten at apply time, not reported as
    // broken connections.
    if (bHasOutputs)
    {
        for (int32 i = 0; i < Custom->AdditionalOutputs.Num(); ++i)
        {
            const FCustomOutput& CO = Custom->AdditionalOutputs[i];
            const int32 NewIdx = NewOutputNames.IndexOfByKey(CO.OutputName);
            const bool bKept = NewIdx != INDEX_NONE;
            if (bKept)
            {
                if (NewIdx != i)
                {
                    FMcpCustomOutputRemap R;
                    R.OutputName     = CO.OutputName.ToString();
                    R.OldOutputIndex = 1 + i;
                    R.NewOutputIndex = 1 + NewIdx;
                    OutItem.OutputRemaps.Add(R);
                }
                continue;
            }
            // Output index in GetOutputs() is 1 + i (primary is index 0).
            const int32 RemovedOutputIndex = 1 + i;

            // Probe live consumers
            TArray<FMcpCustomBrokenConnection> Outgoing;
            McpCollectOutgoingForRemovedOutput(Owner, Custom, RemovedOutputIndex,
                CO.OutputName.ToString(), OutItem.IdentifierDisplay, Outgoing);

            if (Outgoing.Num() > 0 && !OutItem.bBreak)
            {
                OutError.Field = FString::Printf(TEXT("outputs[].%s"), *CO.OutputName.ToString());
                OutError.Code = TEXT("LIVE_CONNECTIONS_ON_REMOVED_PIN");
                OutError.Message = FString::Printf(
                    TEXT("Removed output '%s' has %d live outgoing connection(s); pass onPinRemoved='break' to nullify, or keep the output"),
                    *CO.OutputName.ToString(), Outgoing.Num());
                return false;
            }
            OutItem.Broken.Append(Outgoing);
        }
    }

    // Capture user-facing fields requested for update (skip meta/identifier
    // fields). Mirrors the C.2 RequestedFields convention so the response is
    // shaped identically.
    static const TSet<FString> Meta = {
        TEXT("identifier"), TEXT("onPinRemoved")
    };
    for (const auto& Pair : Item->Values)
    {
        if (Meta.Contains(Pair.Key)) continue;
        OutItem.RequestedFields.Add(Pair.Key);
    }
    return true;
}

// Applies the resolved Phase B mutation for a single update item. Pure helper
// shared by the production handler and unit tests; the caller owns transaction
// scope and graph rebuild. Appends per-connection broken/remap descriptors to
// the supplied JSON-array accumulators. Returns the per-item result entry or
// an invalid pointer on no-op.
static TSharedPtr<FJsonObject> McpApplySingleUpdateCustomItem(
    const FMcpMaterialGraphOwner& Owner,
    const FMcpCustomUpdate& U,
    TArray<TSharedPtr<FJsonObject>>& InOutBrokenJson,
    TArray<TSharedPtr<FJsonObject>>& InOutRemappedJson)
{
    if (!U.Custom || !U.Item.IsValid()) return nullptr;
    U.Custom->Modify();

    // First, apply pre-computed broken connections (do this before replacing
    // arrays because the FCustomInput live wires live in the Inputs array we
    // are about to replace, and the outgoing references point AT this
    // expression - those live on OTHER expressions and need their inputs
    // touched separately).
    for (const FMcpCustomBrokenConnection& B : U.Broken)
    {
        if (B.Direction == TEXT("incoming"))
        {
            if (U.Custom->Inputs.IsValidIndex(B.OurInputIndex))
            {
                FExpressionInput& In = U.Custom->Inputs[B.OurInputIndex].Input;
                In.Expression = nullptr;
                In.OutputIndex = 0;
            }
        }
        else // outgoing
        {
            const TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressions(Owner);
            if (!Exprs) continue;
            for (UMaterialExpression* Other : *Exprs)
            {
                if (!Other || Other == U.Custom) continue;
                if (Other->GetName() != B.OtherNode) continue;
                Other->Modify();
                for (FExpressionInputIterator It(Other); It; ++It)
                {
                    if (!It.Input) continue;
                    if (It->Expression == U.Custom && It->OutputIndex == B.OurOutputIndex)
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
        InOutBrokenJson.Add(O);
    }

    // code / description / outputType / x / y
    FString S;
    if (U.Item->TryGetStringField(TEXT("code"), S))
    {
        U.Custom->Code = S;
    }
    if (U.Item->TryGetStringField(TEXT("description"), S))
    {
        U.Custom->Description = S;
    }
    if (U.Item->HasField(TEXT("outputType")))
    {
        McpApplyPrimaryOutputType(U.Custom, U.Item);
    }
    double X = 0, Y = 0;
    if (U.Item->TryGetNumberField(TEXT("x"), X))
    {
        U.Custom->MaterialExpressionEditorX = (int32)X;
    }
    if (U.Item->TryGetNumberField(TEXT("y"), Y))
    {
        U.Custom->MaterialExpressionEditorY = (int32)Y;
    }

    // Replace pin/define/include arrays wholesale when present.
    const TArray<TSharedPtr<FJsonValue>>* InputsArr = nullptr;
    if (U.Item->TryGetArrayField(TEXT("inputs"), InputsArr) && InputsArr)
    {
        // Preserve wires on kept-name inputs. Capture the old Inputs by name
        // before the wholesale replace, then for any new entry whose name
        // matches an old entry, copy the old FExpressionInput into the
        // freshly built FCustomInput. New (previously absent) names start
        // with a default unconnected FExpressionInput.
        //
        // Connections that the validator pre-classified as broken (e.g.
        // onPinRemoved="break" on a removed input) were already nullified
        // above via the U.Broken pass, so no special handling is needed for
        // those - the old wire's Expression is already nullptr by the time
        // we capture OldByName here.
        TMap<FName, FCustomInput> OldByName;
        OldByName.Reserve(U.Custom->Inputs.Num());
        for (const FCustomInput& Old : U.Custom->Inputs)
        {
            if (!Old.InputName.IsNone())
            {
                OldByName.Add(Old.InputName, Old);
            }
        }
        McpBuildInputs(*InputsArr, U.Custom->Inputs);
        for (FCustomInput& Fresh : U.Custom->Inputs)
        {
            if (const FCustomInput* Prior = OldByName.Find(Fresh.InputName))
            {
                // Carry forward the live connection (or lack thereof) on
                // kept-name inputs. The new array's other fields stay as
                // built from JSON.
                Fresh.Input = Prior->Input;
            }
        }
    }
    const TArray<TSharedPtr<FJsonValue>>* OutputsArr = nullptr;
    if (U.Item->TryGetArrayField(TEXT("outputs"), OutputsArr) && OutputsArr)
    {
        McpBuildAdditionalOutputs(*OutputsArr, U.Custom->AdditionalOutputs);

        // Apply Option A: kept-name outputs whose position changed need
        // every consumer's OutputIndex remapped from old to new index. To
        // avoid double-rewrite when ranges overlap (e.g. swap [X, Y] -> [Y, X]
        // both have remaps and the second pass would reread the first pass's
        // result), we snapshot consumer state into Pending before writing.
        if (U.OutputRemaps.Num() > 0)
        {
            const TArray<TObjectPtr<UMaterialExpression>>* Exprs =
                McpGetGraphExpressions(Owner);
            if (Exprs)
            {
                struct FPending
                {
                    UMaterialExpression* Other = nullptr;
                    int32                NewIdx = 0;
                    FExpressionInput*    Pin = nullptr;
                };
                TArray<FPending> Pending;
                for (UMaterialExpression* Other : *Exprs)
                {
                    if (!Other || Other == U.Custom) continue;
                    for (FExpressionInputIterator It(Other); It; ++It)
                    {
                        if (!It.Input) continue;
                        if (It->Expression != U.Custom) continue;
                        for (const FMcpCustomOutputRemap& R : U.OutputRemaps)
                        {
                            if (It->OutputIndex == R.OldOutputIndex)
                            {
                                FPending P;
                                P.Other  = Other;
                                P.NewIdx = R.NewOutputIndex;
                                P.Pin    = It.Input;
                                Pending.Add(P);
                                break;
                            }
                        }
                    }
                }
                UMaterialExpression* LastOther = nullptr;
                for (const FPending& P : Pending)
                {
                    if (!P.Pin || !P.Other) continue;
                    if (P.Other != LastOther)
                    {
                        P.Other->Modify();
                        LastOther = P.Other;
                    }
                    P.Pin->OutputIndex = P.NewIdx;
                }
            }
            // Surface the remap in the response so callers can see that their
            // outputs[] reorder caused consumer-side index churn (preserved,
            // not broken).
            for (const FMcpCustomOutputRemap& R : U.OutputRemaps)
            {
                TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
                O->SetStringField(TEXT("identifier"),     U.IdentifierDisplay);
                O->SetStringField(TEXT("outputName"),     R.OutputName);
                O->SetNumberField(TEXT("oldOutputIndex"), R.OldOutputIndex);
                O->SetNumberField(TEXT("newOutputIndex"), R.NewOutputIndex);
                InOutRemappedJson.Add(O);
            }
        }
    }
    const TArray<TSharedPtr<FJsonValue>>* DefinesArr = nullptr;
    if (U.Item->TryGetArrayField(TEXT("additionalDefines"), DefinesArr) && DefinesArr)
    {
        McpBuildAdditionalDefines(*DefinesArr, U.Custom->AdditionalDefines);
    }
    const TArray<TSharedPtr<FJsonValue>>* IncArr = nullptr;
    if (U.Item->TryGetArrayField(TEXT("includeFilePaths"), IncArr) && IncArr)
    {
        McpBuildIncludePaths(*IncArr, U.Custom->IncludeFilePaths);
    }

    // Required after pin-array changes
    U.Custom->RebuildOutputs();

    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    Item->SetStringField(TEXT("identifier"),     U.IdentifierDisplay);
    Item->SetStringField(TEXT("expressionName"), U.Custom->GetName());
    Item->SetStringField(TEXT("expressionGuid"), U.Custom->MaterialExpressionGuid.ToString());
    TArray<TSharedPtr<FJsonValue>> Fields;
    for (const FString& F : U.RequestedFields)
    {
        Fields.Add(MakeShared<FJsonValueString>(F));
    }
    Item->SetArrayField(TEXT("fieldsUpdated"), Fields);
    return Item;
}

#endif // WITH_EDITOR
} // namespace

extern bool McpHandle_UpdateCustomExpressions(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool McpHandle_UpdateCustomExpressions(
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

    const TArray<TSharedPtr<FJsonValue>>* NodesArr = nullptr;
    if (!Payload->TryGetArrayField(TEXT("updates"), NodesArr) || !NodesArr || NodesArr->Num() == 0)
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("updates[] is required, minimum length 1"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Phase A: resolve identifier per item, validate body, capture broken
    // connection list (when onPinRemoved == "break"). All-or-nothing.
    TArray<FMcpCustomUpdate>        Resolved;
    TArray<FMcpNodeValidationError> NodeErrors;
    Resolved.Reserve(NodesArr->Num());

    for (int32 i = 0; i < NodesArr->Num(); ++i)
    {
        FMcpCustomUpdate U;
        U.Index = i;

        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!(*NodesArr)[i].IsValid() || !(*NodesArr)[i]->TryGetObject(ObjPtr) || !ObjPtr || !ObjPtr->IsValid())
        {
            FMcpNodeValidationError E;
            E.Index = i;
            E.Code = TEXT("INVALID_NODE_SPEC");
            E.Field = TEXT("nodes[]");
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

        UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr);
        if (!Custom)
        {
            FMcpNodeValidationError E;
            E.Index = i;
            E.Code = TEXT("INVALID_NODE_TYPE");
            E.Field = TEXT("identifier");
            E.Message = FString::Printf(
                TEXT("update_custom_expressions expected MaterialExpressionCustom; got %s"),
                Expr ? *Expr->GetClass()->GetName() : TEXT("<null>"));
            NodeErrors.Add(E);
            Resolved.Add(U);
            continue;
        }
        U.Custom = Custom;

        FMcpNodeValidationError VE;
        if (!McpValidateUpdateCustomItem(i, Owner, Custom, U.Item, U, VE))
        {
            NodeErrors.Add(VE);
            Resolved.Add(U);
            continue;
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
        "McpUpdateCustomExpressions", "MCP update_custom_expressions"));
    McpModifyMaterialGraphOwnerForTransaction(Owner);

    TArray<TSharedPtr<FJsonObject>> ResultsJson;
    TArray<TSharedPtr<FJsonObject>> BrokenJson;
    TArray<TSharedPtr<FJsonObject>> RemappedJson;

    for (const FMcpCustomUpdate& U : Resolved)
    {
        TSharedPtr<FJsonObject> ResultItem =
            McpApplySingleUpdateCustomItem(Owner, U, BrokenJson, RemappedJson);
        if (ResultItem.IsValid()) ResultsJson.Add(ResultItem);
    }

    // Single rebuild + optional save
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
    Resp->SetNumberField(TEXT("nodesUpdated"), Resolved.Num());
    Resp->SetBoolField  (TEXT("saved"), bSaved);

    TArray<TSharedPtr<FJsonValue>> ResultsArr;
    for (const auto& R : ResultsJson) ResultsArr.Add(MakeShared<FJsonValueObject>(R));
    Resp->SetArrayField(TEXT("results"), ResultsArr);

    TArray<TSharedPtr<FJsonValue>> BrokenArr;
    for (const auto& B : BrokenJson) BrokenArr.Add(MakeShared<FJsonValueObject>(B));
    Resp->SetArrayField(TEXT("connectionsBroken"), BrokenArr);

    TArray<TSharedPtr<FJsonValue>> RemappedArr;
    for (const auto& R : RemappedJson) RemappedArr.Add(MakeShared<FJsonValueObject>(R));
    Resp->SetArrayField(TEXT("connectionsRemapped"), RemappedArr);

    Sub->SendAutomationResponse(Socket, RequestId, true,
        TEXT("update_custom_expressions succeeded"), Resp, FString());
    return true;
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("update_custom_expressions requires editor build"), TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =============================================================================
// Test-only forwarders for unit tests of the validation/diff logic.
// =============================================================================
namespace McpAddCustomExpressionsValidationForTests
{
#if WITH_EDITOR
    bool ValidateAddCustomItem(
        int32 Index,
        const TSharedPtr<FJsonObject>& Item,
        TSet<FString>& InOutSeenLocalIds,
        FString& OutCode,
        FString& OutField,
        FString& OutMessage)
    {
        FMcpNodeValidationError E;
        const bool b = McpValidateAddCustomItem(Index, Item, InOutSeenLocalIds, E);
        OutCode = E.Code;
        OutField = E.Field;
        OutMessage = E.Message;
        return b;
    }
#endif
}

namespace McpUpdateCustomExpressionsForTests
{
#if WITH_EDITOR
    // Result shape mirrors the production validator; the broken-connection
    // descriptors include direction + names so tests can verify policy.
    struct FBrokenInfo
    {
        FString PinName;
        FString Direction;
        FString OtherNode;
        FString OtherPin;
    };

    struct FRemapInfo
    {
        FString OutputName;
        int32   OldOutputIndex = INDEX_NONE;
        int32   NewOutputIndex = INDEX_NONE;
    };

    bool ValidateUpdateCustomItem(
        const FMcpMaterialGraphOwner& Owner,
        UMaterialExpressionCustom* Custom,
        const TSharedPtr<FJsonObject>& Item,
        FString& OutCode,
        FString& OutField,
        FString& OutMessage,
        TArray<FBrokenInfo>& OutBroken)
    {
        FMcpCustomUpdate U;
        U.Custom = Custom;
        U.Item = Item;
        FMcpNodeValidationError E;
        const bool b = McpValidateUpdateCustomItem(0, Owner, Custom, Item, U, E);
        OutCode = E.Code;
        OutField = E.Field;
        OutMessage = E.Message;
        OutBroken.Reset();
        for (const FMcpCustomBrokenConnection& B : U.Broken)
        {
            FBrokenInfo I;
            I.PinName = B.PinName;
            I.Direction = B.Direction;
            I.OtherNode = B.OtherNode;
            I.OtherPin = B.OtherPin;
            OutBroken.Add(I);
        }
        return b;
    }

    // Validate + apply forwarder: runs the full Phase A validator and (on
    // success) the per-item Phase B mutator against a transient fixture. The
    // caller owns transaction scope; the helper does not call FScopedTransaction
    // or McpRebuildMaterialGraphOwner, since transient fixtures don't need
    // either. The OutBroken/OutRemapped arrays are populated by Phase B.
    bool ValidateAndApplyUpdateCustomItem(
        const FMcpMaterialGraphOwner& Owner,
        UMaterialExpressionCustom* Custom,
        const TSharedPtr<FJsonObject>& Item,
        FString& OutCode,
        FString& OutField,
        FString& OutMessage,
        TArray<FBrokenInfo>& OutBroken,
        TArray<FRemapInfo>& OutRemapped)
    {
        FMcpCustomUpdate U;
        U.Custom = Custom;
        U.Item = Item;
        FMcpNodeValidationError E;
        const bool b = McpValidateUpdateCustomItem(0, Owner, Custom, Item, U, E);
        OutCode = E.Code;
        OutField = E.Field;
        OutMessage = E.Message;
        OutBroken.Reset();
        OutRemapped.Reset();
        if (!b) return false;

        TArray<TSharedPtr<FJsonObject>> BrokenJson;
        TArray<TSharedPtr<FJsonObject>> RemappedJson;
        McpApplySingleUpdateCustomItem(Owner, U, BrokenJson, RemappedJson);

        for (const TSharedPtr<FJsonObject>& O : BrokenJson)
        {
            FBrokenInfo I;
            O->TryGetStringField(TEXT("pinName"),   I.PinName);
            O->TryGetStringField(TEXT("direction"), I.Direction);
            O->TryGetStringField(TEXT("otherNode"), I.OtherNode);
            O->TryGetStringField(TEXT("otherPin"),  I.OtherPin);
            OutBroken.Add(I);
        }
        for (const TSharedPtr<FJsonObject>& O : RemappedJson)
        {
            FRemapInfo I;
            O->TryGetStringField(TEXT("outputName"), I.OutputName);
            double Old = 0, New = 0;
            O->TryGetNumberField(TEXT("oldOutputIndex"), Old);
            O->TryGetNumberField(TEXT("newOutputIndex"), New);
            I.OldOutputIndex = (int32)Old;
            I.NewOutputIndex = (int32)New;
            OutRemapped.Add(I);
        }
        return true;
    }
#endif
}
