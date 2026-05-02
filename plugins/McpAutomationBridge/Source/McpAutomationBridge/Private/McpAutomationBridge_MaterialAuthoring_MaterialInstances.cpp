/**
 * McpAutomationBridge_MaterialAuthoring_MaterialInstances.cpp
 *
 * Phase 8: Material Authoring - material instance sub-actions.
 *
 * Decomposed from McpAutomationBridge_MaterialAuthoringHandlers.cpp.
 * Handles material instance sub-actions.
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

static bool SaveMaterialInstanceAsset_MaterialInstances(UMaterialInstanceConstant *Instance) {
  if (!Instance)
    return false;

  // Use McpSafeAssetSave for proper asset registry notification
  return McpSafeAssetSave(Instance);
}

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_MaterialInstances(
    const FString& SubAction, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("get_material_instance_info")) {
    return HandleGetMaterialInstanceInfo(RequestId, TEXT("get_material_instance_info"), Payload, Socket);
  }

  if (SubAction == TEXT("set_material_instance_parent")) {
    FString AssetPath;
    FString ParentPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("parentPath"), ParentPath) || ParentPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'parentPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    AssetPath = SanitizeProjectRelativePath(AssetPath);
    ParentPath = SanitizeProjectRelativePath(ParentPath);
    if (AssetPath.IsEmpty() || ParentPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Invalid assetPath or parentPath."), TEXT("INVALID_PATH"));
      return true;
    }

    UMaterialInstanceConstant* Instance = LoadObject<UMaterialInstanceConstant>(nullptr, *AssetPath);
    UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, *ParentPath);
    if (!Instance) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load material instance."), TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    if (!Parent) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load parent material interface."), TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    if (Parent == Instance) {
      SendAutomationError(Socket, RequestId, TEXT("A material instance cannot parent itself."), TEXT("UNSUPPORTED_OPERATION"));
      return true;
    }

    for (UMaterialInterface* Cursor = Parent; Cursor; ) {
      UMaterialInstance* ParentInstance = Cast<UMaterialInstance>(Cursor);
      if (!ParentInstance) {
        break;
      }
      if (ParentInstance == Instance) {
        SendAutomationError(Socket, RequestId, TEXT("Parent update would create a material instance cycle."), TEXT("UNSUPPORTED_OPERATION"));
        return true;
      }
      Cursor = ParentInstance->Parent;
    }

    FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge", "SetMaterialInstanceParent", "MCP set material instance parent"));
    Instance->Modify();
    UMaterialInterface* OldParent = Instance->Parent;
    Instance->SetParentEditorOnly(Parent, true);
    Instance->PreEditChange(nullptr);
    Instance->PostEditChange();
    Instance->MarkPackageDirty();

    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    bool bSaved = false;
    if (bSave) {
      bSaved = SaveMaterialInstanceAsset_MaterialInstances(Instance);
      if (!bSaved) {
        SendAutomationError(Socket, RequestId, TEXT("Failed to save material instance."), TEXT("SAVE_FAILED"));
        return true;
      }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), Instance->GetPathName());
    Result->SetStringField(TEXT("parentPath"), Parent->GetPathName());
    Result->SetStringField(TEXT("baseMaterialPath"), Parent->GetMaterial() ? Parent->GetMaterial()->GetPathName() : TEXT(""));
    Result->SetStringField(TEXT("oldParentPath"), OldParent ? OldParent->GetPathName() : TEXT(""));
    Result->SetArrayField(TEXT("invalidatedOverrideNames"), TArray<TSharedPtr<FJsonValue>>());
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetBoolField(TEXT("dirty"), Instance->GetOutermost() && Instance->GetOutermost()->IsDirty());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Material instance parent updated."), Result);
    return true;
  }


  // ==========================================================================
  // 8.4 Material Instances
  // ==========================================================================

  // --------------------------------------------------------------------------
  // create_material_instance
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("create_material_instance")) {
    FString Name, Path, ParentMaterial;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'name'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Validate and sanitize the asset name (same as create_material)
    FString OriginalName = Name;
    FString SanitizedName = SanitizeAssetName(Name);
    
    FString NormalizedOriginal = OriginalName.Replace(TEXT("_"), TEXT(""));
    FString NormalizedSanitized = SanitizedName.Replace(TEXT("_"), TEXT(""));
    if (NormalizedSanitized != NormalizedOriginal) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid material instance name '%s': contains characters that cannot be used in asset names. Valid name would be: '%s'"),
                                          *OriginalName, *SanitizedName),
                          TEXT("INVALID_NAME"));
      return true;
    }
    Name = SanitizedName;

    if ((!Payload->TryGetStringField(TEXT("parentPath"), ParentMaterial) || ParentMaterial.IsEmpty()) &&
        (!Payload->TryGetStringField(TEXT("parentMaterial"), ParentMaterial) || ParentMaterial.IsEmpty())) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'parentPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Path = GetJsonStringField(Payload, TEXT("path"));
    if (Path.IsEmpty()) {
      Path = TEXT("/Game/Materials");
    }

    // Validate path (same as create_material)
    FString ValidatedPath;
    FString PathError;
    if (!ValidateAssetCreationPath(Path, Name, ValidatedPath, PathError)) {
      SendAutomationError(Socket, RequestId, PathError, TEXT("INVALID_PATH"));
      return true;
    }

    if (ValidatedPath.Contains(TEXT(":"))) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': absolute Windows paths are not allowed"), *ValidatedPath),
                          TEXT("INVALID_PATH"));
      return true;
    }

    FText MountReason;
    if (!FPackageName::IsValidLongPackageName(ValidatedPath, true, &MountReason)) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid package path '%s': %s"), *ValidatedPath, *MountReason.ToString()),
                          TEXT("INVALID_PATH"));
      return true;
    }

    // Check for existing asset collision
    FString FullAssetPath = ValidatedPath + TEXT(".") + Name;
    if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath)) {
      UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(FullAssetPath);
      if (ExistingAsset) {
        UClass* ExistingClass = ExistingAsset->GetClass();
        FString ExistingClassName = ExistingClass ? ExistingClass->GetName() : TEXT("Unknown");
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Asset '%s' already exists as %s. Cannot create MaterialInstanceConstant with the same name."),
                                            *FullAssetPath, *ExistingClassName),
                            TEXT("ASSET_EXISTS"));
      } else {
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Asset '%s' already exists. Cannot overwrite with different asset type."),
                                            *FullAssetPath),
                            TEXT("ASSET_EXISTS"));
      }
      return true;
    }
    // SECURITY: Validate parentPath path before loading
    FString ValidatedParentPath = SanitizeProjectRelativePath(ParentMaterial);
    if (ValidatedParentPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid parentPath '%s': contains traversal sequences or invalid root"), *ParentMaterial),
                          TEXT("INVALID_PATH"));
      return true;
    }
    ParentMaterial = ValidatedParentPath;

    UMaterialInterface *Parent = LoadObject<UMaterialInterface>(nullptr, *ParentMaterial);
    if (!Parent) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Could not load parent material interface."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    UMaterialInstanceConstantFactoryNew *Factory =
        NewObject<UMaterialInstanceConstantFactoryNew>();
    Factory->InitialParent = Parent;

    UPackage *Package = CreatePackage(*ValidatedPath);
    if (!Package) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create package."),
                          TEXT("PACKAGE_ERROR"));
      return true;
    }

    UMaterialInstanceConstant *NewInstance = Cast<UMaterialInstanceConstant>(
        Factory->FactoryCreateNew(UMaterialInstanceConstant::StaticClass(),
                                  Package, FName(*Name),
                                  RF_Public | RF_Standalone, nullptr, GWarn));
    if (!NewInstance) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Failed to create material instance."),
                          TEXT("CREATE_FAILED"));
      return true;
    }

    NewInstance->PostEditChange();
    NewInstance->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialInstanceAsset_MaterialInstances(NewInstance);
    }

    FAssetRegistryModule::AssetCreated(NewInstance);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, NewInstance);
    Result->SetStringField(TEXT("assetPath"), NewInstance->GetPathName());
    Result->SetStringField(TEXT("assetClass"), NewInstance->GetClass()->GetName());
    Result->SetStringField(TEXT("parentPath"), Parent->GetPathName());
    Result->SetStringField(TEXT("baseMaterialPath"), Parent->GetMaterial() ? Parent->GetMaterial()->GetPathName() : TEXT(""));
    Result->SetBoolField(TEXT("saved"), bSave);
    Result->SetBoolField(TEXT("dirty"), NewInstance->GetOutermost() && NewInstance->GetOutermost()->IsDirty());
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Material instance '%s' created."), *Name), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // set_scalar_parameter_value
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_scalar_parameter_value")) {
    FString AssetPath, ParamName;
    double Value = 0.0;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
        ParamName.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetNumberField(TEXT("value"), Value);

    // SECURITY: Validate path BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterialInstanceConstant *Instance =
        LoadObject<UMaterialInstanceConstant>(nullptr, *AssetPath);
    if (!Instance) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Could not load material instance."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    Instance->SetScalarParameterValueEditorOnly(FName(*ParamName), Value);
    Instance->PostEditChange();
    Instance->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialInstanceAsset_MaterialInstances(Instance);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Instance);
    Result->SetStringField(TEXT("parameterName"), ParamName);
    Result->SetNumberField(TEXT("value"), Value);
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Scalar parameter '%s' set to %f."), *ParamName,
                        Value), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // set_vector_parameter_value
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_vector_parameter_value")) {
    FString AssetPath, ParamName;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
        ParamName.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."),
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

    UMaterialInstanceConstant *Instance =
        LoadObject<UMaterialInstanceConstant>(nullptr, *AssetPath);
    if (!Instance) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Could not load material instance."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    FLinearColor Color(1.0f, 1.0f, 1.0f, 1.0f);
    const TSharedPtr<FJsonObject> *ValueObj;
    if (Payload->TryGetObjectField(TEXT("value"), ValueObj)) {
      double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
      (*ValueObj)->TryGetNumberField(TEXT("r"), R);
      (*ValueObj)->TryGetNumberField(TEXT("g"), G);
      (*ValueObj)->TryGetNumberField(TEXT("b"), B);
      (*ValueObj)->TryGetNumberField(TEXT("a"), A);
      Color = FLinearColor(R, G, B, A);
    }

    Instance->SetVectorParameterValueEditorOnly(FName(*ParamName), Color);
    Instance->PostEditChange();
    Instance->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialInstanceAsset_MaterialInstances(Instance);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Instance);
    Result->SetStringField(TEXT("parameterName"), ParamName);
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Vector parameter '%s' set."), *ParamName), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // set_texture_parameter_value
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_texture_parameter_value")) {
    FString AssetPath, ParamName, TexturePath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
        ParamName.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("texturePath"), TexturePath) ||
        TexturePath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'texturePath'."),
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

    UMaterialInstanceConstant *Instance =
        LoadObject<UMaterialInstanceConstant>(nullptr, *AssetPath);
    if (!Instance) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Could not load material instance."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    // SECURITY: Validate texturePath before loading
    FString ValidatedTexturePath = SanitizeProjectRelativePath(TexturePath);
    if (ValidatedTexturePath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid texturePath '%s': contains traversal sequences or invalid root"), *TexturePath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    TexturePath = ValidatedTexturePath;

    UTexture *Texture = LoadObject<UTexture>(nullptr, *TexturePath);
    if (!Texture) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load texture."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    Instance->SetTextureParameterValueEditorOnly(FName(*ParamName), Texture);
    Instance->PostEditChange();
    Instance->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialInstanceAsset_MaterialInstances(Instance);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Instance);
    Result->SetStringField(TEXT("parameterName"), ParamName);
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Texture parameter '%s' set."), *ParamName), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // get_material_instance_parameters (task 3.3) - spec-compliant with FMaterialParameterInfo
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("get_material_instance_parameters"))
  {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("missing-field")); return true;
    }
    AssetPath = SanitizeProjectRelativePath(AssetPath);
    UMaterialInstanceConstant* Instance = LoadObject<UMaterialInstanceConstant>(nullptr, *AssetPath);
    if (!Instance) { SendAutomationError(Socket, RequestId, TEXT("Could not load UMaterialInstanceConstant."), TEXT("invalid-asset")); return true; }

    auto MakeParamIdentity = [](const FMaterialParameterInfo& Info) -> TSharedPtr<FJsonObject> {
      TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
      P->SetStringField(TEXT("name"), Info.Name.ToString());
      FString Assoc = Info.Association == EMaterialParameterAssociation::GlobalParameter ? TEXT("GlobalParameter") :
                      Info.Association == EMaterialParameterAssociation::LayerParameter  ? TEXT("LayerParameter")  :
                      TEXT("BlendParameter");
      P->SetStringField(TEXT("association"), Assoc);
      P->SetNumberField(TEXT("index"), Info.Index);
      return P;
    };

    TArray<TSharedPtr<FJsonValue>> Parameters;

    // Scalar
    {
      TArray<FMaterialParameterInfo> Infos; TArray<FGuid> Guids;
      if (Instance->Parent) Instance->Parent->GetAllScalarParameterInfo(Infos, Guids);
      for (int32 i = 0; i < Infos.Num(); ++i) {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetObjectField(TEXT("parameterInfo"), MakeParamIdentity(Infos[i]));
        Obj->SetStringField(TEXT("type"), TEXT("Scalar"));
        float DefaultVal = 0;
        if (Instance->Parent) Instance->Parent->GetScalarParameterDefaultValue(Infos[i], DefaultVal);
        Obj->SetNumberField(TEXT("inheritedValue"), DefaultVal);
        float ExplicitVal = 0; bool bHasOverride = false;
        for (const auto& SV : Instance->ScalarParameterValues) {
          if (SV.ParameterInfo.Name == Infos[i].Name) { ExplicitVal = SV.ParameterValue; bHasOverride = true; break; }
        }
        Obj->SetBoolField(TEXT("hasExplicitOverride"), bHasOverride);
        if (bHasOverride) Obj->SetNumberField(TEXT("explicitValue"), ExplicitVal);
        float EffVal = 0; Instance->GetScalarParameterValue(Infos[i], EffVal);
        Obj->SetNumberField(TEXT("effectiveValue"), EffVal);
        if (i < Guids.Num()) Obj->SetStringField(TEXT("expressionGuid"), Guids[i].ToString());
        Parameters.Add(MakeShared<FJsonValueObject>(Obj));
      }
    }

    // Vector
    {
      TArray<FMaterialParameterInfo> Infos; TArray<FGuid> Guids;
      if (Instance->Parent) Instance->Parent->GetAllVectorParameterInfo(Infos, Guids);
      for (int32 i = 0; i < Infos.Num(); ++i) {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetObjectField(TEXT("parameterInfo"), MakeParamIdentity(Infos[i]));
        Obj->SetStringField(TEXT("type"), TEXT("Vector"));
        FLinearColor DefaultVal = FLinearColor::Black;
        if (Instance->Parent) Instance->Parent->GetVectorParameterDefaultValue(Infos[i], DefaultVal);
        TSharedPtr<FJsonObject> DefCol = MakeShared<FJsonObject>();
        DefCol->SetNumberField(TEXT("r"), DefaultVal.R); DefCol->SetNumberField(TEXT("g"), DefaultVal.G);
        DefCol->SetNumberField(TEXT("b"), DefaultVal.B); DefCol->SetNumberField(TEXT("a"), DefaultVal.A);
        Obj->SetObjectField(TEXT("inheritedValue"), DefCol);
        FLinearColor ExplicitVal = FLinearColor::Black; bool bHasOverride = false;
        for (const auto& VV : Instance->VectorParameterValues) {
          if (VV.ParameterInfo.Name == Infos[i].Name) { ExplicitVal = VV.ParameterValue; bHasOverride = true; break; }
        }
        Obj->SetBoolField(TEXT("hasExplicitOverride"), bHasOverride);
        if (bHasOverride) {
          TSharedPtr<FJsonObject> ExplCol = MakeShared<FJsonObject>();
          ExplCol->SetNumberField(TEXT("r"), ExplicitVal.R); ExplCol->SetNumberField(TEXT("g"), ExplicitVal.G);
          ExplCol->SetNumberField(TEXT("b"), ExplicitVal.B); ExplCol->SetNumberField(TEXT("a"), ExplicitVal.A);
          Obj->SetObjectField(TEXT("explicitValue"), ExplCol);
        }
        FLinearColor EffVal = FLinearColor::Black; Instance->GetVectorParameterValue(Infos[i], EffVal);
        TSharedPtr<FJsonObject> EffCol = MakeShared<FJsonObject>();
        EffCol->SetNumberField(TEXT("r"), EffVal.R); EffCol->SetNumberField(TEXT("g"), EffVal.G);
        EffCol->SetNumberField(TEXT("b"), EffVal.B); EffCol->SetNumberField(TEXT("a"), EffVal.A);
        Obj->SetObjectField(TEXT("effectiveValue"), EffCol);
        if (i < Guids.Num()) Obj->SetStringField(TEXT("expressionGuid"), Guids[i].ToString());
        Parameters.Add(MakeShared<FJsonValueObject>(Obj));
      }
    }

    // Texture
    {
      TArray<FMaterialParameterInfo> Infos; TArray<FGuid> Guids;
      if (Instance->Parent) Instance->Parent->GetAllTextureParameterInfo(Infos, Guids);
      for (int32 i = 0; i < Infos.Num(); ++i) {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetObjectField(TEXT("parameterInfo"), MakeParamIdentity(Infos[i]));
        Obj->SetStringField(TEXT("type"), TEXT("Texture"));
        UTexture* DefaultTex = nullptr;
        if (Instance->Parent) Instance->Parent->GetTextureParameterDefaultValue(Infos[i], DefaultTex);
        Obj->SetStringField(TEXT("inheritedValue"), DefaultTex ? DefaultTex->GetPathName() : TEXT(""));
        UTexture* ExplicitTex = nullptr; bool bHasOverride = false;
        for (const auto& TV : Instance->TextureParameterValues) {
          if (TV.ParameterInfo.Name == Infos[i].Name) { ExplicitTex = TV.ParameterValue.Get(); bHasOverride = true; break; }
        }
        Obj->SetBoolField(TEXT("hasExplicitOverride"), bHasOverride);
        if (bHasOverride) Obj->SetStringField(TEXT("explicitValue"), ExplicitTex ? ExplicitTex->GetPathName() : TEXT(""));
        UTexture* EffTex = nullptr; Instance->GetTextureParameterValue(Infos[i], EffTex);
        Obj->SetStringField(TEXT("effectiveValue"), EffTex ? EffTex->GetPathName() : TEXT(""));
        if (i < Guids.Num()) Obj->SetStringField(TEXT("expressionGuid"), Guids[i].ToString());
        Parameters.Add(MakeShared<FJsonValueObject>(Obj));
      }
    }

    // Static switch
    {
      TArray<FMaterialParameterInfo> Infos; TArray<FGuid> Guids;
      if (Instance->Parent) Instance->Parent->GetAllStaticSwitchParameterInfo(Infos, Guids);
      for (int32 i = 0; i < Infos.Num(); ++i) {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetObjectField(TEXT("parameterInfo"), MakeParamIdentity(Infos[i]));
        Obj->SetStringField(TEXT("type"), TEXT("StaticSwitch"));
        FStaticParameterSet StaticParams;
        Instance->GetStaticParameterValues(StaticParams);
        bool bHasOverride = false; bool bExplicitVal = false;
        for (const auto& SP : StaticParams.StaticSwitchParameters) {
          if (SP.ParameterInfo.Name == Infos[i].Name && SP.bOverride) { bHasOverride = true; bExplicitVal = SP.Value; break; }
        }
        Obj->SetBoolField(TEXT("hasExplicitOverride"), bHasOverride);
        if (bHasOverride) Obj->SetBoolField(TEXT("explicitValue"), bExplicitVal);
        if (i < Guids.Num()) Obj->SetStringField(TEXT("expressionGuid"), Guids[i].ToString());
        Parameters.Add(MakeShared<FJsonValueObject>(Obj));
      }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetStringField(TEXT("assetClass"), TEXT("MaterialInstanceConstant"));
    Result->SetStringField(TEXT("parentPath"), Instance->Parent ? Instance->Parent->GetPathName() : TEXT(""));
    Result->SetArrayField(TEXT("parameters"), Parameters);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Material instance parameters retrieved."), Result);
    return true;
  }


  if (SubAction == TEXT("set_material_instance_parameter") ||
      SubAction == TEXT("reset_material_instance_parameter") ||
      SubAction == TEXT("clear_material_instance_parameters") ||
      SubAction == TEXT("bulk_set_material_instance_parameters"))
  {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("missing-field")); return true;
    }
    AssetPath = SanitizeProjectRelativePath(AssetPath);
    UMaterialInstanceConstant* Instance = LoadObject<UMaterialInstanceConstant>(nullptr, *AssetPath);
    if (!Instance) { SendAutomationError(Socket, RequestId, TEXT("Could not load UMaterialInstanceConstant."), TEXT("invalid-asset")); return true; }

    // Helper: build parameter identity object from FMaterialParameterInfo
    auto BuildParamIdentity = [](const FMaterialParameterInfo& Info) -> TSharedPtr<FJsonObject> {
      TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
      P->SetStringField(TEXT("name"), Info.Name.ToString());
      FString Assoc = Info.Association == EMaterialParameterAssociation::GlobalParameter ? TEXT("GlobalParameter") :
                      Info.Association == EMaterialParameterAssociation::LayerParameter  ? TEXT("LayerParameter")  :
                      TEXT("BlendParameter");
      P->SetStringField(TEXT("association"), Assoc);
      P->SetNumberField(TEXT("index"), Info.Index);
      return P;
    };

    // Helper: validate that a parameter name exists in the parent namespace
    auto ParameterExistsInParent = [&](const FName& ParamName, const FString& TypeHint) -> bool {
      if (!Instance->Parent) return false;
      TArray<FMaterialParameterInfo> Infos; TArray<FGuid> Guids;
      if (TypeHint == TEXT("Scalar"))       Instance->Parent->GetAllScalarParameterInfo(Infos, Guids);
      else if (TypeHint == TEXT("Vector"))  Instance->Parent->GetAllVectorParameterInfo(Infos, Guids);
      else if (TypeHint == TEXT("DoubleVector")) Instance->Parent->GetAllDoubleVectorParameterInfo(Infos, Guids);
      else if (TypeHint == TEXT("Texture") || TypeHint == TEXT("RuntimeVirtualTexture") || TypeHint == TEXT("SparseVolumeTexture"))
                                             Instance->Parent->GetAllTextureParameterInfo(Infos, Guids);
      else if (TypeHint == TEXT("Font"))    Instance->Parent->GetAllFontParameterInfo(Infos, Guids);
      else if (TypeHint == TEXT("StaticSwitch")) Instance->Parent->GetAllStaticSwitchParameterInfo(Infos, Guids);
      else {
        // Try all types
        TArray<FMaterialParameterInfo> All; TArray<FGuid> AllG;
        Instance->Parent->GetAllScalarParameterInfo(All, AllG); for (const auto& I : All) { if (I.Name == ParamName) return true; }
        Instance->Parent->GetAllVectorParameterInfo(All, AllG); for (const auto& I : All) { if (I.Name == ParamName) return true; }
        Instance->Parent->GetAllTextureParameterInfo(All, AllG); for (const auto& I : All) { if (I.Name == ParamName) return true; }
        Instance->Parent->GetAllStaticSwitchParameterInfo(All, AllG); for (const auto& I : All) { if (I.Name == ParamName) return true; }
        return false;
      }
      for (const auto& I : Infos) { if (I.Name == ParamName) return true; }
      return false;
    };

    if (SubAction == TEXT("clear_material_instance_parameters"))
    {
      FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge", "ClearMaterialInstanceParams", "MCP clear material instance parameters"));
      Instance->Modify();
      const int32 BeforeCount = Instance->ScalarParameterValues.Num() + Instance->VectorParameterValues.Num() +
                                Instance->TextureParameterValues.Num();
      Instance->ScalarParameterValues.Empty();
      Instance->VectorParameterValues.Empty();
      Instance->TextureParameterValues.Empty();
      FStaticParameterSet StaticParams;
      Instance->GetStaticParameterValues(StaticParams);
      StaticParams.StaticSwitchParameters.Empty();
      Instance->UpdateStaticPermutation(StaticParams);
      Instance->PreEditChange(nullptr);
      Instance->PostEditChange();
      Instance->MarkPackageDirty();

      bool bSave = false; Payload->TryGetBoolField(TEXT("save"), bSave);
      bool bSaved = false;
      if (bSave) { bSaved = McpSafeAssetSave(Instance); if (!bSaved) { SendAutomationError(Socket, RequestId, TEXT("Save failed."), TEXT("save-failed")); return true; } }

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("assetPath"), AssetPath);
      Result->SetNumberField(TEXT("clearedCount"), BeforeCount);
      Result->SetNumberField(TEXT("remainingExplicitOverrideCount"), 0);
      Result->SetBoolField(TEXT("saved"), bSaved);
      Result->SetBoolField(TEXT("dirty"), Instance->GetOutermost()->IsDirty());
      SendAutomationResponse(Socket, RequestId, true, TEXT("Material instance parameters cleared."), Result);
      return true;
    }

    // Helper: apply one parameter override (returns error string or empty string on success)
    auto ApplyOneOverride = [&](const TSharedPtr<FJsonObject>& ParamObj, const TSharedPtr<FJsonObject>& ValueObj) -> FString
    {
      if (!ParamObj || !ValueObj) return TEXT("Invalid parameter or value object.");
      FString ParamName; ParamObj->TryGetStringField(TEXT("name"), ParamName);
      if (ParamName.IsEmpty()) return TEXT("Parameter 'name' is required.");
      FString Association; ParamObj->TryGetStringField(TEXT("association"), Association);
      if (!Association.IsEmpty() && Association != TEXT("GlobalParameter")) return TEXT("unsupported-association: only GlobalParameter writes are supported.");
      FString TypeStr; ParamObj->TryGetStringField(TEXT("type"), TypeStr);

      const FMaterialParameterInfo Info(FName(*ParamName), EMaterialParameterAssociation::GlobalParameter);
      if (!ParameterExistsInParent(FName(*ParamName), TypeStr)) {
        return FString::Printf(TEXT("unknown-parameter: '%s' not found in parent parameter namespace."), *ParamName);
      }

      if (TypeStr == TEXT("Scalar")) {
        double Val = 0; ValueObj->TryGetNumberField(TEXT("value"), Val);
        // Note: ValueObj IS the value for scalar (it's a number in the outer object)
        double ScalarVal = 0;
        if (!Payload->TryGetNumberField(TEXT("value"), ScalarVal)) ValueObj->TryGetNumberField(TEXT("value"), ScalarVal);
        Instance->SetScalarParameterValueEditorOnly(Info, (float)ScalarVal);
      } else if (TypeStr == TEXT("Vector")) {
        double R = 0, G = 0, B = 0, A = 1;
        ValueObj->TryGetNumberField(TEXT("r"), R); ValueObj->TryGetNumberField(TEXT("g"), G);
        ValueObj->TryGetNumberField(TEXT("b"), B); ValueObj->TryGetNumberField(TEXT("a"), A);
        Instance->SetVectorParameterValueEditorOnly(Info, FLinearColor((float)R, (float)G, (float)B, (float)A));
      } else if (TypeStr == TEXT("Texture") || TypeStr == TEXT("RuntimeVirtualTexture") || TypeStr == TEXT("SparseVolumeTexture")) {
        FString TexPath; ValueObj->TryGetStringField(TEXT("assetPath"), TexPath);
        if (!TexPath.IsEmpty()) {
          UTexture* Tex = LoadObject<UTexture>(nullptr, *SanitizeProjectRelativePath(TexPath));
          if (!Tex) return FString::Printf(TEXT("Could not load texture '%s'."), *TexPath);
          Instance->SetTextureParameterValueEditorOnly(Info, Tex);
        }
      } else if (TypeStr == TEXT("Font")) {
        FString FontPath; ValueObj->TryGetStringField(TEXT("fontPath"), FontPath);
        double FontPage = 0; ValueObj->TryGetNumberField(TEXT("fontPage"), FontPage);
        UFont* Font = LoadObject<UFont>(nullptr, *SanitizeProjectRelativePath(FontPath));
        if (!Font) return FString::Printf(TEXT("Could not load font '%s'."), *FontPath);
        Instance->SetFontParameterValueEditorOnly(Info, Font, (int32)FontPage);
      } else if (TypeStr == TEXT("StaticSwitch")) {
        bool bEnabled2 = false; ValueObj->TryGetBoolField(TEXT("enabled"), bEnabled2);
        FStaticParameterSet StaticParams;
        Instance->GetStaticParameterValues(StaticParams);
        bool bFound = false;
        for (auto& SP : StaticParams.StaticSwitchParameters) {
          if (SP.ParameterInfo.Name == FName(*ParamName)) {
            SP.Value = bEnabled2; SP.bOverride = true; bFound = true; break;
          }
        }
        if (!bFound) {
          FStaticSwitchParameter NewParam;
          NewParam.ParameterInfo = FMaterialParameterInfo(FName(*ParamName));
          NewParam.Value = bEnabled2; NewParam.bOverride = true;
          StaticParams.StaticSwitchParameters.Add(NewParam);
        }
        Instance->UpdateStaticPermutation(StaticParams);
      } else if (TypeStr == TEXT("StaticComponentMask")) {
        return TEXT("StaticComponentMask parameters are not supported in UE 5.4+.");
      } else {
        return FString::Printf(TEXT("unsupported-type: '%s' is not a supported parameter type."), *TypeStr);
      }
      return TEXT("");
    };

    if (SubAction == TEXT("bulk_set_material_instance_parameters"))
    {
      const TArray<TSharedPtr<FJsonValue>>* Overrides = nullptr;
      if (!Payload->TryGetArrayField(TEXT("overrides"), Overrides) || !Overrides) {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'overrides' array."), TEXT("missing-field")); return true;
      }

      // Pre-validate all entries
      for (const auto& OverrideVal : *Overrides) {
        const TSharedPtr<FJsonObject>* OverrideObj = nullptr;
        if (!OverrideVal->TryGetObject(OverrideObj) || !OverrideObj) {
          SendAutomationError(Socket, RequestId, TEXT("Invalid override entry."), TEXT("partial-failure-not-allowed")); return true;
        }
        const TSharedPtr<FJsonObject>* ParamObj = nullptr;
        if (!(*OverrideObj)->TryGetObjectField(TEXT("parameter"), ParamObj) || !ParamObj) {
          SendAutomationError(Socket, RequestId, TEXT("Override missing 'parameter' field."), TEXT("partial-failure-not-allowed")); return true;
        }
        const TSharedPtr<FJsonObject>* ValueObj = nullptr;
        (*OverrideObj)->TryGetObjectField(TEXT("value"), ValueObj);

        FString ParamName; (*ParamObj)->TryGetStringField(TEXT("name"), ParamName);
        FString TypeStr; (*ParamObj)->TryGetStringField(TEXT("type"), TypeStr);
        FString Assoc; (*ParamObj)->TryGetStringField(TEXT("association"), Assoc);
        if (!Assoc.IsEmpty() && Assoc != TEXT("GlobalParameter")) {
          SendAutomationError(Socket, RequestId, TEXT("unsupported-association: only GlobalParameter writes are supported."), TEXT("partial-failure-not-allowed")); return true;
        }
        if (!ParameterExistsInParent(FName(*ParamName), TypeStr)) {
          SendAutomationError(Socket, RequestId,
              FString::Printf(TEXT("unknown-parameter: '%s' not found in parent parameter namespace."), *ParamName),
              TEXT("partial-failure-not-allowed")); return true;
        }
      }

      FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge", "BulkSetMaterialInstanceParams", "MCP bulk set material instance parameters"));
      Instance->Modify();

      TArray<TSharedPtr<FJsonValue>> AppliedParams;
      for (const auto& OverrideVal : *Overrides) {
        const TSharedPtr<FJsonObject>* OverrideObj = nullptr;
        OverrideVal->TryGetObject(OverrideObj);
        const TSharedPtr<FJsonObject>* ParamObj = nullptr;
        (*OverrideObj)->TryGetObjectField(TEXT("parameter"), ParamObj);
        const TSharedPtr<FJsonObject>* ValueObj = nullptr;
        (*OverrideObj)->TryGetObjectField(TEXT("value"), ValueObj);
        ApplyOneOverride(*ParamObj, ValueObj ? *ValueObj : TSharedPtr<FJsonObject>());
        AppliedParams.Add(MakeShared<FJsonValueObject>(*ParamObj));
      }

      Instance->PreEditChange(nullptr);
      Instance->PostEditChange();
      Instance->MarkPackageDirty();

      bool bSave = false; Payload->TryGetBoolField(TEXT("save"), bSave);
      bool bSaved = false;
      if (bSave) { bSaved = McpSafeAssetSave(Instance); if (!bSaved) { SendAutomationError(Socket, RequestId, TEXT("Save failed."), TEXT("save-failed")); return true; } }

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("assetPath"), AssetPath);
      Result->SetNumberField(TEXT("appliedCount"), AppliedParams.Num());
      Result->SetArrayField(TEXT("parameters"), AppliedParams);
      Result->SetBoolField(TEXT("saved"), bSaved);
      Result->SetBoolField(TEXT("dirty"), Instance->GetOutermost()->IsDirty());
      SendAutomationResponse(Socket, RequestId, true, TEXT("Bulk material instance parameters applied."), Result);
      return true;
    }

    if (SubAction == TEXT("set_material_instance_parameter"))
    {
      const TSharedPtr<FJsonObject>* ParamObj = nullptr;
      if (!Payload->TryGetObjectField(TEXT("parameter"), ParamObj) || !ParamObj) {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'parameter' object."), TEXT("missing-field")); return true;
      }
      const TSharedPtr<FJsonObject>* ValueObj = nullptr;
      Payload->TryGetObjectField(TEXT("value"), ValueObj);

      FString Assoc; (*ParamObj)->TryGetStringField(TEXT("association"), Assoc);
      if (!Assoc.IsEmpty() && Assoc != TEXT("GlobalParameter")) {
        SendAutomationError(Socket, RequestId, TEXT("unsupported-association: only GlobalParameter writes are supported."), TEXT("unsupported-association")); return true;
      }
      FString ParamName; (*ParamObj)->TryGetStringField(TEXT("name"), ParamName);
      FString TypeStr; (*ParamObj)->TryGetStringField(TEXT("type"), TypeStr);
      if (!ParameterExistsInParent(FName(*ParamName), TypeStr)) {
        SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("unknown-parameter: '%s' not found in parent parameter namespace."), *ParamName),
            TEXT("unknown-parameter")); return true;
      }

      FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge", "SetMaterialInstanceParam", "MCP set material instance parameter"));
      Instance->Modify();
      FString Err = ApplyOneOverride(*ParamObj, ValueObj ? *ValueObj : TSharedPtr<FJsonObject>());
      if (!Err.IsEmpty()) {
        if (Err.Contains(TEXT("unknown-parameter"))) { SendAutomationError(Socket, RequestId, Err, TEXT("unknown-parameter")); }
        else if (Err.Contains(TEXT("unsupported-association"))) { SendAutomationError(Socket, RequestId, Err, TEXT("unsupported-association")); }
        else if (Err.Contains(TEXT("unsupported-type"))) { SendAutomationError(Socket, RequestId, Err, TEXT("unsupported-type")); }
        else { SendAutomationError(Socket, RequestId, Err, TEXT("invalid-asset")); }
        return true;
      }

      Instance->PreEditChange(nullptr);
      Instance->PostEditChange();
      Instance->MarkPackageDirty();

      bool bSave = false; Payload->TryGetBoolField(TEXT("save"), bSave);
      bool bSaved = false;
      if (bSave) { bSaved = McpSafeAssetSave(Instance); if (!bSaved) { SendAutomationError(Socket, RequestId, TEXT("Save failed."), TEXT("save-failed")); return true; } }

      const FMaterialParameterInfo Info{FName(*ParamName)};
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("assetPath"), AssetPath);
      Result->SetObjectField(TEXT("parameter"), *ParamObj);
      Result->SetBoolField(TEXT("saved"), bSaved);
      Result->SetBoolField(TEXT("dirty"), Instance->GetOutermost()->IsDirty());
      SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Parameter '%s' set."), *ParamName), Result);
      return true;
    }

    if (SubAction == TEXT("reset_material_instance_parameter"))
    {
      const TSharedPtr<FJsonObject>* ParamObj = nullptr;
      if (!Payload->TryGetObjectField(TEXT("parameter"), ParamObj) || !ParamObj) {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'parameter' object."), TEXT("missing-field")); return true;
      }
      FString ParamName; (*ParamObj)->TryGetStringField(TEXT("name"), ParamName);
      FString Assoc; (*ParamObj)->TryGetStringField(TEXT("association"), Assoc);
      if (!Assoc.IsEmpty() && Assoc != TEXT("GlobalParameter")) {
        SendAutomationError(Socket, RequestId, TEXT("unsupported-association."), TEXT("unsupported-association")); return true;
      }

      FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge", "ResetMaterialInstanceParam", "MCP reset material instance parameter"));
      Instance->Modify();
      const FName PN(*ParamName);
      Instance->ScalarParameterValues.RemoveAll([&PN](const FScalarParameterValue& V){ return V.ParameterInfo.Name == PN; });
      Instance->VectorParameterValues.RemoveAll([&PN](const FVectorParameterValue& V){ return V.ParameterInfo.Name == PN; });
      Instance->TextureParameterValues.RemoveAll([&PN](const FTextureParameterValue& V){ return V.ParameterInfo.Name == PN; });
      FStaticParameterSet StaticParams;
      Instance->GetStaticParameterValues(StaticParams);
      StaticParams.StaticSwitchParameters.RemoveAll([&PN](const FStaticSwitchParameter& V){ return V.ParameterInfo.Name == PN; });
      Instance->UpdateStaticPermutation(StaticParams);
      Instance->PreEditChange(nullptr);
      Instance->PostEditChange();
      Instance->MarkPackageDirty();

      bool bSave = false; Payload->TryGetBoolField(TEXT("save"), bSave);
      bool bSaved = false;
      if (bSave) { bSaved = McpSafeAssetSave(Instance); if (!bSaved) { SendAutomationError(Socket, RequestId, TEXT("Save failed."), TEXT("save-failed")); return true; } }

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("assetPath"), AssetPath);
      Result->SetObjectField(TEXT("parameter"), *ParamObj);
      Result->SetBoolField(TEXT("saved"), bSaved);
      Result->SetBoolField(TEXT("dirty"), Instance->GetOutermost()->IsDirty());
      SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Parameter '%s' reset."), *ParamName), Result);
      return true;
    }
  }

  return false;
}

#else // !WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_MaterialInstances(
    const FString& /*SubAction*/, const FString& /*RequestId*/,
    const TSharedPtr<FJsonObject>& /*Payload*/,
    TSharedPtr<FMcpBridgeWebSocket> /*Socket*/)
{
  return false;
}

#endif // WITH_EDITOR
