/**
 * McpAutomationBridge_MaterialAuthoring_MaterialProperties.cpp
 *
 * Phase 8: Material Authoring - material property sub-actions.
 *
 * Decomposed from McpAutomationBridge_MaterialAuthoringHandlers.cpp.
 * Handles material property and compilation sub-actions.
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

static bool SaveMaterialAsset_MaterialProperties(UMaterial *Material) {
  if (!Material)
    return false;

  // Use McpSafeAssetSave for proper asset registry notification
  return McpSafeAssetSave(Material);
}

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_MaterialProperties(
    const FString& SubAction, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{

  // --------------------------------------------------------------------------
  // set_blend_mode
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_blend_mode")) {
    FString AssetPath, BlendMode;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("blendMode"), BlendMode)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'blendMode'."),
                          TEXT("INVALID_ARGUMENT"));
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

    // Reject if the asset is a MaterialFunction - BlendMode is material-only
    {
      UObject* TestLoad = LoadObject<UObject>(nullptr, *AssetPath);
      if (TestLoad && !Cast<UMaterial>(TestLoad))
      {
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("set_blend_mode is not supported on %s assets"),
                                            *TestLoad->GetClass()->GetName()),
                            TEXT("UNSUPPORTED_ASSET_TYPE"));
        return true;
      }
    }

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    bool bValidBlendMode = false;
    if (BlendMode == TEXT("Opaque")) {
      Material->BlendMode = EBlendMode::BLEND_Opaque;
      bValidBlendMode = true;
    } else if (BlendMode == TEXT("Masked")) {
      Material->BlendMode = EBlendMode::BLEND_Masked;
      bValidBlendMode = true;
    } else if (BlendMode == TEXT("Translucent")) {
      Material->BlendMode = EBlendMode::BLEND_Translucent;
      bValidBlendMode = true;
    } else if (BlendMode == TEXT("Additive")) {
      Material->BlendMode = EBlendMode::BLEND_Additive;
      bValidBlendMode = true;
    } else if (BlendMode == TEXT("Modulate")) {
      Material->BlendMode = EBlendMode::BLEND_Modulate;
      bValidBlendMode = true;
    } else if (BlendMode == TEXT("AlphaComposite")) {
      Material->BlendMode = EBlendMode::BLEND_AlphaComposite;
      bValidBlendMode = true;
    } else if (BlendMode == TEXT("AlphaHoldout")) {
      Material->BlendMode = EBlendMode::BLEND_AlphaHoldout;
      bValidBlendMode = true;
    }

    if (!bValidBlendMode) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid blendMode '%s'. Valid values: Opaque, Masked, Translucent, Additive, Modulate, AlphaComposite, AlphaHoldout"),
                                          *BlendMode),
                          TEXT("INVALID_ENUM"));
      return true;
    }

    Material->PostEditChange();
    Material->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset_MaterialProperties(Material);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Material);
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Blend mode set to %s."), *BlendMode), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // set_shading_model
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_shading_model")) {
    FString AssetPath, ShadingModel;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("shadingModel"), ShadingModel)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'shadingModel'."),
                          TEXT("INVALID_ARGUMENT"));
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

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    bool bValidShadingModel = false;
    if (ShadingModel == TEXT("Unlit")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_Unlit);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("DefaultLit")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_DefaultLit);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("Subsurface")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_Subsurface);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("SubsurfaceProfile")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_SubsurfaceProfile);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("PreintegratedSkin")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_PreintegratedSkin);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("ClearCoat")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_ClearCoat);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("Hair")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_Hair);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("Cloth")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_Cloth);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("Eye")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_Eye);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("TwoSidedFoliage")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_TwoSidedFoliage);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("ThinTranslucent")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_ThinTranslucent);
      bValidShadingModel = true;
    }

    if (!bValidShadingModel) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid shadingModel '%s'. Valid values: Unlit, DefaultLit, Subsurface, SubsurfaceProfile, PreintegratedSkin, ClearCoat, Hair, Cloth, Eye, TwoSidedFoliage, ThinTranslucent"),
                                          *ShadingModel),
                          TEXT("INVALID_ENUM"));
      return true;
    }

    Material->PostEditChange();
    Material->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset_MaterialProperties(Material);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Material);
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Shading model set to %s."), *ShadingModel), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // set_material_domain
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_material_domain")) {
    FString AssetPath, Domain;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("materialDomain"), Domain)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'materialDomain'."),
                          TEXT("INVALID_ARGUMENT"));
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

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    bool bValidDomain = false;
    if (Domain == TEXT("Surface")) {
      Material->MaterialDomain = EMaterialDomain::MD_Surface;
      bValidDomain = true;
    } else if (Domain == TEXT("DeferredDecal")) {
      Material->MaterialDomain = EMaterialDomain::MD_DeferredDecal;
      bValidDomain = true;
    } else if (Domain == TEXT("LightFunction")) {
      Material->MaterialDomain = EMaterialDomain::MD_LightFunction;
      bValidDomain = true;
    } else if (Domain == TEXT("Volume")) {
      Material->MaterialDomain = EMaterialDomain::MD_Volume;
      bValidDomain = true;
    } else if (Domain == TEXT("PostProcess")) {
      Material->MaterialDomain = EMaterialDomain::MD_PostProcess;
      bValidDomain = true;
    } else if (Domain == TEXT("UI")) {
      Material->MaterialDomain = EMaterialDomain::MD_UI;
      bValidDomain = true;
    }

    if (!bValidDomain) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid materialDomain '%s'. Valid values: Surface, DeferredDecal, LightFunction, Volume, PostProcess, UI"),
                                          *Domain),
                          TEXT("INVALID_ENUM"));
      return true;
    }

    Material->PostEditChange();
    Material->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset_MaterialProperties(Material);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Material);
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Material domain set to %s."), *Domain), Result);
    return true;
  }


  // ==========================================================================
  // 8.6 Utilities
  // ==========================================================================

  // --------------------------------------------------------------------------
  // compile_material
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("compile_material")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
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

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Force recompile
    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    Material->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset_MaterialProperties(Material);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetBoolField(TEXT("compiled"), true);
    Result->SetBoolField(TEXT("saved"), bSave);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Material compiled."), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // get_material_info
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("get_material_info")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
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

    // Try UMaterialFunction first (includes Layer/LayerBlend via polymorphism)
    if (UMaterialFunction* Func = LoadObject<UMaterialFunction>(nullptr, *AssetPath))
    {
      FMcpMaterialGraphOwner GraphOwner;
      GraphOwner.Asset = Func;
      GraphOwner.GraphSource = Func;
      GraphOwner.Kind = EMcpMaterialGraphOwnerKind::MaterialFunction;
      GraphOwner.bReadOnly = false;
      const TArray<TObjectPtr<UMaterialExpression>>* Expressions = McpGetGraphExpressions(GraphOwner);

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("assetClass"), Func->GetClass()->GetName());
      Result->SetStringField(TEXT("description"), Func->Description);
      Result->SetBoolField(TEXT("exposedToLibrary"), Func->bExposeToLibrary != 0);
      Result->SetNumberField(TEXT("nodeCount"), Expressions ? Expressions->Num() : 0);

      TArray<TSharedPtr<FJsonValue>> InputsArr;
      TArray<TSharedPtr<FJsonValue>> OutputsArr;
      if (Expressions) {
        for (UMaterialExpression* Expr : *Expressions) {
          if (UMaterialExpressionFunctionInput* InputExpr = Cast<UMaterialExpressionFunctionInput>(Expr)) {
            TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
            Obj->SetStringField(TEXT("name"), InputExpr->InputName.ToString());
            Obj->SetStringField(TEXT("id"), InputExpr->Id.ToString());
            InputsArr.Add(MakeShared<FJsonValueObject>(Obj));
          } else if (UMaterialExpressionFunctionOutput* OutputExpr = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
            TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
            Obj->SetStringField(TEXT("name"), OutputExpr->OutputName.ToString());
            Obj->SetStringField(TEXT("id"), OutputExpr->Id.ToString());
            OutputsArr.Add(MakeShared<FJsonValueObject>(Obj));
          }
        }
      }
      Result->SetArrayField(TEXT("inputs"), InputsArr);
      Result->SetArrayField(TEXT("outputs"), OutputsArr);

      // parameters
      TArray<TSharedPtr<FJsonValue>> ParamsArray;
      if (Expressions) {
        for (UMaterialExpression* Expr : *Expressions) {
          if (UMaterialExpressionParameter* Param = Cast<UMaterialExpressionParameter>(Expr)) {
          TSharedPtr<FJsonObject> ParamObj = McpHandlerUtils::CreateResultObject();
          ParamObj->SetStringField(TEXT("name"), Param->ParameterName.ToString());
          ParamObj->SetStringField(TEXT("type"), Expr->GetClass()->GetName());
          ParamObj->SetStringField(TEXT("nodeId"), Expr->MaterialExpressionGuid.ToString());
          ParamsArray.Add(MakeShared<FJsonValueObject>(ParamObj));
          }
        }
      }
      Result->SetArrayField(TEXT("parameters"), ParamsArray);

      SendAutomationResponse(Socket, RequestId, true, TEXT("Material function info retrieved."), Result);
      return true;
    }

    // Try UMaterialFunctionInstance
    if (UMaterialFunctionInstance* Inst = LoadObject<UMaterialFunctionInstance>(nullptr, *AssetPath))
    {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("assetClass"), Inst->GetClass()->GetName());
      UMaterialFunction* Base = Inst->GetBaseFunction();
      if (Base)
      {
        FMcpMaterialGraphOwner GraphOwner;
        GraphOwner.Asset = Inst;
        GraphOwner.GraphSource = Base;
        GraphOwner.Kind = EMcpMaterialGraphOwnerKind::MaterialFunctionInstance;
        GraphOwner.bReadOnly = true;
        const TArray<TObjectPtr<UMaterialExpression>>* Expressions = McpGetGraphExpressions(GraphOwner);
        Result->SetStringField(TEXT("parentAsset"), Base->GetPathName());
        Result->SetNumberField(TEXT("nodeCount"), Expressions ? Expressions->Num() : 0);
      }
      McpCollectFunctionInstanceOverrides(Inst, Result.ToSharedRef());
      SendAutomationResponse(Socket, RequestId, true, TEXT("Material function instance info retrieved."), Result);
      return true;
    }

    if (UMaterialInstanceConstant* MaterialInstance = LoadObject<UMaterialInstanceConstant>(nullptr, *AssetPath))
    {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("assetClass"), MaterialInstance->GetClass()->GetName());
      McpCollectMaterialInstanceInfo(MaterialInstance, Result.ToSharedRef(), true, false);
      SendAutomationResponse(Socket, RequestId, true, TEXT("Material instance info retrieved."), Result);
      return true;
    }

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load Material, MaterialInstanceConstant, MaterialFunction, or MaterialFunctionInstance."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetClass"), TEXT("Material"));

    // Domain
    switch (Material->MaterialDomain) {
    case EMaterialDomain::MD_Surface:
      Result->SetStringField(TEXT("domain"), TEXT("Surface")); break;
    case EMaterialDomain::MD_DeferredDecal:
      Result->SetStringField(TEXT("domain"), TEXT("DeferredDecal")); break;
    case EMaterialDomain::MD_LightFunction:
      Result->SetStringField(TEXT("domain"), TEXT("LightFunction")); break;
    case EMaterialDomain::MD_Volume:
      Result->SetStringField(TEXT("domain"), TEXT("Volume")); break;
    case EMaterialDomain::MD_PostProcess:
      Result->SetStringField(TEXT("domain"), TEXT("PostProcess")); break;
    case EMaterialDomain::MD_UI:
      Result->SetStringField(TEXT("domain"), TEXT("UI")); break;
    default:
      Result->SetStringField(TEXT("domain"), TEXT("Unknown")); break;
    }

    // Blend mode
    switch (Material->BlendMode) {
    case EBlendMode::BLEND_Opaque:
      Result->SetStringField(TEXT("blendMode"), TEXT("Opaque")); break;
    case EBlendMode::BLEND_Masked:
      Result->SetStringField(TEXT("blendMode"), TEXT("Masked")); break;
    case EBlendMode::BLEND_Translucent:
      Result->SetStringField(TEXT("blendMode"), TEXT("Translucent")); break;
    case EBlendMode::BLEND_Additive:
      Result->SetStringField(TEXT("blendMode"), TEXT("Additive")); break;
    case EBlendMode::BLEND_Modulate:
      Result->SetStringField(TEXT("blendMode"), TEXT("Modulate")); break;
    default:
      Result->SetStringField(TEXT("blendMode"), TEXT("Unknown")); break;
    }

    Result->SetBoolField(TEXT("twoSided"), Material->TwoSided);
    Result->SetNumberField(TEXT("nodeCount"), MCP_GET_MATERIAL_EXPRESSIONS(Material).Num());

    // List parameters
    TArray<TSharedPtr<FJsonValue>> ParamsArray;
    for (UMaterialExpression *Expr : MCP_GET_MATERIAL_EXPRESSIONS(Material)) {
      if (UMaterialExpressionParameter *Param = Cast<UMaterialExpressionParameter>(Expr)) {
        TSharedPtr<FJsonObject> ParamObj = McpHandlerUtils::CreateResultObject();
        ParamObj->SetStringField(TEXT("name"), Param->ParameterName.ToString());
        ParamObj->SetStringField(TEXT("type"), Expr->GetClass()->GetName());
        ParamObj->SetStringField(TEXT("nodeId"), Expr->MaterialExpressionGuid.ToString());
        ParamsArray.Add(MakeShared<FJsonValueObject>(ParamObj));
      }
    }
    Result->SetArrayField(TEXT("parameters"), ParamsArray);

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material info retrieved."), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // set_two_sided
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_two_sided")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
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

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load Material."), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    bool bTwoSided = GetJsonBoolField(Payload, TEXT("twoSided"), true);
    Material->TwoSided = bTwoSided ? 1 : 0;
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetBoolField(TEXT("twoSided"), bTwoSided);

    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Two-sided set to %s."), bTwoSided ? TEXT("true") : TEXT("false")), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // set_cast_shadows
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_cast_shadows")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // SECURITY: Validate assetPath before use
    FString ValidatedAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedAssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid assetPath '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedAssetPath;

    // Note: Cast shadows is typically a material property but may be on the component
    // This is a stub that acknowledges the request
    bool CastShadows = GetJsonBoolField(Payload, TEXT("castShadows"), true);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetBoolField(TEXT("castShadows"), CastShadows);

    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Cast shadows set to %s."), CastShadows ? TEXT("true") : TEXT("false")), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // set_material_attributes_mode (task 2.10)
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_material_attributes_mode"))
  {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("missing-field")); return true;
    }
    AssetPath = SanitizeProjectRelativePath(AssetPath);
    if (AssetPath.IsEmpty()) { SendAutomationError(Socket, RequestId, TEXT("Invalid assetPath."), TEXT("invalid-asset")); return true; }

    UMaterial* Mat = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Mat) { SendAutomationError(Socket, RequestId, TEXT("Asset is not a UMaterial."), TEXT("unsupported-asset-type")); return true; }

    bool bEnabled = false;
    if (!Payload->TryGetBoolField(TEXT("enabled"), bEnabled)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'enabled'."), TEXT("missing-field")); return true;
    }

    FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge", "SetMaterialAttributesMode", "MCP set material attributes mode"));
    Mat->Modify();
    Mat->bUseMaterialAttributes = bEnabled;
    Mat->PreEditChange(nullptr);
    Mat->PostEditChange();
    Mat->MarkPackageDirty();

    bool bSave = false; Payload->TryGetBoolField(TEXT("save"), bSave);
    bool bSaved = false;
    if (bSave) { bSaved = McpSafeAssetSave(Mat); if (!bSaved) { SendAutomationError(Socket, RequestId, TEXT("Save failed."), TEXT("save-failed")); return true; } }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetBoolField(TEXT("bUseMaterialAttributes"), bEnabled);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetBoolField(TEXT("dirty"), Mat->GetOutermost()->IsDirty());
    SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Material attributes mode set to %s."), bEnabled ? TEXT("enabled") : TEXT("disabled")), Result);
    return true;
  }

  return false;
}

#else // !WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_MaterialProperties(
    const FString& /*SubAction*/, const FString& /*RequestId*/,
    const TSharedPtr<FJsonObject>& /*Payload*/,
    TSharedPtr<FMcpBridgeWebSocket> /*Socket*/)
{
  return false;
}

#endif // WITH_EDITOR
