/**
 * McpAutomationBridge_MaterialAuthoring_AdvancedNodes.cpp
 *
 * Phase 8: Material Authoring - advanced node sub-actions.
 *
 * Decomposed from McpAutomationBridge_MaterialAuthoringHandlers.cpp.
 * Handles advanced expression node sub-actions.
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
//   UObject* Material      // alias for backward-compat: points at the graph-source
//                          // UObject (UMaterial or UMaterialFunction). NewObject(),
//                          // PostEditChange() and MarkPackageDirty() all work via
//                          // virtual dispatch on UObject*. For expression-collection
//                          // access use McpGetGraphExpressionsMutable(GraphOwner)
//                          // (NOT MCP_GET_MATERIAL_EXPRESSIONS(Material) which only
//                          // compiles for UMaterial).
//   FString AssetPath
//   float X, Y
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

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_AdvancedNodes(
    const FString& SubAction, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{

  // --------------------------------------------------------------------------
  // add_math_node
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_math_node")) {
    LOAD_GRAPH_OWNER_OR_RETURN();

    FString Operation;
    if (!Payload->TryGetStringField(TEXT("operation"), Operation)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'operation'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterialExpression *MathNode = nullptr;
    if (Operation == TEXT("Add")) {
      MathNode = NewObject<UMaterialExpressionAdd>(
          Material, UMaterialExpressionAdd::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Subtract")) {
      MathNode = NewObject<UMaterialExpressionSubtract>(
          Material, UMaterialExpressionSubtract::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Multiply")) {
      MathNode = NewObject<UMaterialExpressionMultiply>(
          Material, UMaterialExpressionMultiply::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Divide")) {
      MathNode = NewObject<UMaterialExpressionDivide>(
          Material, UMaterialExpressionDivide::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Lerp")) {
      MathNode = NewObject<UMaterialExpressionLinearInterpolate>(
          Material, UMaterialExpressionLinearInterpolate::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Clamp")) {
      MathNode = NewObject<UMaterialExpressionClamp>(
          Material, UMaterialExpressionClamp::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Power")) {
      MathNode = NewObject<UMaterialExpressionPower>(
          Material, UMaterialExpressionPower::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Frac")) {
      MathNode = NewObject<UMaterialExpressionFrac>(
          Material, UMaterialExpressionFrac::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("OneMinus")) {
      MathNode = NewObject<UMaterialExpressionOneMinus>(
          Material, UMaterialExpressionOneMinus::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Append")) {
      MathNode = NewObject<UMaterialExpressionAppendVector>(
          Material, UMaterialExpressionAppendVector::StaticClass(), NAME_None,
          RF_Transactional);
    } else {
      SendAutomationError(
          Socket, RequestId,
          FString::Printf(TEXT("Unknown operation: %s"), *Operation),
          TEXT("UNKNOWN_OPERATION"));
      return true;
    }

    MathNode->MaterialExpressionEditorX = (int32)X;
    MathNode->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    if (auto* ExprArr = McpGetGraphExpressionsMutable(GraphOwner)) ExprArr->Add(MathNode);
#endif

    { FString RebuildErr; McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr); }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MathNode->MaterialExpressionGuid.ToString());
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Math node '%s' added."), *Operation), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // add_world_position, add_vertex_normal, add_pixel_depth, add_fresnel,
  // add_reflection_vector, add_panner, add_rotator, add_noise, add_voronoi
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_world_position") ||
      SubAction == TEXT("add_vertex_normal") ||
      SubAction == TEXT("add_pixel_depth") || SubAction == TEXT("add_fresnel") ||
      SubAction == TEXT("add_reflection_vector") ||
      SubAction == TEXT("add_panner") || SubAction == TEXT("add_rotator") ||
      SubAction == TEXT("add_noise") || SubAction == TEXT("add_voronoi")) {
    LOAD_GRAPH_OWNER_OR_RETURN();

    UMaterialExpression *NewExpr = nullptr;
    FString NodeName;

    if (SubAction == TEXT("add_world_position")) {
      NewExpr = NewObject<UMaterialExpressionWorldPosition>(
          Material, UMaterialExpressionWorldPosition::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("WorldPosition");
    } else if (SubAction == TEXT("add_vertex_normal")) {
      NewExpr = NewObject<UMaterialExpressionVertexNormalWS>(
          Material, UMaterialExpressionVertexNormalWS::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("VertexNormalWS");
    } else if (SubAction == TEXT("add_pixel_depth")) {
      NewExpr = NewObject<UMaterialExpressionPixelDepth>(
          Material, UMaterialExpressionPixelDepth::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("PixelDepth");
    } else if (SubAction == TEXT("add_fresnel")) {
      NewExpr = NewObject<UMaterialExpressionFresnel>(
          Material, UMaterialExpressionFresnel::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("Fresnel");
    } else if (SubAction == TEXT("add_reflection_vector")) {
      NewExpr = NewObject<UMaterialExpressionReflectionVectorWS>(
          Material, UMaterialExpressionReflectionVectorWS::StaticClass(),
          NAME_None, RF_Transactional);
      NodeName = TEXT("ReflectionVectorWS");
    } else if (SubAction == TEXT("add_panner")) {
      NewExpr = NewObject<UMaterialExpressionPanner>(
          Material, UMaterialExpressionPanner::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("Panner");
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    } else if (SubAction == TEXT("add_rotator")) {
      // Use runtime class lookup to avoid GetPrivateStaticClass requirement
      // StaticClass() calls GetPrivateStaticClass() internally which isn't exported
      UClass* RotatorClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.MaterialExpressionRotator"));
      if (RotatorClass)
      {
        UObject* NewExprObj = NewObject<UObject>(Material, RotatorClass, NAME_None, RF_Transactional);
        NewExpr = static_cast<UMaterialExpressionRotator*>(NewExprObj);
      }
      NodeName = TEXT("Rotator");
#endif
    } else if (SubAction == TEXT("add_noise")) {
      NewExpr = NewObject<UMaterialExpressionNoise>(
          Material, UMaterialExpressionNoise::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("Noise");
    } else if (SubAction == TEXT("add_voronoi")) {
      // Voronoi is implemented via Noise with different settings
      UMaterialExpressionNoise *NoiseExpr =
          NewObject<UMaterialExpressionNoise>(
              Material, UMaterialExpressionNoise::StaticClass(), NAME_None,
              RF_Transactional);
      NoiseExpr->NoiseFunction = ENoiseFunction::NOISEFUNCTION_VoronoiALU;
      NewExpr = NoiseExpr;
      NodeName = TEXT("Voronoi");
    }

    if (NewExpr) {
      NewExpr->MaterialExpressionEditorX = (int32)X;
      NewExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
      if (auto* ExprArr = McpGetGraphExpressionsMutable(GraphOwner)) ExprArr->Add(NewExpr);
#endif

      { FString __RErr; McpRebuildMaterialGraphOwner(GraphOwner, __RErr); }
      Material->MarkPackageDirty();

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("nodeId"),
                             NewExpr->MaterialExpressionGuid.ToString());
      SendAutomationResponse(
          Socket, RequestId, true,
          FString::Printf(TEXT("%s node added."), *NodeName), Result);
    } else {
      // NewExpr was null - could be class lookup failure or UE < 5.1 for rotator
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Failed to create %s node."), *NodeName),
                          TEXT("CREATE_FAILED"));
    }
    return true;
  }


  // --------------------------------------------------------------------------
  // add_if, add_switch
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_if") || SubAction == TEXT("add_switch")) {
    LOAD_GRAPH_OWNER_OR_RETURN();

    UMaterialExpression *NewExpr = nullptr;
    FString NodeName;

    if (SubAction == TEXT("add_if")) {
      NewExpr = NewObject<UMaterialExpressionIf>(
          Material, UMaterialExpressionIf::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("If");
    } else {
      // Switch can be implemented via StaticSwitch or If
      NewExpr = NewObject<UMaterialExpressionIf>(
          Material, UMaterialExpressionIf::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("Switch");
    }

    NewExpr->MaterialExpressionEditorX = (int32)X;
    NewExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    if (auto* ExprArr = McpGetGraphExpressionsMutable(GraphOwner)) ExprArr->Add(NewExpr);
#endif

    { FString RebuildErr; McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr); }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           NewExpr->MaterialExpressionGuid.ToString());
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("%s node added."), *NodeName),
                           Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // add_component_mask
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_component_mask")) {
    LOAD_GRAPH_OWNER_OR_RETURN();

    bool bR = true, bG = true, bB = true, bA = false;
    Payload->TryGetBoolField(TEXT("r"), bR);
    Payload->TryGetBoolField(TEXT("g"), bG);
    Payload->TryGetBoolField(TEXT("b"), bB);
    Payload->TryGetBoolField(TEXT("a"), bA);

    UMaterialExpressionComponentMask *MaskExpr =
        NewObject<UMaterialExpressionComponentMask>(
            Material, UMaterialExpressionComponentMask::StaticClass(), NAME_None,
            RF_Transactional);
    MaskExpr->R = bR ? 1 : 0;
    MaskExpr->G = bG ? 1 : 0;
    MaskExpr->B = bB ? 1 : 0;
    MaskExpr->A = bA ? 1 : 0;
    MaskExpr->MaterialExpressionEditorX = (int32)X;
    MaskExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    if (auto* ExprArr = McpGetGraphExpressionsMutable(GraphOwner)) ExprArr->Add(MaskExpr);
#endif

    { FString RebuildErr; McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr); }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MaskExpr->MaterialExpressionGuid.ToString());
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("ComponentMask node added."), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // add_dot_product
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_dot_product")) {
    LOAD_GRAPH_OWNER_OR_RETURN();

    UMaterialExpressionDotProduct *DotExpr =
        NewObject<UMaterialExpressionDotProduct>(
            Material, UMaterialExpressionDotProduct::StaticClass(), NAME_None,
            RF_Transactional);
    DotExpr->MaterialExpressionEditorX = (int32)X;
    DotExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    if (auto* ExprArr = McpGetGraphExpressionsMutable(GraphOwner)) ExprArr->Add(DotExpr);
#endif

    { FString RebuildErr; McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr); }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           DotExpr->MaterialExpressionGuid.ToString());
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("DotProduct node added."), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // add_cross_product
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_cross_product")) {
    LOAD_GRAPH_OWNER_OR_RETURN();

    UMaterialExpressionCrossProduct *CrossExpr =
        NewObject<UMaterialExpressionCrossProduct>(
            Material, UMaterialExpressionCrossProduct::StaticClass(), NAME_None,
            RF_Transactional);
    CrossExpr->MaterialExpressionEditorX = (int32)X;
    CrossExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    if (auto* ExprArr = McpGetGraphExpressionsMutable(GraphOwner)) ExprArr->Add(CrossExpr);
#endif

    { FString RebuildErr; McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr); }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           CrossExpr->MaterialExpressionGuid.ToString());
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("CrossProduct node added."), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // add_desaturation
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_desaturation")) {
    LOAD_GRAPH_OWNER_OR_RETURN();

    UMaterialExpressionDesaturation *DesatExpr =
        NewObject<UMaterialExpressionDesaturation>(
            Material, UMaterialExpressionDesaturation::StaticClass(), NAME_None,
            RF_Transactional);
    
    // Set optional luminance factors
    const TSharedPtr<FJsonObject> *LumObj;
    if (Payload->TryGetObjectField(TEXT("luminanceFactors"), LumObj)) {
      double R = 0.3, G = 0.59, B = 0.11;
      (*LumObj)->TryGetNumberField(TEXT("r"), R);
      (*LumObj)->TryGetNumberField(TEXT("g"), G);
      (*LumObj)->TryGetNumberField(TEXT("b"), B);
      DesatExpr->LuminanceFactors = FLinearColor(R, G, B, 1.0f);
    }
    
    DesatExpr->MaterialExpressionEditorX = (int32)X;
    DesatExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    if (auto* ExprArr = McpGetGraphExpressionsMutable(GraphOwner)) ExprArr->Add(DesatExpr);
#endif

    { FString RebuildErr; McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr); }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           DesatExpr->MaterialExpressionGuid.ToString());
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Desaturation node added."), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // add_append (dedicated handler for convenience)
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_append")) {
    LOAD_GRAPH_OWNER_OR_RETURN();

    UMaterialExpressionAppendVector *AppendExpr =
        NewObject<UMaterialExpressionAppendVector>(
            Material, UMaterialExpressionAppendVector::StaticClass(), NAME_None,
            RF_Transactional);
    AppendExpr->MaterialExpressionEditorX = (int32)X;
    AppendExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    if (auto* ExprArr = McpGetGraphExpressionsMutable(GraphOwner)) ExprArr->Add(AppendExpr);
#endif

    { FString RebuildErr; McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr); }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           AppendExpr->MaterialExpressionGuid.ToString());
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Append node added."), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // add_custom_expression (task 2.6) - graph-owner aware, supports typed inputs/outputs
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_custom_expression")) {
    FString AssetPath2;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath2) &&
        !Payload->TryGetStringField(TEXT("materialPath"), AssetPath2)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("missing-field")); return true;
    }
    AssetPath2 = SanitizeProjectRelativePath(AssetPath2);
    if (AssetPath2.IsEmpty()) { SendAutomationError(Socket, RequestId, TEXT("Invalid assetPath."), TEXT("invalid-asset")); return true; }

    FString Code2;
    if (!Payload->TryGetStringField(TEXT("code"), Code2) || Code2.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'code'."), TEXT("missing-field")); return true;
    }

    FMcpMaterialGraphOwner GraphOwner2;
    FString GraphOwnerError2;
    if (!McpResolveMaterialGraphOwner(AssetPath2, GraphOwner2, GraphOwnerError2) || GraphOwner2.bReadOnly) {
      SendAutomationError(Socket, RequestId, GraphOwnerError2.IsEmpty() ? TEXT("Cannot mutate this asset.") : GraphOwnerError2, TEXT("unsupported-operation")); return true;
    }

    FScopedTransaction Transaction2(NSLOCTEXT("McpAutomationBridge", "AddCustomExpression", "MCP add custom expression"));
    GraphOwner2.Asset->Modify();

    UMaterialExpressionCustom* CustomExpr = NewObject<UMaterialExpressionCustom>(
        GraphOwner2.GraphSource ? GraphOwner2.GraphSource : GraphOwner2.Asset,
        UMaterialExpressionCustom::StaticClass(), NAME_None, RF_Transactional);
    CustomExpr->Code = Code2;

    FString OutputType2; Payload->TryGetStringField(TEXT("outputType"), OutputType2);
    if (OutputType2 == TEXT("Float2") || OutputType2 == TEXT("CMOT_Float2")) CustomExpr->OutputType = CMOT_Float2;
    else if (OutputType2 == TEXT("Float3") || OutputType2 == TEXT("CMOT_Float3")) CustomExpr->OutputType = CMOT_Float3;
    else if (OutputType2 == TEXT("Float4") || OutputType2 == TEXT("CMOT_Float4")) CustomExpr->OutputType = CMOT_Float4;
    else if (OutputType2 == TEXT("MaterialAttributes")) CustomExpr->OutputType = CMOT_MaterialAttributes;
    else CustomExpr->OutputType = CMOT_Float1;

    FString Description2; Payload->TryGetStringField(TEXT("description"), Description2);
    if (!Description2.IsEmpty()) CustomExpr->Description = Description2;

    float CX = 0, CY = 0;
    Payload->TryGetNumberField(TEXT("x"), CX); Payload->TryGetNumberField(TEXT("y"), CY);
    CustomExpr->MaterialExpressionEditorX = (int32)CX; CustomExpr->MaterialExpressionEditorY = (int32)CY;
    CustomExpr->MaterialExpressionGuid = FGuid::NewGuid();

    // Named inputs
    const TArray<TSharedPtr<FJsonValue>>* InputsArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("inputs"), InputsArr) && InputsArr) {
      CustomExpr->Inputs.Empty();
      for (const auto& InputVal : *InputsArr) {
        const TSharedPtr<FJsonObject>* InputObj = nullptr;
        if (!InputVal->TryGetObject(InputObj) || !InputObj) continue;
        FCustomInput CI;
        FString InputName; (*InputObj)->TryGetStringField(TEXT("name"), InputName);
        CI.InputName = FName(*InputName);
        CustomExpr->Inputs.Add(CI);
      }
    }

    // Additional outputs
    const TArray<TSharedPtr<FJsonValue>>* OutputsArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("additionalOutputs"), OutputsArr) && OutputsArr) {
      CustomExpr->AdditionalOutputs.Empty();
      for (const auto& OutputVal : *OutputsArr) {
        const TSharedPtr<FJsonObject>* OutputObj = nullptr;
        if (!OutputVal->TryGetObject(OutputObj) || !OutputObj) continue;
        FCustomOutput CO;
        FString OutName; (*OutputObj)->TryGetStringField(TEXT("name"), OutName);
        CO.OutputName = FName(*OutName);
        FString OutType; (*OutputObj)->TryGetStringField(TEXT("outputType"), OutType);
        if (OutType == TEXT("Float2") || OutType == TEXT("CMOT_Float2")) CO.OutputType = CMOT_Float2;
        else if (OutType == TEXT("Float3") || OutType == TEXT("CMOT_Float3")) CO.OutputType = CMOT_Float3;
        else if (OutType == TEXT("Float4") || OutType == TEXT("CMOT_Float4")) CO.OutputType = CMOT_Float4;
        else CO.OutputType = CMOT_Float1;
        CustomExpr->AdditionalOutputs.Add(CO);
      }
    }

    // Additional defines
    const TArray<TSharedPtr<FJsonValue>>* DefinesArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("additionalDefines"), DefinesArr) && DefinesArr) {
      CustomExpr->AdditionalDefines.Empty();
      for (const auto& DefVal : *DefinesArr) {
        const TSharedPtr<FJsonObject>* DefObj = nullptr;
        if (!DefVal->TryGetObject(DefObj) || !DefObj) continue;
        FCustomDefine CD;
        FString DefName, DefValue; (*DefObj)->TryGetStringField(TEXT("name"), DefName); (*DefObj)->TryGetStringField(TEXT("value"), DefValue);
        CD.DefineName = DefName; CD.DefineValue = DefValue;
        CustomExpr->AdditionalDefines.Add(CD);
      }
    }

    // Include paths
    const TArray<TSharedPtr<FJsonValue>>* IncludesArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("includeFilePaths"), IncludesArr) && IncludesArr) {
      CustomExpr->IncludeFilePaths.Empty();
      for (const auto& IncVal : *IncludesArr) {
        FString IncPath = IncVal->AsString();
        if (!IncPath.IsEmpty()) CustomExpr->IncludeFilePaths.Add(IncPath);
      }
    }

    CustomExpr->RebuildOutputs();

    TArray<TObjectPtr<UMaterialExpression>>* Exprs2 = McpGetGraphExpressionsMutable(GraphOwner2);
    if (Exprs2) Exprs2->Add(CustomExpr);

    FString RebuildErr2;
    McpRebuildMaterialGraphOwner(GraphOwner2, RebuildErr2);

    bool bSave2 = false; Payload->TryGetBoolField(TEXT("save"), bSave2);
    bool bSaved2 = false;
    if (bSave2) { bSaved2 = McpSafeAssetSave(GraphOwner2.Asset); if (!bSaved2) { SendAutomationError(Socket, RequestId, TEXT("Save failed."), TEXT("save-failed")); return true; } }

    TArray<TSharedPtr<FJsonValue>> InputNames, OutputNames;
    for (const FCustomInput& CI : CustomExpr->Inputs) InputNames.Add(MakeShared<FJsonValueString>(CI.InputName.ToString()));
    for (const FCustomOutput& CO : CustomExpr->AdditionalOutputs) OutputNames.Add(MakeShared<FJsonValueString>(CO.OutputName.ToString()));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath2);
    const int32 ExprIdx2 = Exprs2 ? (Exprs2->Num() - 1) : 0;
    Result->SetObjectField(TEXT("expressionIdentity"), McpBuildMaterialExpressionIdentity(GraphOwner2, CustomExpr, ExprIdx2));
    Result->SetStringField(TEXT("nodeId"), CustomExpr->MaterialExpressionGuid.ToString());
    Result->SetArrayField(TEXT("inputNames"), InputNames);
    Result->SetArrayField(TEXT("additionalOutputNames"), OutputNames);
    Result->SetBoolField(TEXT("saved"), bSaved2);
    Result->SetBoolField(TEXT("dirty"), GraphOwner2.Asset->GetOutermost()->IsDirty());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Custom HLSL expression added."), Result);
    return true;
  }

  return false;
}

#undef LOAD_GRAPH_OWNER_OR_RETURN

#else // !WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_AdvancedNodes(
    const FString& /*SubAction*/, const FString& /*RequestId*/,
    const TSharedPtr<FJsonObject>& /*Payload*/,
    TSharedPtr<FMcpBridgeWebSocket> /*Socket*/)
{
  return false;
}

#endif // WITH_EDITOR
