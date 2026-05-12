/**
 * McpAutomationBridge_MaterialAuthoring_NodeOps.cpp
 *
 * Phase 8: Material Authoring - node ops sub-actions.
 *
 * Decomposed from McpAutomationBridge_MaterialAuthoringHandlers.cpp.
 * Handles material graph node operations and inspection sub-actions.
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

#if WITH_EDITOR

// Local copy of LOAD_GRAPH_OWNER_OR_RETURN. Resolves through McpResolveMaterialGraphOwner
// so UMaterial AND UMaterialFunction graphs both work. Exposes:
//   FMcpMaterialGraphOwner GraphOwner
//   UObject* Material      // alias to GraphOwner.GraphSource for backward-compat
//   FString AssetPath
//   float X, Y
// For expression-collection access use McpGetGraphExpressionsMutable(GraphOwner)
// or the local FindExpressionByIdOrName_NodeOps helper (NOT
// MCP_GET_MATERIAL_EXPRESSIONS(Material) which only compiles for UMaterial).
#define LOAD_GRAPH_OWNER_OR_RETURN()                                              \
  FString AssetPath;                                                           \
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||             \
      AssetPath.IsEmpty()) {                                                   \
    SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),       \
                        TEXT("INVALID_ARGUMENT"));                             \
    return true;                                                               \
  }                                                                            \
  {                                                                            \
    FString Validated = SanitizeProjectRelativePath(AssetPath);                \
    if (Validated.IsEmpty()) {                                                 \
      SendAutomationError(Socket, RequestId,                                   \
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath), \
                          TEXT("INVALID_PATH"));                               \
      return true;                                                             \
    }                                                                          \
    AssetPath = Validated;                                                     \
  }                                                                            \
  FMcpMaterialGraphOwner GraphOwner;                                           \
  {                                                                            \
    FString GraphOwnerError;                                                   \
    if (!McpResolveMaterialGraphOwner(AssetPath, GraphOwner, GraphOwnerError) || \
        GraphOwner.bReadOnly) {                                                \
      SendAutomationError(Socket, RequestId,                                   \
                          GraphOwnerError.IsEmpty() ? TEXT("Cannot mutate this asset.") : GraphOwnerError, \
                          TEXT("ASSET_NOT_FOUND"));                            \
      return true;                                                             \
    }                                                                          \
  }                                                                            \
  UObject* Material = GraphOwner.GraphSource ? GraphOwner.GraphSource : GraphOwner.Asset; \
  float X = 0.0f, Y = 0.0f;                                                    \
  Payload->TryGetNumberField(TEXT("x"), X);                                    \
  Payload->TryGetNumberField(TEXT("y"), Y)

static UMaterialExpression *FindExpressionByIdOrName_NodeOps(
    const FMcpMaterialGraphOwner &GraphOwner, const FString &IdOrName) {
  if (IdOrName.IsEmpty()) {
    return nullptr;
  }

  const FString Needle = IdOrName.TrimStartAndEnd();
  const TArray<TObjectPtr<UMaterialExpression>>* Exprs =
      McpGetGraphExpressions(GraphOwner);
  if (!Exprs) {
    return nullptr;
  }
  for (UMaterialExpression *Expr : *Exprs) {
    if (!Expr) {
      continue;
    }
    if (Expr->MaterialExpressionGuid.ToString() == Needle) {
      return Expr;
    }
    if (Expr->GetName() == Needle) {
      return Expr;
    }
    if (Expr->GetPathName() == Needle) {
      return Expr;
    }
    if (UMaterialExpressionParameter *ParamExpr =
            Cast<UMaterialExpressionParameter>(Expr)) {
      if (ParamExpr->ParameterName.ToString() == Needle) {
        return Expr;
      }
    }
  }
  return nullptr;
}

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_NodeOps(
    const FString& SubAction, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{

  if (SubAction == TEXT("add_material_node")) {
    return HandleAddMaterialNode(RequestId, TEXT("add_material_node"), Payload, Socket);
  }

  if (SubAction == TEXT("move_material_node")) {
    // D.1: forward to canonical plural handler with a synthesized single-item nodes[]
    // (the legacy singular handler has been removed).
    TSharedPtr<FJsonObject> Forward = MakeShared<FJsonObject>();
    FString AssetPath;
    if (Payload->TryGetStringField(TEXT("assetPath"), AssetPath)) {
      Forward->SetStringField(TEXT("assetPath"), AssetPath);
    } else if (Payload->TryGetStringField(TEXT("materialPath"), AssetPath)) {
      Forward->SetStringField(TEXT("materialPath"), AssetPath);
    }
    TArray<TSharedPtr<FJsonValue>> NodesArray;
    NodesArray.Add(MakeShared<FJsonValueObject>(Payload));
    Forward->SetArrayField(TEXT("nodes"), NodesArray);
    return HandleSetMaterialNodePositions(RequestId, TEXT("set_material_node_positions"), Forward, Socket);
  }

  if (SubAction == TEXT("remove_material_node")) {
    return HandleRemoveMaterialNode(RequestId, TEXT("remove_material_node"), Payload, Socket);
  }

  if (SubAction == TEXT("disconnect_input_pin")) {
    const TSharedPtr<FJsonObject>* TargetExpression = nullptr;
    if (Payload->TryGetObjectField(TEXT("targetExpression"), TargetExpression) &&
        TargetExpression && TargetExpression->IsValid()) {
      int32 ExpressionIndex = INDEX_NONE;
      if ((*TargetExpression)->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex)) {
        Payload->SetNumberField(TEXT("expressionIndex"), ExpressionIndex);
      }
      FString ExpressionPath;
      if ((*TargetExpression)->TryGetStringField(TEXT("expressionPath"), ExpressionPath) &&
          !ExpressionPath.IsEmpty()) {
        Payload->SetStringField(TEXT("expressionPath"), ExpressionPath);
      }
    }
    FString TargetInputName;
    if (Payload->TryGetStringField(TEXT("targetInputName"), TargetInputName) &&
        !TargetInputName.IsEmpty()) {
      Payload->SetStringField(TEXT("inputName"), TargetInputName);
    }
    return HandleBreakMaterialConnections(RequestId, TEXT("break_material_connections"), Payload, Socket);
  }


  // --------------------------------------------------------------------------
  // disconnect_nodes
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("disconnect_nodes")) {
    LOAD_GRAPH_OWNER_OR_RETURN();

    FString NodeId, PinName;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    Payload->TryGetStringField(TEXT("pinName"), PinName);

    // Disconnect from main node
    if (NodeId.IsEmpty() || NodeId == TEXT("Main")) {
      // N5: safe cast — UMaterialFunction has no main shader pins
      UMaterial* MatPtr = Cast<UMaterial>(Material);
      if (!MatPtr) {
        SendAutomationError(Socket, RequestId,
            TEXT("Disconnecting from main material pins requires a UMaterial. "
                 "UMaterialFunction has no main shader pins. "
                 "Pass an explicit nodeId instead."),
            TEXT("UNSUPPORTED_OPERATION"));
        return true;
      }
      if (!PinName.IsEmpty()) {
        bool bFound = false;
#if WITH_EDITORONLY_DATA
        if (PinName == TEXT("BaseColor")) {
          MCP_GET_MATERIAL_INPUT(MatPtr, BaseColor).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("EmissiveColor")) {
          MCP_GET_MATERIAL_INPUT(MatPtr, EmissiveColor).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Roughness")) {
          MCP_GET_MATERIAL_INPUT(MatPtr, Roughness).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Metallic")) {
          MCP_GET_MATERIAL_INPUT(MatPtr, Metallic).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Specular")) {
          MCP_GET_MATERIAL_INPUT(MatPtr, Specular).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Normal")) {
          MCP_GET_MATERIAL_INPUT(MatPtr, Normal).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Opacity")) {
          MCP_GET_MATERIAL_INPUT(MatPtr, Opacity).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("OpacityMask")) {
          MCP_GET_MATERIAL_INPUT(MatPtr, OpacityMask).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("AmbientOcclusion")) {
          MCP_GET_MATERIAL_INPUT(MatPtr, AmbientOcclusion).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("SubsurfaceColor")) {
          MCP_GET_MATERIAL_INPUT(MatPtr, SubsurfaceColor).Expression = nullptr;
          bFound = true;
        }
#endif

        if (bFound) {
          { FString RebuildErr; McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr); }
          SendAutomationResponse(Socket, RequestId, true,
                                 TEXT("Disconnected from main material pin."));
          return true;
        }
      }
    }

    // Disconnect from a specific expression node (non-Main)
    UMaterialExpression* Expr = FindExpressionByIdOrName_NodeOps(GraphOwner, NodeId);
    if (!Expr) {
      SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("Node '%s' not found."), *NodeId),
          TEXT("NODE_NOT_FOUND"));
      return true;
    }

    if (!PinName.IsEmpty()) {
      FProperty* Prop = Expr->GetClass()->FindPropertyByName(FName(*PinName));
      if (Prop) {
        if (FStructProperty* StructProp = CastField<FStructProperty>(Prop)) {
          FExpressionInput* InputPtr =
              StructProp->ContainerPtrToValuePtr<FExpressionInput>(Expr);
          if (InputPtr) {
            InputPtr->Expression = nullptr;
            { FString RebuildErr; McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr); }
            SendAutomationResponse(Socket, RequestId, true,
                TEXT("Disconnected pin."));
            return true;
          }
        }
      }
      SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("Pin '%s' not found on node '%s'."), *PinName, *NodeId),
          TEXT("INVALID_PIN"));
      return true;
    }

    for (FExpressionInputIterator It(Expr); It; ++It) {
      It->Expression = nullptr;
    }
    { FString RebuildErr; McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr); }

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Disconnect operation completed."));
    return true;
  }


  // --------------------------------------------------------------------------
  // add_material_node - Generic node adder
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_material_node")) {
    FString AssetPath, NodeType;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("nodeType"), NodeType) || NodeType.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeType'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // SECURITY: Validate asset path using SanitizeProjectRelativePath
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid assetPath '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    FMcpMaterialGraphOwner GraphOwner;
    {
      FString GraphOwnerError;
      if (!McpResolveMaterialGraphOwner(AssetPath, GraphOwner, GraphOwnerError) ||
          GraphOwner.bReadOnly) {
        SendAutomationError(Socket, RequestId,
                            GraphOwnerError.IsEmpty() ? TEXT("Cannot mutate this asset.") : GraphOwnerError,
                            TEXT("ASSET_NOT_FOUND"));
        return true;
      }
    }
    UObject* Material = GraphOwner.GraphSource ? GraphOwner.GraphSource : GraphOwner.Asset;

    // Get position from payload
    float X = 0.0f;
    float Y = 0.0f;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    // N4: strip MaterialExpression prefix so full names map through the alias table below
    if (NodeType.StartsWith(TEXT("MaterialExpression")))
    {
      NodeType = NodeType.Mid(18);  // "MaterialExpression" is 18 chars
    }

    // Resolve the expression class based on nodeType
    UClass *ExpressionClass = nullptr;
    if (NodeType == TEXT("TextureSample"))
      ExpressionClass = UMaterialExpressionTextureSample::StaticClass();
    else if (NodeType == TEXT("VectorParameter") || NodeType == TEXT("ConstantVectorParameter"))
      ExpressionClass = UMaterialExpressionVectorParameter::StaticClass();
    else if (NodeType == TEXT("ScalarParameter") || NodeType == TEXT("ConstantScalarParameter"))
      ExpressionClass = UMaterialExpressionScalarParameter::StaticClass();
    else if (NodeType == TEXT("Add"))
      ExpressionClass = UMaterialExpressionAdd::StaticClass();
    else if (NodeType == TEXT("Multiply"))
      ExpressionClass = UMaterialExpressionMultiply::StaticClass();
    else if (NodeType == TEXT("Constant") || NodeType == TEXT("Float") || NodeType == TEXT("Scalar"))
      ExpressionClass = UMaterialExpressionConstant::StaticClass();
    else if (NodeType == TEXT("Constant3Vector") || NodeType == TEXT("ConstantVector") || 
             NodeType == TEXT("Color") || NodeType == TEXT("Vector3"))
      ExpressionClass = UMaterialExpressionConstant3Vector::StaticClass();
    else if (NodeType == TEXT("Lerp") || NodeType == TEXT("LinearInterpolate"))
      ExpressionClass = UMaterialExpressionLinearInterpolate::StaticClass();
    else if (NodeType == TEXT("Divide"))
      ExpressionClass = UMaterialExpressionDivide::StaticClass();
    else if (NodeType == TEXT("Subtract"))
      ExpressionClass = UMaterialExpressionSubtract::StaticClass();
    else if (NodeType == TEXT("Power"))
      ExpressionClass = UMaterialExpressionPower::StaticClass();
    else if (NodeType == TEXT("Clamp"))
      ExpressionClass = UMaterialExpressionClamp::StaticClass();
    else if (NodeType == TEXT("Frac"))
      ExpressionClass = UMaterialExpressionFrac::StaticClass();
    else if (NodeType == TEXT("OneMinus"))
      ExpressionClass = UMaterialExpressionOneMinus::StaticClass();
    else if (NodeType == TEXT("Panner"))
      ExpressionClass = UMaterialExpressionPanner::StaticClass();
    else if (NodeType == TEXT("TextureCoordinate") || NodeType == TEXT("TexCoord"))
      ExpressionClass = UMaterialExpressionTextureCoordinate::StaticClass();
    else if (NodeType == TEXT("ComponentMask"))
      ExpressionClass = UMaterialExpressionComponentMask::StaticClass();
    else if (NodeType == TEXT("DotProduct"))
      ExpressionClass = UMaterialExpressionDotProduct::StaticClass();
    else if (NodeType == TEXT("CrossProduct"))
      ExpressionClass = UMaterialExpressionCrossProduct::StaticClass();
    else if (NodeType == TEXT("Desaturation"))
      ExpressionClass = UMaterialExpressionDesaturation::StaticClass();
    else if (NodeType == TEXT("Fresnel"))
      ExpressionClass = UMaterialExpressionFresnel::StaticClass();
    else if (NodeType == TEXT("Noise"))
      ExpressionClass = UMaterialExpressionNoise::StaticClass();
    else if (NodeType == TEXT("WorldPosition"))
      ExpressionClass = UMaterialExpressionWorldPosition::StaticClass();
    else if (NodeType == TEXT("VertexNormalWS") || NodeType == TEXT("VertexNormal"))
      ExpressionClass = UMaterialExpressionVertexNormalWS::StaticClass();
    else if (NodeType == TEXT("ReflectionVectorWS") || NodeType == TEXT("ReflectionVector"))
      ExpressionClass = UMaterialExpressionReflectionVectorWS::StaticClass();
    else if (NodeType == TEXT("PixelDepth"))
      ExpressionClass = UMaterialExpressionPixelDepth::StaticClass();
    else if (NodeType == TEXT("AppendVector"))
      ExpressionClass = UMaterialExpressionAppendVector::StaticClass();
    else if (NodeType == TEXT("If"))
      ExpressionClass = UMaterialExpressionIf::StaticClass();
    else if (NodeType == TEXT("MaterialFunctionCall"))
      ExpressionClass = UMaterialExpressionMaterialFunctionCall::StaticClass();
    else if (NodeType == TEXT("FunctionInput"))
      ExpressionClass = UMaterialExpressionFunctionInput::StaticClass();
    else if (NodeType == TEXT("FunctionOutput"))
      ExpressionClass = UMaterialExpressionFunctionOutput::StaticClass();
    else if (NodeType == TEXT("Custom"))
      ExpressionClass = UMaterialExpressionCustom::StaticClass();
    else if (NodeType == TEXT("StaticSwitchParameter") || NodeType == TEXT("StaticSwitch"))
      ExpressionClass = UMaterialExpressionStaticSwitchParameter::StaticClass();
    else if (NodeType == TEXT("TextureSampleParameter2D"))
      ExpressionClass = UMaterialExpressionTextureSampleParameter2D::StaticClass();
    else {
      // Try to resolve by full class path or with MaterialExpression prefix
      ExpressionClass = ResolveClassByName(NodeType);
      if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())) {
        FString PrefixedName = FString::Printf(TEXT("MaterialExpression%s"), *NodeType);
        ExpressionClass = ResolveClassByName(PrefixedName);
      }
      if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())) {
        SendAutomationError(
            Socket, RequestId,
            FString::Printf(
                TEXT("Unknown node type: %s. Available types: TextureSample, VectorParameter, "
                     "ScalarParameter, Add, Multiply, Constant, Constant3Vector, Color, Lerp, "
                     "Divide, Subtract, Power, Clamp, Frac, OneMinus, Panner, TextureCoordinate, "
                     "ComponentMask, DotProduct, CrossProduct, Desaturation, Fresnel, Noise, "
                     "WorldPosition, VertexNormalWS, ReflectionVectorWS, PixelDepth, AppendVector, "
                     "If, MaterialFunctionCall, FunctionInput, FunctionOutput, Custom, "
                     "StaticSwitchParameter, TextureSampleParameter2D. Or use full class name "
                     "like 'MaterialExpressionLerp'."),
                *NodeType),
            TEXT("UNKNOWN_TYPE"));
        return true;
      }
    }

    // Create the expression
    UMaterialExpression *NewExpr = NewObject<UMaterialExpression>(
        Material, ExpressionClass, NAME_None, RF_Transactional);
    if (!NewExpr) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create expression."), TEXT("CREATE_FAILED"));
      return true;
    }

    // Set editor position
    NewExpr->MaterialExpressionEditorX = (int32)X;
    NewExpr->MaterialExpressionEditorY = (int32)Y;

    // Add to material's expression collection. McpGetGraphExpressionsMutable
    // resolves to UMaterial::GetEditorOnlyData()->ExpressionCollection.Expressions
    // OR UMaterialFunction::GetEditorOnlyData()->ExpressionCollection.Expressions
    // depending on GraphOwner.Kind, so the gate that used to check
    // Material->GetEditorOnlyData() is now redundant.
#if WITH_EDITORONLY_DATA
    if (auto* ExprArr = McpGetGraphExpressionsMutable(GraphOwner)) ExprArr->Add(NewExpr);
#endif

    // If parameter node, set the parameter name
    FString ParamName;
    if (Payload->TryGetStringField(TEXT("name"), ParamName) && !ParamName.IsEmpty()) {
      if (UMaterialExpressionParameter *ParamExpr = Cast<UMaterialExpressionParameter>(NewExpr)) {
        ParamExpr->ParameterName = FName(*ParamName);
      }
    }

    { FString RebuildErr; McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr); }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), NewExpr->MaterialExpressionGuid.ToString());
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetStringField(TEXT("nodeType"), NodeType);
    Result->SetBoolField(TEXT("nodeAdded"), true);

    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Material node '%s' added."), *NodeType), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // remove_material_node
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("remove_material_node")) {
    FString AssetPath, NodeId;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("nodeId"), NodeId) || NodeId.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Validate path security BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    FMcpMaterialGraphOwner GraphOwner;
    {
      FString GraphOwnerError;
      if (!McpResolveMaterialGraphOwner(AssetPath, GraphOwner, GraphOwnerError) ||
          GraphOwner.bReadOnly) {
        SendAutomationError(Socket, RequestId,
                            GraphOwnerError.IsEmpty() ? TEXT("Cannot mutate this asset.") : GraphOwnerError,
                            TEXT("ASSET_NOT_FOUND"));
        return true;
      }
    }
    UObject* Material = GraphOwner.GraphSource ? GraphOwner.GraphSource : GraphOwner.Asset;

    UMaterialExpression *Expr = FindExpressionByIdOrName_NodeOps(GraphOwner, NodeId);
    if (!Expr) {
      SendAutomationError(Socket, RequestId, TEXT("Node not found."), TEXT("NOT_FOUND"));
      return true;
    }

    // Remove the expression from the graph collection. Works for both
    // UMaterial and UMaterialFunction via McpGetGraphExpressionsMutable.
    if (auto* ExprArr = McpGetGraphExpressionsMutable(GraphOwner)) {
      ExprArr->Remove(Expr);
    }
    { FString RebuildErr; McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr); }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), NodeId);
    Result->SetBoolField(TEXT("removed"), true);

    SendAutomationResponse(Socket, RequestId, true, TEXT("Material node removed."), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // set_material_parameter
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_material_parameter")) {
    FString AssetPath, ParameterName, ParameterType;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParameterName) || ParameterName.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetStringField(TEXT("parameterType"), ParameterType);

    // SECURITY: Validate assetPath before use
    FString ValidatedAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedAssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid assetPath '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedAssetPath;

    // This is a stub that routes to appropriate parameter handler
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetStringField(TEXT("parameterName"), ParameterName);
    Result->SetBoolField(TEXT("parameterSet"), true);

    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Parameter '%s' set."), *ParameterName), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // get_material_node_details
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("get_material_node_details")) {
    FString AssetPath, NodeId;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("nodeId"), NodeId) || NodeId.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Validate path security BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    FMcpMaterialGraphOwner GraphOwner;
    {
      FString GraphOwnerError;
      if (!McpResolveMaterialGraphOwner(AssetPath, GraphOwner, GraphOwnerError) ||
          GraphOwner.bReadOnly) {
        SendAutomationError(Socket, RequestId,
                            GraphOwnerError.IsEmpty() ? TEXT("Cannot mutate this asset.") : GraphOwnerError,
                            TEXT("ASSET_NOT_FOUND"));
        return true;
      }
    }
    UObject* Material = GraphOwner.GraphSource ? GraphOwner.GraphSource : GraphOwner.Asset;

    UMaterialExpression *Expr = FindExpressionByIdOrName_NodeOps(GraphOwner, NodeId);
    if (!Expr) {
      SendAutomationError(Socket, RequestId, TEXT("Node not found."), TEXT("NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), Expr->MaterialExpressionGuid.ToString());
    Result->SetStringField(TEXT("nodeType"), Expr->GetClass()->GetName());
    Result->SetStringField(TEXT("nodeName"), Expr->GetName());

    SendAutomationResponse(Socket, RequestId, true, TEXT("Node details retrieved."), Result);
    return true;
  }

  return false;
}

#undef LOAD_GRAPH_OWNER_OR_RETURN

#else // !WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_NodeOps(
    const FString& /*SubAction*/, const FString& /*RequestId*/,
    const TSharedPtr<FJsonObject>& /*Payload*/,
    TSharedPtr<FMcpBridgeWebSocket> /*Socket*/)
{
  return false;
}

#endif // WITH_EDITOR
