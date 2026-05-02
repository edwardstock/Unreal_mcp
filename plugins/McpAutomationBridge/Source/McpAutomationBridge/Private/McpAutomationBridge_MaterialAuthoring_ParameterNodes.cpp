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

// JSON & Serialization
#include "Dom/JsonObject.h"

#if WITH_EDITOR

#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"

// Local copy of LOAD_MATERIAL_OR_RETURN. Mirrors the macro in
// McpAutomationBridge_MaterialAuthoringHandlers.cpp (validates assetPath,
// loads UMaterial, reads x/y) so each domain dispatcher can keep its
// inline blocks unchanged.
#define LOAD_MATERIAL_OR_RETURN()                                              \
  FString AssetPath;                                                           \
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||             \
      AssetPath.IsEmpty()) {                                                   \
    SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),       \
                        TEXT("INVALID_ARGUMENT"));                             \
    return true;                                                               \
  }                                                                            \
  FString ValidatedAssetPath = SanitizeProjectRelativePath(AssetPath);         \
  if (ValidatedAssetPath.IsEmpty()) {                                          \
    SendAutomationError(Socket, RequestId,                                     \
                        FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath), \
                        TEXT("INVALID_PATH"));                                 \
    return true;                                                               \
  }                                                                            \
  AssetPath = ValidatedAssetPath;                                              \
  UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);            \
  if (!Material) {                                                             \
    SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),   \
                        TEXT("ASSET_NOT_FOUND"));                              \
    return true;                                                               \
  }                                                                            \
  float X = 0.0f, Y = 0.0f;                                                    \
  Payload->TryGetNumberField(TEXT("x"), X);                                    \
  Payload->TryGetNumberField(TEXT("y"), Y)

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_ParameterNodes(
    const FString& SubAction, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  // --------------------------------------------------------------------------
  // add_scalar_parameter
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_scalar_parameter")) {
    LOAD_MATERIAL_OR_RETURN();

    FString ParamName, Group;
    double DefaultValue = 0.0;
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
        ParamName.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetNumberField(TEXT("defaultValue"), DefaultValue);
    Payload->TryGetStringField(TEXT("group"), Group);

    UMaterialExpressionScalarParameter *ScalarParam =
        NewObject<UMaterialExpressionScalarParameter>(
            Material, UMaterialExpressionScalarParameter::StaticClass(),
            NAME_None, RF_Transactional);
    ScalarParam->ParameterName = FName(*ParamName);
    ScalarParam->DefaultValue = DefaultValue;
    if (!Group.IsEmpty()) {
      ScalarParam->Group = FName(*Group);
    }
    ScalarParam->MaterialExpressionEditorX = (int32)X;
    ScalarParam->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(ScalarParam);
#endif

    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           ScalarParam->MaterialExpressionGuid.ToString());
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Scalar parameter '%s' added."), *ParamName),
        Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_vector_parameter
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_vector_parameter")) {
    LOAD_MATERIAL_OR_RETURN();

    FString ParamName, Group;
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
        ParamName.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetStringField(TEXT("group"), Group);

    UMaterialExpressionVectorParameter *VecParam =
        NewObject<UMaterialExpressionVectorParameter>(
            Material, UMaterialExpressionVectorParameter::StaticClass(),
            NAME_None, RF_Transactional);
    VecParam->ParameterName = FName(*ParamName);
    if (!Group.IsEmpty()) {
      VecParam->Group = FName(*Group);
    }

    // Parse default value
    const TSharedPtr<FJsonObject> *DefaultObj;
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

#if WITH_EDITORONLY_DATA
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(VecParam);
#endif

    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           VecParam->MaterialExpressionGuid.ToString());
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Vector parameter '%s' added."), *ParamName),
        Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_static_switch_parameter
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_static_switch_parameter")) {
    LOAD_MATERIAL_OR_RETURN();

    FString ParamName, Group;
    bool DefaultValue = false;
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
        ParamName.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetBoolField(TEXT("defaultValue"), DefaultValue);
    Payload->TryGetStringField(TEXT("group"), Group);

    UMaterialExpressionStaticSwitchParameter *SwitchParam =
        NewObject<UMaterialExpressionStaticSwitchParameter>(
            Material, UMaterialExpressionStaticSwitchParameter::StaticClass(),
            NAME_None, RF_Transactional);
    SwitchParam->ParameterName = FName(*ParamName);
    SwitchParam->DefaultValue = DefaultValue;
    if (!Group.IsEmpty()) {
      SwitchParam->Group = FName(*Group);
    }
    SwitchParam->MaterialExpressionEditorX = (int32)X;
    SwitchParam->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(SwitchParam);
#endif

    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           SwitchParam->MaterialExpressionGuid.ToString());
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Static switch '%s' added."), *ParamName), Result);
    return true;
  }

  return false;
}

#undef LOAD_MATERIAL_OR_RETURN

#else // !WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_ParameterNodes(
    const FString& /*SubAction*/, const FString& /*RequestId*/,
    const TSharedPtr<FJsonObject>& /*Payload*/,
    TSharedPtr<FMcpBridgeWebSocket> /*Socket*/)
{
  return false;
}

#endif // WITH_EDITOR
