/**
 * McpAutomationBridge_MaterialAuthoring_SpecializedMaterials.cpp
 *
 * Phase 8: Material Authoring - specialized material sub-actions.
 *
 * Decomposed from McpAutomationBridge_MaterialAuthoringHandlers.cpp.
 * Handles specialized material creation and layer sub-actions.
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

static bool SaveMaterialAsset_SpecializedMaterials(UMaterial *Material) {
  if (!Material)
    return false;

  // Use McpSafeAssetSave for proper asset registry notification
  return McpSafeAssetSave(Material);
}

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_SpecializedMaterials(
    const FString& SubAction, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{

  // ==========================================================================
  // 8.5 Specialized Materials
  // ==========================================================================

  // --------------------------------------------------------------------------
  // create_landscape_material, create_decal_material, create_post_process_material
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("create_landscape_material") ||
      SubAction == TEXT("create_decal_material") ||
      SubAction == TEXT("create_post_process_material")) {
    FString Name, Path;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'name'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Path = GetJsonStringField(Payload, TEXT("path"));
    if (Path.IsEmpty()) {
      Path = TEXT("/Game/Materials");
    }

    // Name validation - sanitize and check for invalid characters
    FString OriginalName = Name;
    FString SanitizedName = SanitizeAssetName(Name);
    FString NormalizedOriginal = OriginalName.Replace(TEXT("_"), TEXT(""));
    FString NormalizedSanitized = SanitizedName.Replace(TEXT("_"), TEXT(""));
    if (NormalizedSanitized != NormalizedOriginal) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid asset name '%s'. Names can only contain alphanumeric characters and underscores."), *OriginalName),
                          TEXT("INVALID_NAME"));
      return true;
    }
    Name = SanitizedName;

    // Path validation - check for traversal and normalize
    FString ValidatedPath;
    FString PathError;
    if (!ValidateAssetCreationPath(Path, Name, ValidatedPath, PathError)) {
      SendAutomationError(Socket, RequestId, PathError, TEXT("INVALID_PATH"));
      return true;
    }
    Path = ValidatedPath;

    // Check for existing asset collision (different class)
    FString FullAssetPath = Path + TEXT(".") + Name;
    if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath)) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Asset already exists at path: %s"), *FullAssetPath),
                          TEXT("ASSET_EXISTS"));
      return true;
    }

    // Create material using factory
    UMaterialFactoryNew *Factory = NewObject<UMaterialFactoryNew>();
    UPackage *Package = CreatePackage(*Path);
    if (!Package) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create package."),
                          TEXT("PACKAGE_ERROR"));
      return true;
    }

    UMaterial *NewMaterial = Cast<UMaterial>(
        Factory->FactoryCreateNew(UMaterial::StaticClass(), Package,
                                  FName(*Name), RF_Public | RF_Standalone,
                                  nullptr, GWarn));
    if (!NewMaterial) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create material."),
                          TEXT("CREATE_FAILED"));
      return true;
    }

    // Set domain based on type
    if (SubAction == TEXT("create_landscape_material")) {
      // Landscape materials use Surface domain but typically have special setup
      NewMaterial->MaterialDomain = EMaterialDomain::MD_Surface;
      NewMaterial->BlendMode = EBlendMode::BLEND_Opaque;
    } else if (SubAction == TEXT("create_decal_material")) {
      NewMaterial->MaterialDomain = EMaterialDomain::MD_DeferredDecal;
      NewMaterial->BlendMode = EBlendMode::BLEND_Translucent;
    } else if (SubAction == TEXT("create_post_process_material")) {
      NewMaterial->MaterialDomain = EMaterialDomain::MD_PostProcess;
      NewMaterial->BlendMode = EBlendMode::BLEND_Opaque;
    }

    NewMaterial->PostEditChange();
    NewMaterial->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset_SpecializedMaterials(NewMaterial);
    }

    FAssetRegistryModule::AssetCreated(NewMaterial);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, NewMaterial);
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Material '%s' created."), *Name),
                           Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // add_landscape_layer, configure_layer_blend
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_landscape_layer")) {
#if MCP_HAS_LANDSCAPE_LAYER
    FString LayerName;
    if (!Payload->TryGetStringField(TEXT("layerName"), LayerName) || LayerName.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'layerName'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    // Accept path via multiple parameter names (assetPath, materialPath, or path)
    FString Path;
    if (Payload->TryGetStringField(TEXT("assetPath"), Path) && !Path.IsEmpty()) {
      // Use assetPath
    } else if (Payload->TryGetStringField(TEXT("materialPath"), Path) && !Path.IsEmpty()) {
      // Use materialPath
    } else if (Payload->TryGetStringField(TEXT("path"), Path) && !Path.IsEmpty()) {
      // Use path
    } else {
      Path = TEXT("/Game/Landscape/Layers");
    }
    
    // Validate path security - reject traversal and invalid paths
    FString ValidatedPath = SanitizeProjectRelativePath(Path);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid characters"), *Path),
                          TEXT("INVALID_PATH"));
      return true;
    }
    Path = ValidatedPath;
    
    // Validate the full package path
    FString PackagePath = Path / LayerName;
    if (!FPackageName::IsValidLongPackageName(PackagePath)) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid package path: %s"), *PackagePath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    
    // Create the landscape layer info asset
    FString PackageName = PackagePath;
    UPackage* Package = CreatePackage(*PackageName);
    if (!Package) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create package."), TEXT("PACKAGE_ERROR"));
      return true;
    }
    
    ULandscapeLayerInfoObject* LayerInfo = NewObject<ULandscapeLayerInfoObject>(
        Package, FName(*LayerName), RF_Public | RF_Standalone);
    
    if (!LayerInfo) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create layer info."), TEXT("CREATION_ERROR"));
      return true;
    }
    
    // Set layer name
PRAGMA_DISABLE_DEPRECATION_WARNINGS
    LayerInfo->LayerName = FName(*LayerName);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
    
    // Set optional properties
    double Hardness = 0.5;
    if (Payload->TryGetNumberField(TEXT("hardness"), Hardness)) {
PRAGMA_DISABLE_DEPRECATION_WARNINGS
      LayerInfo->Hardness = static_cast<float>(Hardness);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
    }
    
    // Set physical material if provided
    FString PhysMaterialPath;
    if (Payload->TryGetStringField(TEXT("physicalMaterialPath"), PhysMaterialPath) && !PhysMaterialPath.IsEmpty()) {
      // SECURITY: Validate physicalMaterialPath before loading
      FString ValidatedPhysMatPath = SanitizeProjectRelativePath(PhysMaterialPath);
      if (ValidatedPhysMatPath.IsEmpty()) {
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Invalid physicalMaterialPath '%s': contains traversal sequences or invalid root"), *PhysMaterialPath),
                            TEXT("INVALID_PATH"));
        return true;
      }
      PhysMaterialPath = ValidatedPhysMatPath;

      UPhysicalMaterial* PhysMat = LoadObject<UPhysicalMaterial>(nullptr, *PhysMaterialPath);
      if (PhysMat) {
PRAGMA_DISABLE_DEPRECATION_WARNINGS
        LayerInfo->PhysMaterial = PhysMat;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
      }
    }
    
#if WITH_EDITORONLY_DATA
    // Set blend method if specified (replaces bNoWeightBlend)
    bool bNoWeightBlend = false;
    if (Payload->TryGetBoolField(TEXT("noWeightBlend"), bNoWeightBlend)) {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 7
      // UE 5.7+: Use SetBlendMethod with ELandscapeTargetLayerBlendMethod
      LayerInfo->SetBlendMethod(bNoWeightBlend ? ELandscapeTargetLayerBlendMethod::None : ELandscapeTargetLayerBlendMethod::FinalWeightBlending, false);
#else
      // UE 5.0-5.6: Use direct bNoWeightBlend property
      LayerInfo->bNoWeightBlend = bNoWeightBlend;
#endif
    }
#endif
    
    // Save the asset
    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      FString AssetPathStr = LayerInfo->GetPathName();
      int32 DotIndex = AssetPathStr.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
      if (DotIndex != INDEX_NONE) { AssetPathStr.LeftInline(DotIndex); }
      LayerInfo->MarkPackageDirty();
    }
    
    // Notify asset registry
    FAssetRegistryModule::AssetCreated(LayerInfo);
    
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, LayerInfo);
    Result->SetStringField(TEXT("layerName"), LayerName);
    
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Landscape layer '%s' created."), *LayerName),
                           Result);
    return true;
#else
    SendAutomationError(Socket, RequestId, TEXT("Landscape module not available."), TEXT("NOT_SUPPORTED"));
    return true;
#endif
  }

  
  if (SubAction == TEXT("configure_layer_blend")) {
    // Configure layer blend by adding layer weight parameters and blend setup
    FString AssetPath;
    // Accept both assetPath and materialPath as parameter names
    if (Payload->TryGetStringField(TEXT("assetPath"), AssetPath) && !AssetPath.IsEmpty()) {
      // Use assetPath
    } else if (Payload->TryGetStringField(TEXT("materialPath"), AssetPath) && !AssetPath.IsEmpty()) {
      // Use materialPath
    } else {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath' or 'materialPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    // SECURITY: Validate path BEFORE loading asset
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
    
    // Parse layers array
    const TArray<TSharedPtr<FJsonValue>> *LayersArray;
    if (!Payload->TryGetArrayField(TEXT("layers"), LayersArray) ||
        LayersArray->Num() == 0) {
      SendAutomationError(Socket, RequestId, TEXT("Missing or empty 'layers' array."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    TArray<FString> CreatedNodeIds;
    int32 BaseX = 0, BaseY = 0;
    Payload->TryGetNumberField(TEXT("x"), BaseX);
    Payload->TryGetNumberField(TEXT("y"), BaseY);
    
    // For each layer, create a scalar parameter for layer weight
    for (int32 i = 0; i < LayersArray->Num(); ++i) {
      const TSharedPtr<FJsonObject> *LayerObj;
      if (!(*LayersArray)[i]->TryGetObject(LayerObj)) {
        continue;
      }
      
      FString LayerName;
      if (!(*LayerObj)->TryGetStringField(TEXT("name"), LayerName) ||
          LayerName.IsEmpty()) {
        continue;
      }
      
      FString BlendType;
      (*LayerObj)->TryGetStringField(TEXT("blendType"), BlendType);
      
      // Create scalar parameter for layer weight
      UMaterialExpressionScalarParameter *WeightParam =
          NewObject<UMaterialExpressionScalarParameter>(
              Material, UMaterialExpressionScalarParameter::StaticClass(),
              NAME_None, RF_Transactional);
      
      WeightParam->ParameterName = FName(*LayerName);
      WeightParam->DefaultValue = (i == 0) ? 1.0f : 0.0f; // First layer enabled by default
      WeightParam->MaterialExpressionEditorX = BaseX;
      WeightParam->MaterialExpressionEditorY = BaseY + (i * 150);
      
#if WITH_EDITORONLY_DATA
      MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(WeightParam);
#endif
      
      CreatedNodeIds.Add(WeightParam->MaterialExpressionGuid.ToString());
    }
    
    Material->PostEditChange();
    Material->MarkPackageDirty();
    
    // Save if requested
    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset_SpecializedMaterials(Material);
    }
    
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetNumberField(TEXT("layerCount"), CreatedNodeIds.Num());
    
    TArray<TSharedPtr<FJsonValue>> NodeIdArray;
    for (const FString &NodeId : CreatedNodeIds) {
      NodeIdArray.Add(MakeShared<FJsonValueString>(NodeId));
    }
    Result->SetArrayField(TEXT("nodeIds"), NodeIdArray);
    
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Layer blend configured with %d layers."),
                                          CreatedNodeIds.Num()),
                           Result);
    return true;
  }

  return false;
}

#else // !WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_SpecializedMaterials(
    const FString& /*SubAction*/, const FString& /*RequestId*/,
    const TSharedPtr<FJsonObject>& /*Payload*/,
    TSharedPtr<FMcpBridgeWebSocket> /*Socket*/)
{
  return false;
}

#endif // WITH_EDITOR
