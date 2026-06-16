/**
 * McpAutomationBridge_MaterialAuthoring_ParameterNodes.cpp
 *
 * Phase 8: Material Authoring - parameter node sub-actions.
 *
 * Decomposed from McpAutomationBridge_MaterialAuthoringHandlers.cpp.
 * Handles: add_scalar_parameter, add_vector_parameter, add_static_switch_parameter.
 */

// MCP Core
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_MaterialExpressionDetails.h"

// JSON & Serialization
#include "Dom/JsonObject.h"

#if WITH_EDITOR

#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "ScopedTransaction.h"

// Local macro: validates assetPath, resolves it through McpResolveMaterialGraphOwner
// (so UMaterial AND UMaterialFunction graphs both work), reads x/y. Exposes:
//   FMcpMaterialGraphOwner GraphOwner;
//   FString AssetPath;
//   float X, Y;
// Refuses read-only graphs (e.g., UMaterialFunctionInstance) with a clear error.
#define LOAD_GRAPH_OWNER_OR_RETURN()                                            \
  FString AssetPath;                                                            \
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||              \
      AssetPath.IsEmpty()) {                                                    \
    SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),        \
                        TEXT("INVALID_ARGUMENT"));                              \
    return true;                                                                \
  }                                                                             \
  {                                                                             \
    FString Validated = SanitizeProjectRelativePath(AssetPath);                 \
    if (Validated.IsEmpty()) {                                                  \
      SendAutomationError(Socket, RequestId,                                    \
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath), \
                          TEXT("INVALID_PATH"));                                \
      return true;                                                              \
    }                                                                           \
    AssetPath = Validated;                                                      \
  }                                                                             \
  FMcpMaterialGraphOwner GraphOwner;                                            \
  {                                                                             \
    FString GraphOwnerError;                                                    \
    if (!McpResolveMaterialGraphOwner(AssetPath, GraphOwner, GraphOwnerError) || \
        GraphOwner.bReadOnly) {                                                 \
      SendAutomationError(Socket, RequestId,                                    \
                          GraphOwnerError.IsEmpty() ? TEXT("Cannot mutate this asset.") : GraphOwnerError, \
                          TEXT("ASSET_NOT_FOUND"));                             \
      return true;                                                              \
    }                                                                           \
  }                                                                             \
  float X = 0.0f, Y = 0.0f;                                                     \
  Payload->TryGetNumberField(TEXT("x"), X);                                     \
  Payload->TryGetNumberField(TEXT("y"), Y)

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_ParameterNodes(
    const FString& SubAction,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
    // --------------------------------------------------------------------------
    // add_scalar_parameter
    // --------------------------------------------------------------------------
    if (SubAction == TEXT("add_scalar_parameter")) {
        LOAD_GRAPH_OWNER_OR_RETURN();

        FString ParamName, Group;
        double DefaultValue = 0.0;
        if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
            ParamName.IsEmpty()) {
            SendAutomationError(Socket,
                RequestId,
                TEXT("Missing 'parameterName'."),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }
        if (!Payload->TryGetNumberField(TEXT("defaultValue"), DefaultValue)) {
            const TSharedPtr<FJsonObject>* DefObj = nullptr;
            if (Payload->TryGetObjectField(TEXT("defaultValue"), DefObj) && DefObj && DefObj->IsValid())
                (*DefObj)->TryGetNumberField(TEXT("value"), DefaultValue);
        }
        Payload->TryGetStringField(TEXT("group"), Group);

        FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge",
            "AddScalarParameter",
            "MCP add scalar parameter"));
        GraphOwner.Asset->Modify();

        UObject* Outer = GraphOwner.GraphSource ? GraphOwner.GraphSource : GraphOwner.Asset;
        UMaterialExpressionScalarParameter* ScalarParam =
            NewObject<UMaterialExpressionScalarParameter>(
                Outer,
                UMaterialExpressionScalarParameter::StaticClass(),
                NAME_None,
                RF_Transactional);
        ScalarParam->ParameterName = FName(*ParamName);
        ScalarParam->DefaultValue = DefaultValue;
        if (!Group.IsEmpty()) {
            ScalarParam->Group = FName(*Group);
        }
        ScalarParam->MaterialExpressionEditorX = (int32)X;
        ScalarParam->MaterialExpressionEditorY = (int32)Y;
        ScalarParam->MaterialExpressionGuid = FGuid::NewGuid();

        if (TArray<TObjectPtr<UMaterialExpression>>* Exprs =
            McpGetGraphExpressionsMutable(GraphOwner)) {
            Exprs->Add(ScalarParam);
        }

        FString RebuildErr;
        McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("nodeId"),
            ScalarParam->MaterialExpressionGuid.ToString());
        SendAutomationResponse(
            Socket,
            RequestId,
            true,
            FString::Printf(TEXT("Scalar parameter '%s' added."), *ParamName),
            Result);
        return true;
    }

    // --------------------------------------------------------------------------
    // add_vector_parameter
    // --------------------------------------------------------------------------
    if (SubAction == TEXT("add_vector_parameter")) {
        LOAD_GRAPH_OWNER_OR_RETURN();

        FString ParamName, Group;
        if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
            ParamName.IsEmpty()) {
            SendAutomationError(Socket,
                RequestId,
                TEXT("Missing 'parameterName'."),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }
        Payload->TryGetStringField(TEXT("group"), Group);

        FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge",
            "AddVectorParameter",
            "MCP add vector parameter"));
        GraphOwner.Asset->Modify();

        UObject* Outer = GraphOwner.GraphSource ? GraphOwner.GraphSource : GraphOwner.Asset;
        UMaterialExpressionVectorParameter* VecParam =
            NewObject<UMaterialExpressionVectorParameter>(
                Outer,
                UMaterialExpressionVectorParameter::StaticClass(),
                NAME_None,
                RF_Transactional
            );
        VecParam->ParameterName = FName(*ParamName);


        const TSharedPtr<FJsonObject>* ChannelNameValues;
        if (Payload->TryGetObjectField(TEXT("channelNames"), ChannelNameValues)) {
            FParameterChannelNames ChannelNames = VecParam->ChannelNames;
            FString R, G, B, A;
            if ((*ChannelNameValues)->TryGetStringField(TEXT("r"), R)) {
                ChannelNames.R = FText::FromString(R);
            }
            if ((*ChannelNameValues)->TryGetStringField(TEXT("g"), G)) {
                ChannelNames.G = FText::FromString(G);
            }
            if ((*ChannelNameValues)->TryGetStringField(TEXT("b"), B)) {
                ChannelNames.B = FText::FromString(B);
            }
            if ((*ChannelNameValues)->TryGetStringField(TEXT("a"), A)) {
                ChannelNames.A = FText::FromString(A);
            }
            VecParam->ChannelNames = ChannelNames;
        }

        if (!Group.IsEmpty()) {
            VecParam->Group = FName(*Group);
        }

        // Parse default value
        const TSharedPtr<FJsonObject>* DefaultObj;
        if (Payload->TryGetObjectField(TEXT("defaultValue"), DefaultObj)) {
            double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
            (*DefaultObj)->TryGetNumberField(TEXT("r"), R);
            (*DefaultObj)->TryGetNumberField(TEXT("g"), G);
            (*DefaultObj)->TryGetNumberField(TEXT("b"), B);
            (*DefaultObj)->TryGetNumberField(TEXT("a"), A);
            VecParam->DefaultValue = FLinearColor(R, G, B, A);
        }

        VecParam->MaterialExpressionEditorX = (int32)X;
        VecParam->MaterialExpressionEditorY = (int32)Y;
        VecParam->MaterialExpressionGuid = FGuid::NewGuid();

        if (TArray<TObjectPtr<UMaterialExpression>>* Exprs =
            McpGetGraphExpressionsMutable(GraphOwner)) {
            Exprs->Add(VecParam);
        }

        FString RebuildErr;
        McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("nodeId"),
            VecParam->MaterialExpressionGuid.ToString());
        SendAutomationResponse(
            Socket,
            RequestId,
            true,
            FString::Printf(TEXT("Vector parameter '%s' added."), *ParamName),
            Result);
        return true;
    }

    // --------------------------------------------------------------------------
    // add_static_switch_parameter
    // --------------------------------------------------------------------------
    if (SubAction == TEXT("add_static_switch_parameter")) {
        LOAD_GRAPH_OWNER_OR_RETURN();

        FString ParamName, Group;
        bool DefaultValue = false;
        if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
            ParamName.IsEmpty()) {
            SendAutomationError(Socket,
                RequestId,
                TEXT("Missing 'parameterName'."),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }
        if (!Payload->TryGetBoolField(TEXT("defaultValue"), DefaultValue)) {
            const TSharedPtr<FJsonObject>* DefObj = nullptr;
            if (Payload->TryGetObjectField(TEXT("defaultValue"), DefObj) && DefObj && DefObj->IsValid())
                (*DefObj)->TryGetBoolField(TEXT("value"), DefaultValue);
        }
        Payload->TryGetStringField(TEXT("group"), Group);

        FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge",
            "AddStaticSwitchParameter",
            "MCP add static switch parameter"));
        GraphOwner.Asset->Modify();

        UObject* Outer = GraphOwner.GraphSource ? GraphOwner.GraphSource : GraphOwner.Asset;
        UMaterialExpressionStaticSwitchParameter* SwitchParam =
            NewObject<UMaterialExpressionStaticSwitchParameter>(
                Outer,
                UMaterialExpressionStaticSwitchParameter::StaticClass(),
                NAME_None,
                RF_Transactional);
        SwitchParam->ParameterName = FName(*ParamName);
        SwitchParam->DefaultValue = DefaultValue;
        if (!Group.IsEmpty()) {
            SwitchParam->Group = FName(*Group);
        }
        SwitchParam->MaterialExpressionEditorX = (int32)X;
        SwitchParam->MaterialExpressionEditorY = (int32)Y;
        SwitchParam->MaterialExpressionGuid = FGuid::NewGuid();

        if (TArray<TObjectPtr<UMaterialExpression>>* Exprs =
            McpGetGraphExpressionsMutable(GraphOwner)) {
            Exprs->Add(SwitchParam);
        }

        FString RebuildErr;
        McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("nodeId"),
            SwitchParam->MaterialExpressionGuid.ToString());
        SendAutomationResponse(
            Socket,
            RequestId,
            true,
            FString::Printf(TEXT("Static switch '%s' added."), *ParamName),
            Result);
        return true;
    }

    return false;
}

#undef LOAD_GRAPH_OWNER_OR_RETURN

// helpers defined in McpAutomationBridge_AssetWorkflowHandlers.cpp (file-scope externals);
// McpResolveMaterialGraphOwner and McpGetGraphExpressions are static inline in McpAutomationBridgeHelpers.h
extern int32 McpExpressionIndex(const FMcpMaterialGraphOwner& Owner, const UMaterialExpression* Expression);
extern void McpAddExpressionIdentity(const FMcpMaterialGraphOwner& Owner, UMaterialExpression* Expression, int32 Index, const TSharedRef<FJsonObject>& Out);

bool UMcpAutomationBridgeSubsystem::HandleGetParameterDefaults(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (!Action.Equals(TEXT("get_parameter_defaults"), ESearchCase::IgnoreCase)) return false;

    if (!Payload.IsValid())
    {
        SendAutomationError(Socket, RequestId, TEXT("payload missing"), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("assetPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FMcpMaterialGraphOwner Owner;
    FString Err;
    if (!McpResolveMaterialGraphOwner(AssetPath, Owner, Err))
    {
        SendAutomationError(Socket, RequestId, Err,
            Err.Contains(TEXT("not found")) ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE"));
        return true;
    }

    const TArray<TObjectPtr<UMaterialExpression>>* AllPtr = McpGetGraphExpressions(Owner);
    static const TArray<TObjectPtr<UMaterialExpression>> Empty;
    const auto& All = AllPtr ? *AllPtr : Empty;

    TArray<TSharedPtr<FJsonValue>> Items;
    for (int32 i = 0; i < All.Num(); ++i)
    {
        UMaterialExpression* Expr = All[i];
        if (!Expr) continue;
        // UMaterialExpressionTextureSampleParameter and UMaterialExpressionTextureObjectParameter
        // are NOT subclasses of UMaterialExpressionParameter, so check each explicitly
        if (!Cast<UMaterialExpressionParameter>(Expr) &&
            !Cast<UMaterialExpressionTextureSampleParameter>(Expr) &&
            !Cast<UMaterialExpressionTextureObjectParameter>(Expr))
        {
            continue;
        }
        TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
        TSharedPtr<FJsonObject> Identity = MakeShared<FJsonObject>();
        McpAddExpressionIdentity(Owner, Expr, i, Identity.ToSharedRef());
        Item->SetObjectField(TEXT("nodeIdentity"), Identity);
        Item->SetStringField(TEXT("kind"), Expr->GetClass()->GetName());
        McpMaterialExpressionDetails::AppendParameterDetails(Expr, Item.ToSharedRef());
        Items.Add(MakeShared<FJsonValueObject>(Item));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Owner.Asset);
    Result->SetArrayField(TEXT("parameters"), Items);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Parameter defaults retrieved"), Result, FString());
    return true;
}

#else // !WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_ParameterNodes(
    const FString& /*SubAction*/,
    const FString& /*RequestId*/,
    const TSharedPtr<FJsonObject>& /*Payload*/,
    TSharedPtr<FMcpBridgeWebSocket> /*Socket*/) {
    return false;
}

bool UMcpAutomationBridgeSubsystem::HandleGetParameterDefaults(
    const FString& /*RequestId*/, const FString& /*Action*/,
    const TSharedPtr<FJsonObject>& /*Payload*/,
    TSharedPtr<FMcpBridgeWebSocket> /*Socket*/)
{
    return false;
}

#endif // WITH_EDITOR
