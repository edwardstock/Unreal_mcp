/**
 * McpAutomationBridge_MaterialAuthoring_MaterialCreation.cpp
 *
 * Phase 8: Material Authoring - material creation sub-actions.
 *
 * Decomposed from McpAutomationBridge_MaterialAuthoringHandlers.cpp.
 * Handles material creation sub-actions.
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

static bool SaveMaterialAsset_MaterialCreation(UMaterial* Material) {
    if (!Material)
        return false;

    // Use McpSafeAssetSave for proper asset registry notification
    return McpSafeAssetSave(Material);
}

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_MaterialCreation(
    const FString& SubAction,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {

    // ==========================================================================
    // 8.1 Material Creation Actions
    // ==========================================================================
    if (SubAction == TEXT("create_material")) {
        FString Name, Path;
        if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
            SendAutomationError(Socket,
                RequestId,
                TEXT("Missing 'name'."),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Validate and sanitize the asset name
        FString OriginalName = Name;
        FString SanitizedName = SanitizeAssetName(Name);

        // Check if sanitization significantly changed the name (indicates invalid characters)
        // If the sanitized name is different and doesn't just have underscores added/removed
        FString NormalizedOriginal = OriginalName.Replace(TEXT("_"), TEXT(""));
        FString NormalizedSanitized = SanitizedName.Replace(TEXT("_"), TEXT(""));
        if (NormalizedSanitized != NormalizedOriginal) {
            SendAutomationError(Socket,
                RequestId,
                FString::Printf(
                    TEXT("Invalid material name '%s': contains characters that cannot be used in asset names. Valid name would be: '%s'"),
                    *OriginalName,
                    *SanitizedName),
                TEXT("INVALID_NAME"));
            return true;
        }
        Name = SanitizedName;

        Path = GetJsonStringField(Payload, TEXT("path"));
        if (Path.IsEmpty()) {
            Path = TEXT("/Game/Materials");
        }

        // Validate path doesn't contain traversal sequences
        FString ValidatedPath;
        FString PathError;
        if (!ValidateAssetCreationPath(Path, Name, ValidatedPath, PathError)) {
            SendAutomationError(Socket, RequestId, PathError, TEXT("INVALID_PATH"));
            return true;
        }

        // Additional validation: reject Windows absolute paths (contain colon)
        if (ValidatedPath.Contains(TEXT(":"))) {
            SendAutomationError(Socket,
                RequestId,
                FString::Printf(TEXT("Invalid path '%s': absolute Windows paths are not allowed"), *ValidatedPath),
                TEXT("INVALID_PATH"));
            return true;
        }

        // Additional validation: verify mount point using engine API
        FText MountReason;
        if (!FPackageName::IsValidLongPackageName(ValidatedPath, true, &MountReason)) {
            SendAutomationError(Socket,
                RequestId,
                FString::Printf(TEXT("Invalid package path '%s': %s"), *ValidatedPath, *MountReason.ToString()),
                TEXT("INVALID_PATH"));
            return true;
        }

        // Validate parent folder exists
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

        FString ParentFolderPath = FPackageName::GetLongPackagePath(ValidatedPath);
        if (!AssetRegistry.PathExists(FName(*ParentFolderPath))) {
            SendAutomationError(Socket,
                RequestId,
                FString::Printf(TEXT("Parent folder does not exist: %s. Create the folder first or use an existing path."),
                    *ParentFolderPath),
                TEXT("PARENT_FOLDER_NOT_FOUND"));
            return true;
        }

        // Check for existing asset collision to prevent UE crash
        FString FullAssetPath = ValidatedPath + TEXT(".") + Name;
        if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath)) {
            UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(FullAssetPath);
            if (ExistingAsset) {
                UClass* ExistingClass = ExistingAsset->GetClass();
                FString ExistingClassName = ExistingClass ? ExistingClass->GetName() : TEXT("Unknown");
                SendAutomationError(Socket,
                    RequestId,
                    FString::Printf(TEXT("Asset '%s' already exists as %s. Cannot create Material with the same name."),
                        *FullAssetPath,
                        *ExistingClassName),
                    TEXT("ASSET_EXISTS"));
            }
            else {
                SendAutomationError(Socket,
                    RequestId,
                    FString::Printf(TEXT("Asset '%s' already exists."),
                        *FullAssetPath),
                    TEXT("ASSET_EXISTS"));
            }
            return true;
        }
        // Create material using factory - use ValidatedPath, not original Path!
        UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
        UPackage* Package = CreatePackage(*ValidatedPath);
        if (!Package) {
            SendAutomationError(Socket,
                RequestId,
                TEXT("Failed to create package."),
                TEXT("PACKAGE_ERROR"));
            return true;
        }

        UMaterial* NewMaterial = Cast<UMaterial>(
            Factory->FactoryCreateNew(UMaterial::StaticClass(),
                Package,
                FName(*Name),
                RF_Public | RF_Standalone,
                nullptr,
                GWarn));
        if (!NewMaterial) {
            SendAutomationError(Socket,
                RequestId,
                TEXT("Failed to create material."),
                TEXT("CREATE_FAILED"));
            return true;
        }

        // Set properties
        FString MaterialDomain;
        if (Payload->TryGetStringField(TEXT("materialDomain"), MaterialDomain)) {
            bool bValidMaterialDomain = false;
            if (MaterialDomain == TEXT("Surface")) {
                NewMaterial->MaterialDomain = MD_Surface;
                bValidMaterialDomain = true;
            }
            else if (MaterialDomain == TEXT("DeferredDecal")) {
                NewMaterial->MaterialDomain = MD_DeferredDecal;
                bValidMaterialDomain = true;
            }
            else if (MaterialDomain == TEXT("LightFunction")) {
                NewMaterial->MaterialDomain = MD_LightFunction;
                bValidMaterialDomain = true;
            }
            else if (MaterialDomain == TEXT("Volume")) {
                NewMaterial->MaterialDomain = MD_Volume;
                bValidMaterialDomain = true;
            }
            else if (MaterialDomain == TEXT("PostProcess")) {
                NewMaterial->MaterialDomain = MD_PostProcess;
                bValidMaterialDomain = true;
            }
            else if (MaterialDomain == TEXT("UI")) {
                NewMaterial->MaterialDomain = MD_UI;
                bValidMaterialDomain = true;
            }
            if (!bValidMaterialDomain) {
                SendAutomationError(Socket,
                    RequestId,
                    FString::Printf(
                        TEXT("Invalid materialDomain '%s'. Valid values: Surface, DeferredDecal, LightFunction, Volume, PostProcess, UI"),
                        *MaterialDomain),
                    TEXT("INVALID_ENUM"));
                return true;
            }
        }

        FString BlendMode;
        if (Payload->TryGetStringField(TEXT("blendMode"), BlendMode)) {
            bool bValidBlendMode = false;
            if (BlendMode == TEXT("Opaque")) {
                NewMaterial->BlendMode = EBlendMode::BLEND_Opaque;
                bValidBlendMode = true;
            }
            else if (BlendMode == TEXT("Masked")) {
                NewMaterial->BlendMode = EBlendMode::BLEND_Masked;
                bValidBlendMode = true;
            }
            else if (BlendMode == TEXT("Translucent")) {
                NewMaterial->BlendMode = EBlendMode::BLEND_Translucent;
                bValidBlendMode = true;
            }
            else if (BlendMode == TEXT("Additive")) {
                NewMaterial->BlendMode = EBlendMode::BLEND_Additive;
                bValidBlendMode = true;
            }
            else if (BlendMode == TEXT("Modulate")) {
                NewMaterial->BlendMode = EBlendMode::BLEND_Modulate;
                bValidBlendMode = true;
            }
            else if (BlendMode == TEXT("AlphaComposite")) {
                NewMaterial->BlendMode = EBlendMode::BLEND_AlphaComposite;
                bValidBlendMode = true;
            }
            else if (BlendMode == TEXT("AlphaHoldout")) {
                NewMaterial->BlendMode = EBlendMode::BLEND_AlphaHoldout;
                bValidBlendMode = true;
            }
            if (!bValidBlendMode) {
                SendAutomationError(Socket,
                    RequestId,
                    FString::Printf(
                        TEXT(
                            "Invalid blendMode '%s'. Valid values: Opaque, Masked, Translucent, Additive, Modulate, AlphaComposite, AlphaHoldout"),
                        *BlendMode),
                    TEXT("INVALID_ENUM"));
                return true;
            }
        }

        FString ShadingModel;
        if (Payload->TryGetStringField(TEXT("shadingModel"), ShadingModel)) {
            bool bValidShadingModel = false;
            if (ShadingModel == TEXT("Unlit")) {
                NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_Unlit);
                bValidShadingModel = true;
            }
            else if (ShadingModel == TEXT("DefaultLit")) {
                NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_DefaultLit);
                bValidShadingModel = true;
            }
            else if (ShadingModel == TEXT("Subsurface")) {
                NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_Subsurface);
                bValidShadingModel = true;
            }
            else if (ShadingModel == TEXT("SubsurfaceProfile")) {
                NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_SubsurfaceProfile);
                bValidShadingModel = true;
            }
            else if (ShadingModel == TEXT("PreintegratedSkin")) {
                NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_PreintegratedSkin);
                bValidShadingModel = true;
            }
            else if (ShadingModel == TEXT("ClearCoat")) {
                NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_ClearCoat);
                bValidShadingModel = true;
            }
            else if (ShadingModel == TEXT("Hair")) {
                NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_Hair);
                bValidShadingModel = true;
            }
            else if (ShadingModel == TEXT("Cloth")) {
                NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_Cloth);
                bValidShadingModel = true;
            }
            else if (ShadingModel == TEXT("Eye")) {
                NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_Eye);
                bValidShadingModel = true;
            }
            else if (ShadingModel == TEXT("TwoSidedFoliage")) {
                NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_TwoSidedFoliage);
                bValidShadingModel = true;
            }
            else if (ShadingModel == TEXT("ThinTranslucent")) {
                NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_ThinTranslucent);
                bValidShadingModel = true;
            }
            if (!bValidShadingModel) {
                SendAutomationError(Socket,
                    RequestId,
                    FString::Printf(
                        TEXT(
                            "Invalid shadingModel '%s'. Valid values: Unlit, DefaultLit, Subsurface, SubsurfaceProfile, PreintegratedSkin, ClearCoat, Hair, Cloth, Eye, TwoSidedFoliage, ThinTranslucent"),
                        *ShadingModel),
                    TEXT("INVALID_ENUM"));
                return true;
            }
        }

        bool bTwoSided = false;
        if (Payload->TryGetBoolField(TEXT("twoSided"), bTwoSided)) {
            NewMaterial->TwoSided = bTwoSided;
        }

        NewMaterial->PostEditChange();
        NewMaterial->MarkPackageDirty();

        // Notify asset registry FIRST (required for UE 5.7+ before saving)
        FAssetRegistryModule::AssetCreated(NewMaterial);

        bool bSave = true;
        Payload->TryGetBoolField(TEXT("save"), bSave);
        if (bSave) {
            SaveMaterialAsset_MaterialCreation(NewMaterial);
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        McpHandlerUtils::AddVerification(Result, NewMaterial);
        SendAutomationResponse(Socket,
            RequestId,
            true,
            FString::Printf(TEXT("Material '%s' created."), *Name),
            Result);
        return true;
    }

    return false;
}

#else // !WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_MaterialCreation(
    const FString& /*SubAction*/,
    const FString& /*RequestId*/,
    const TSharedPtr<FJsonObject>& /*Payload*/,
    TSharedPtr<FMcpBridgeWebSocket> /*Socket*/) {
    return false;
}

#endif // WITH_EDITOR
