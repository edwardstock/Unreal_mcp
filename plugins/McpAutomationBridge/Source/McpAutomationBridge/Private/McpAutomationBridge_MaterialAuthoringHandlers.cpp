/**
 * McpAutomationBridge_MaterialAuthoringHandlers.cpp
 * =============================================================================
 * Phase 8: Material Authoring System Handlers
 *
 * Provides advanced material creation and shader authoring capabilities for the MCP
 * Automation Bridge. This file implements the `manage_material_authoring` tool.
 *
 * HANDLERS BY CATEGORY:
 * ---------------------
 * 8.1  Material Creation    - create_material, create_material_instance, create_material_function
 * 8.2  Expression Nodes     - add_expression, remove_expression, connect_expressions,
 *                              disconnect_expressions, get_expression_info
 * 8.3  Material Properties  - set_material_property, set_material_shading_model,
 *                              set_material_blend_mode, set_material_two_sided
 * 8.4  Parameters           - add_scalar_parameter, add_vector_parameter, add_texture_parameter,
 *                              set_parameter_default, get_parameter_value
 * 8.5  Material Functions   - create_material_function, call_material_function,
 *                              add_function_input, add_function_output
 * 8.6  Specialized Materials - create_landscape_material, create_decal_material,
 *                              create_post_process_material, add_landscape_layer
 * 8.7  Material Instances   - create_material_instance, set_instance_parameter,
 *                              create_material_instance_dynamic
 * 8.8  Utility Actions      - compile_material, get_material_info, export_material_code,
 *                              duplicate_material
 *
 * VERSION COMPATIBILITY:
 * ----------------------
 * - UE 5.0: Material->Expressions (direct access)
 * - UE 5.1+: Material->GetEditorOnlyData()->ExpressionCollection.Expressions
 * - UE 5.1+: MaterialExpressionRotator, MaterialDomain.h available
 * - MCP_GET_MATERIAL_EXPRESSIONS macro handles version differences
 *
 * REFACTORING NOTES:
 * ------------------
 * - Uses McpHandlerUtils for JSON parsing and response building
 * - McpSafeAssetSave for UE 5.7+ safe asset saving
 * - Path validation via SanitizeProjectRelativePath()
 * - Expression finding by ID or name with robust lookup
 *
 * Copyright (c) 2024 MCP Automation Bridge Contributors
 */

// MCP Core
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpVersionCompatibility.h"

// JSON & Serialization
#include "Dom/JsonObject.h"

// Engine Version
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR

// Asset Tools & Registry
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"

// Graph
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"

// Material Core
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInstance.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/Texture.h"

// UE 5.1+ MaterialDomain
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "MaterialDomain.h"
#endif

// Material Expressions (Basic)
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionFrac.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionIf.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionPixelDepth.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionReflectionVectorWS.h"

// UE 5.1+ MaterialExpressionRotator
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "Materials/MaterialExpressionRotator.h"
#endif

// Material Expressions (Parameters)
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"

// Material Expressions (Utility)
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionCrossProduct.h"
#include "Materials/MaterialExpressionDesaturation.h"

// Material Expressions (Texture extended)
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"

// Material Expressions (Material Attributes)
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"

// Material Function Instances
#include "Materials/MaterialFunctionMaterialLayer.h"
#include "Materials/MaterialFunctionMaterialLayerBlend.h"
#include "Factories/MaterialFunctionInstanceFactory.h"

// Factories
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialFunctionFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"

// Core
#include "UObject/SavePackage.h"
#include "ScopedTransaction.h"
#include "EditorAssetLibrary.h"
#include "Engine/Font.h"

// Landscape (UE 5.0+)
#if ENGINE_MAJOR_VERSION >= 5
#include "LandscapeLayerInfoObject.h"
#define MCP_HAS_LANDSCAPE_LAYER 1
#else
#define MCP_HAS_LANDSCAPE_LAYER 0
#endif
#endif

bool UMcpAutomationBridgeSubsystem::HandleManageMaterialAuthoringAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (Action != TEXT("manage_material_authoring")) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing payload."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  if (!Payload->TryGetStringField(TEXT("subAction"), SubAction) || SubAction.IsEmpty())
  {
    SendAutomationError(Socket, RequestId,
                        TEXT("Missing 'subAction' for manage_material_authoring"),
                        TEXT("MISSING_SUB_ACTION"));
    return true;
  }

  // C.1: redesigned add_material_nodes (transactional batch with inline connections).
  // Lives in McpAutomationBridge_Material_GraphWrites.cpp.
  extern bool McpHandle_AddMaterialNodes(
      UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
  if (SubAction == TEXT("add_material_nodes")) {
    return McpHandle_AddMaterialNodes(this, RequestId, Payload, Socket);
  }

  // C.2: update_material_nodes (transactional, applicability against resolved class).
  // Lives in McpAutomationBridge_Material_GraphWrites.cpp.
  extern bool McpHandle_UpdateMaterialNodes(
      UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
  if (SubAction == TEXT("update_material_nodes")) {
    return McpHandle_UpdateMaterialNodes(this, RequestId, Payload, Socket);
  }

  // C.3: remove_material_nodes (transactional batch).
  // Lives in McpAutomationBridge_Material_GraphWrites.cpp.
  extern bool McpHandle_RemoveMaterialNodes(
      UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
  if (SubAction == TEXT("remove_material_nodes")) {
    return McpHandle_RemoveMaterialNodes(this, RequestId, Payload, Socket);
  }

  // C.4: connect_material_pins (transactional batch).
  // Lives in McpAutomationBridge_Material_GraphWrites.cpp.
  extern bool McpHandle_ConnectMaterialPins(
      UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
  if (SubAction == TEXT("connect_material_pins")) {
    return McpHandle_ConnectMaterialPins(this, RequestId, Payload, Socket);
  }

  // C.4: break_material_connections (transactional batch).
  // Lives in McpAutomationBridge_Material_GraphWrites.cpp.
  extern bool McpHandle_BreakMaterialConnections(
      UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
  if (SubAction == TEXT("break_material_connections")) {
    return McpHandle_BreakMaterialConnections(this, RequestId, Payload, Socket);
  }

  // C.5: add_custom_expressions (transactional batch of UMaterialExpressionCustom).
  // Lives in McpAutomationBridge_Material_CustomExpressions.cpp.
  extern bool McpHandle_AddCustomExpressions(
      UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
  if (SubAction == TEXT("add_custom_expressions")) {
    return McpHandle_AddCustomExpressions(this, RequestId, Payload, Socket);
  }

  // C.5: update_custom_expressions (transactional batch with onPinRemoved policy).
  // Lives in McpAutomationBridge_Material_CustomExpressions.cpp.
  extern bool McpHandle_UpdateCustomExpressions(
      UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
  if (SubAction == TEXT("update_custom_expressions")) {
    return McpHandle_UpdateCustomExpressions(this, RequestId, Payload, Socket);
  }

  // C.6: function authoring + function calls.
  // All live in McpAutomationBridge_Material_FunctionAuthoring.cpp.
  extern bool McpHandle_AddFunctionInputs(
      UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
  if (SubAction == TEXT("add_function_inputs")) {
    return McpHandle_AddFunctionInputs(this, RequestId, Payload, Socket);
  }
  extern bool McpHandle_AddFunctionOutputs(
      UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
  if (SubAction == TEXT("add_function_outputs")) {
    return McpHandle_AddFunctionOutputs(this, RequestId, Payload, Socket);
  }
  extern bool McpHandle_UpdateFunctionInputs(
      UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
  if (SubAction == TEXT("update_function_inputs")) {
    return McpHandle_UpdateFunctionInputs(this, RequestId, Payload, Socket);
  }
  extern bool McpHandle_UpdateFunctionOutputs(
      UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
  if (SubAction == TEXT("update_function_outputs")) {
    return McpHandle_UpdateFunctionOutputs(this, RequestId, Payload, Socket);
  }
  extern bool McpHandle_AddMaterialFunctionCalls(
      UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
  if (SubAction == TEXT("add_material_function_calls")) {
    return McpHandle_AddMaterialFunctionCalls(this, RequestId, Payload, Socket);
  }
  extern bool McpHandle_UpdateMaterialFunctionCalls(
      UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
  if (SubAction == TEXT("update_material_function_calls")) {
    return McpHandle_UpdateMaterialFunctionCalls(this, RequestId, Payload, Socket);
  }

  // E.1: find_material_expressions - consolidated read API.
  // Lives in McpAutomationBridge_Material_GraphReads.cpp.
  extern bool McpHandle_FindMaterialExpressions(
      UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
  if (SubAction == TEXT("find_material_expressions")) {
    return McpHandle_FindMaterialExpressions(this, RequestId, Payload, Socket);
  }

  // Per-domain dispatchers (decomposition of Phase 8 authoring sub-actions).
  // Each returns true if it consumed the request; false to fall through.
  if (HandleAuthoring_ParameterNodes(SubAction, RequestId, Payload, Socket)) {
    return true;
  }
  if (HandleAuthoring_NodeOps(SubAction, RequestId, Payload, Socket)) {
    return true;
  }
  if (HandleAuthoring_MaterialCreation(SubAction, RequestId, Payload, Socket)) {
    return true;
  }
  if (HandleAuthoring_MaterialProperties(SubAction, RequestId, Payload, Socket)) {
    return true;
  }
  if (HandleAuthoring_TextureNodes(SubAction, RequestId, Payload, Socket)) {
    return true;
  }
  if (HandleAuthoring_AdvancedNodes(SubAction, RequestId, Payload, Socket)) {
    return true;
  }
  if (HandleAuthoring_FunctionAuthoring(SubAction, RequestId, Payload, Socket)) {
    return true;
  }
  if (HandleAuthoring_SpecializedMaterials(SubAction, RequestId, Payload, Socket)) {
    return true;
  }
  if (HandleAuthoring_MaterialInstances(SubAction, RequestId, Payload, Socket)) {
    return true;
  }
  if (HandleAuthoring_FunctionInstances(SubAction, RequestId, Payload, Socket)) {
    return true;
  }

  // Unknown subAction
  SendAutomationError(
      Socket, RequestId,
      FString::Printf(TEXT("Unknown subAction: %s"), *SubAction),
      TEXT("INVALID_SUBACTION"));
  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Editor only."),
                      TEXT("EDITOR_ONLY"));
  return true;
#endif
}
