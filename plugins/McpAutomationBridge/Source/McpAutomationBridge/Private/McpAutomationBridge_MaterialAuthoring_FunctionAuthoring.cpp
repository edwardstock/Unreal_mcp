/**
 * McpAutomationBridge_MaterialAuthoring_FunctionAuthoring.cpp
 *
 * Phase 8: Material Authoring - function authoring sub-actions.
 *
 * Decomposed from McpAutomationBridge_MaterialAuthoringHandlers.cpp.
 * Handles material function authoring sub-actions.
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

// Local copy of LOAD_MATERIAL_OR_RETURN. Mirrors the macro in
// McpAutomationBridge_MaterialAuthoringHandlers.cpp so this domain can
// keep its extracted blocks unchanged.
#define LOAD_MATERIAL_OR_RETURN()                                              \
  FString AssetPath;                                                           \
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||             \
      AssetPath.IsEmpty()) {                                                   \
    SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),       \
                        TEXT("INVALID_ARGUMENT"));                             \
    return true;                                                               \
  }                                                                            \
  /* SECURITY: Validate path BEFORE loading asset */                           \
  FString ValidatedAssetPath = SanitizeProjectRelativePath(AssetPath);         \
  if (ValidatedAssetPath.IsEmpty()) {                                          \
    SendAutomationError(Socket, RequestId,                                     \
                        FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath), \
                        TEXT("INVALID_PATH"));                                \
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

static bool SaveMaterialFunctionAsset_FunctionAuthoring(UMaterialFunction *Function) {
  if (!Function)
    return false;

  // Use McpSafeAssetSave for proper asset registry notification
  return McpSafeAssetSave(Function);
}

static UMaterialExpression* McpAuthoringFindExpressionFromPayload_FunctionAuthoring(
    const FMcpMaterialGraphOwner& Owner,
    const TSharedPtr<FJsonObject>& Payload)
{
    const TSharedPtr<FJsonObject>* ExpressionObject = nullptr;
    if (Payload->TryGetObjectField(TEXT("expression"), ExpressionObject) && ExpressionObject && ExpressionObject->IsValid())
    {
        int32 ExpressionIndex = INDEX_NONE;
        if ((*ExpressionObject)->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex))
            return McpFindGraphExpression(Owner, FString(), ExpressionIndex);

        FString ExpressionPath;
        if ((*ExpressionObject)->TryGetStringField(TEXT("expressionPath"), ExpressionPath) && !ExpressionPath.IsEmpty())
        {
            UMaterialExpression* Found = McpFindGraphExpression(Owner, ExpressionPath);
            FString ExpressionGuid;
            if (Found && (*ExpressionObject)->TryGetStringField(TEXT("expressionGuid"), ExpressionGuid) && !ExpressionGuid.IsEmpty())
            {
                FGuid ParsedGuid;
                if (FGuid::Parse(ExpressionGuid, ParsedGuid) && Found->MaterialExpressionGuid != ParsedGuid)
                    return nullptr;
            }
            return Found;
        }

        FString ExpressionName;
        if ((*ExpressionObject)->TryGetStringField(TEXT("expressionName"), ExpressionName) && !ExpressionName.IsEmpty())
            return McpFindGraphExpression(Owner, ExpressionName);
    }

    FString ExpressionRef;
    if (Payload->TryGetStringField(TEXT("expression"), ExpressionRef) && !ExpressionRef.IsEmpty())
        return McpFindGraphExpression(Owner, ExpressionRef);

    int32 ExpressionIndex = INDEX_NONE;
    if (Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex))
        return McpFindGraphExpression(Owner, FString(), ExpressionIndex);

    FString NodeId;
    if (Payload->TryGetStringField(TEXT("nodeId"), NodeId) && !NodeId.IsEmpty())
        return McpFindGraphExpression(Owner, NodeId);

    return nullptr;
}

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_FunctionAuthoring(
    const FString& SubAction, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{

  // ==========================================================================
  // 8.3 Material Functions
  // ==========================================================================

  // --------------------------------------------------------------------------
  // create_material_function
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("create_material_function")) {
    FString Name, Path, Description;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'name'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Validate and sanitize the asset name (same as create_material)
    FString OriginalName = Name;
    FString SanitizedName = SanitizeAssetName(Name);
    
    // Check if sanitization significantly changed the name (indicates invalid characters)
    FString NormalizedOriginal = OriginalName.Replace(TEXT("_"), TEXT(""));
    FString NormalizedSanitized = SanitizedName.Replace(TEXT("_"), TEXT(""));
    if (NormalizedSanitized != NormalizedOriginal) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid material function name '%s': contains characters that cannot be used in asset names. Valid name would be: '%s'"),
                                          *OriginalName, *SanitizedName),
                          TEXT("INVALID_NAME"));
      return true;
    }
    Name = SanitizedName;

    Path = GetJsonStringField(Payload, TEXT("path"));
    if (Path.IsEmpty()) {
      Path = TEXT("/Game/Materials/Functions");
    }

    // Validate path doesn't contain traversal sequences (same as create_material)
    FString ValidatedPath;
    FString PathError;
    if (!ValidateAssetCreationPath(Path, Name, ValidatedPath, PathError)) {
      SendAutomationError(Socket, RequestId, PathError, TEXT("INVALID_PATH"));
      return true;
    }

    // Additional validation: reject Windows absolute paths (contain colon)
    if (ValidatedPath.Contains(TEXT(":"))) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': absolute Windows paths are not allowed"), *ValidatedPath),
                          TEXT("INVALID_PATH"));
      return true;
    }

    // Additional validation: verify mount point using engine API
    FText MountReason;
    if (!FPackageName::IsValidLongPackageName(ValidatedPath, true, &MountReason)) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid package path '%s': %s"), *ValidatedPath, *MountReason.ToString()),
                          TEXT("INVALID_PATH"));
      return true;
    }

    // Check for existing asset collision to prevent UE crash
    // Creating a MaterialFunction over an existing Material causes fatal error
    FString FullAssetPath = ValidatedPath + TEXT(".") + Name;
    if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath)) {
      // Get the existing asset's class to provide helpful error
      UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(FullAssetPath);
      if (ExistingAsset) {
        UClass* ExistingClass = ExistingAsset->GetClass();
        FString ExistingClassName = ExistingClass ? ExistingClass->GetName() : TEXT("Unknown");
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Asset '%s' already exists as %s. Cannot create MaterialFunction with the same name."),
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

    Payload->TryGetStringField(TEXT("description"), Description);

    bool bExposeToLibrary = true;
    Payload->TryGetBoolField(TEXT("exposeToLibrary"), bExposeToLibrary);

    // Create function using factory - use ValidatedPath, not original Path!
    UMaterialFunctionFactoryNew *Factory =
        NewObject<UMaterialFunctionFactoryNew>();
    UPackage *Package = CreatePackage(*ValidatedPath);
    if (!Package) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create package."),
                          TEXT("PACKAGE_ERROR"));
      return true;
    }

    UMaterialFunction *NewFunc = Cast<UMaterialFunction>(
        Factory->FactoryCreateNew(UMaterialFunction::StaticClass(), Package,
                                  FName(*Name), RF_Public | RF_Standalone,
                                  nullptr, GWarn));
    if (!NewFunc) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Failed to create material function."),
                          TEXT("CREATE_FAILED"));
      return true;
    }

    if (!Description.IsEmpty()) {
      NewFunc->Description = Description;
    }
    NewFunc->bExposeToLibrary = bExposeToLibrary;

    NewFunc->PostEditChange();
    NewFunc->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialFunctionAsset_FunctionAuthoring(NewFunc);
    }

    FAssetRegistryModule::AssetCreated(NewFunc);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, NewFunc);
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Material function '%s' created."), *Name), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // add_function_input / add_function_output
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_function_input") ||
      SubAction == TEXT("add_function_output")) {
    FString AssetPath, InputName, InputType;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("inputName"), InputName) ||
        InputName.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'inputName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetStringField(TEXT("inputType"), InputType);

    float X = 0.0f, Y = 0.0f;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    // SECURITY: Validate path BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterialFunction *Func =
        LoadObject<UMaterialFunction>(nullptr, *AssetPath);
    if (!Func) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Could not load Material Function."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    UMaterialExpression *NewExpr = nullptr;
    if (SubAction == TEXT("add_function_input")) {
      UMaterialExpressionFunctionInput *Input =
          NewObject<UMaterialExpressionFunctionInput>(
              Func, UMaterialExpressionFunctionInput::StaticClass(), NAME_None,
              RF_Transactional);
      Input->InputName = FName(*InputName);
      // Set input type
      if (InputType == TEXT("Float1") || InputType == TEXT("Scalar"))
        Input->InputType = EFunctionInputType::FunctionInput_Scalar;
      else if (InputType == TEXT("Float2") || InputType == TEXT("Vector2"))
        Input->InputType = EFunctionInputType::FunctionInput_Vector2;
      else if (InputType == TEXT("Float3") || InputType == TEXT("Vector3"))
        Input->InputType = EFunctionInputType::FunctionInput_Vector3;
      else if (InputType == TEXT("Float4") || InputType == TEXT("Vector4"))
        Input->InputType = EFunctionInputType::FunctionInput_Vector4;
      else if (InputType == TEXT("Texture2D"))
        Input->InputType = EFunctionInputType::FunctionInput_Texture2D;
      else if (InputType == TEXT("TextureCube"))
        Input->InputType = EFunctionInputType::FunctionInput_TextureCube;
      else if (InputType == TEXT("Bool"))
        Input->InputType = EFunctionInputType::FunctionInput_StaticBool;
      else if (InputType == TEXT("MaterialAttributes"))
        Input->InputType = EFunctionInputType::FunctionInput_MaterialAttributes;
      else
        Input->InputType = EFunctionInputType::FunctionInput_Vector3;
      NewExpr = Input;
    } else {
      UMaterialExpressionFunctionOutput *Output =
          NewObject<UMaterialExpressionFunctionOutput>(
              Func, UMaterialExpressionFunctionOutput::StaticClass(), NAME_None,
              RF_Transactional);
      Output->OutputName = FName(*InputName);
      NewExpr = Output;
    }

    NewExpr->MaterialExpressionEditorX = (int32)X;
    NewExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    // UE 5.0: MaterialFunction uses FunctionExpressions, not Expressions
    #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      Func->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(NewExpr);
    #else
      Func->FunctionExpressions.Add(NewExpr);
    #endif
#endif
    Func->PostEditChange();
    Func->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           NewExpr->MaterialExpressionGuid.ToString());
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Function %s '%s' added."),
                        SubAction == TEXT("add_function_input") ? TEXT("input")
                                                                 : TEXT("output"),
                        *InputName),
        Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // use_material_function
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("use_material_function")) {
    LOAD_MATERIAL_OR_RETURN();

    FString FunctionPath;
    if (!Payload->TryGetStringField(TEXT("functionPath"), FunctionPath) ||
        FunctionPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'functionPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // SECURITY: Validate functionPath before loading
    FString ValidatedFunctionPath = SanitizeProjectRelativePath(FunctionPath);
    if (ValidatedFunctionPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid functionPath '%s': contains traversal sequences or invalid root"), *FunctionPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    FunctionPath = ValidatedFunctionPath;

    UMaterialFunction *Func =
        LoadObject<UMaterialFunction>(nullptr, *FunctionPath);
    if (!Func) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Could not load Material Function."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    UMaterialExpressionMaterialFunctionCall *FuncCall =
        NewObject<UMaterialExpressionMaterialFunctionCall>(
            Material, UMaterialExpressionMaterialFunctionCall::StaticClass(),
            NAME_None, RF_Transactional);
    FuncCall->SetMaterialFunction(Func);
    FuncCall->MaterialExpressionEditorX = (int32)X;
    FuncCall->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(FuncCall);
#endif

    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           FuncCall->MaterialExpressionGuid.ToString());
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material function added."), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // update_function_input (task 2.2)
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("update_function_input"))
  {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("missing-field"));
      return true;
    }
    AssetPath = SanitizeProjectRelativePath(AssetPath);
    if (AssetPath.IsEmpty()) { SendAutomationError(Socket, RequestId, TEXT("Invalid assetPath."), TEXT("invalid-asset")); return true; }

    FMcpMaterialGraphOwner GraphOwner;
    FString GraphOwnerError;
    if (!McpResolveMaterialGraphOwner(AssetPath, GraphOwner, GraphOwnerError) || GraphOwner.bReadOnly) {
      SendAutomationError(Socket, RequestId, GraphOwnerError.IsEmpty() ? TEXT("Cannot mutate this asset.") : GraphOwnerError, TEXT("unsupported-operation"));
      return true;
    }

    UMaterialExpressionFunctionInput* Input = Cast<UMaterialExpressionFunctionInput>(McpAuthoringFindExpressionFromPayload_FunctionAuthoring(GraphOwner, Payload));
    if (!Input) { SendAutomationError(Socket, RequestId, TEXT("Target expression is not a UMaterialExpressionFunctionInput."), TEXT("invalid-pin")); return true; }

    TArray<FString> ChangedFields;
    FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge", "UpdateFunctionInput", "MCP update function input"));
    GraphOwner.Asset->Modify();
    Input->Modify();

    FString NewName;
    if (Payload->TryGetStringField(TEXT("inputName"), NewName) && !NewName.IsEmpty()) {
      FString Lower = NewName.ToLower();
      if (Lower == TEXT("return value") || Lower == TEXT("returnvalue")) { SendAutomationError(Socket, RequestId, TEXT("Reserved input name."), TEXT("invalid-name")); return true; }
      Input->InputName = FName(*NewName); ChangedFields.Add(TEXT("inputName"));
    }
    FString NewTypeStr;
    if (Payload->TryGetStringField(TEXT("inputType"), NewTypeStr) && !NewTypeStr.IsEmpty()) {
      auto ParseInputType = [](const FString& T) -> EFunctionInputType {
        if (T == TEXT("Scalar") || T == TEXT("Float1")) return EFunctionInputType::FunctionInput_Scalar;
        if (T == TEXT("Vector2") || T == TEXT("Float2")) return EFunctionInputType::FunctionInput_Vector2;
        if (T == TEXT("Vector3") || T == TEXT("Float3")) return EFunctionInputType::FunctionInput_Vector3;
        if (T == TEXT("Vector4") || T == TEXT("Float4")) return EFunctionInputType::FunctionInput_Vector4;
        if (T == TEXT("Texture2D")) return EFunctionInputType::FunctionInput_Texture2D;
        if (T == TEXT("TextureCube")) return EFunctionInputType::FunctionInput_TextureCube;
        if (T == TEXT("VolumeTexture")) return EFunctionInputType::FunctionInput_VolumeTexture;
        if (T == TEXT("StaticBool") || T == TEXT("Bool")) return EFunctionInputType::FunctionInput_StaticBool;
        if (T == TEXT("MaterialAttributes")) return EFunctionInputType::FunctionInput_MaterialAttributes;
        return EFunctionInputType::FunctionInput_Vector3;
      };
      Input->InputType = ParseInputType(NewTypeStr); ChangedFields.Add(TEXT("inputType"));
    }
    FString NewDesc;
    if (Payload->TryGetStringField(TEXT("description"), NewDesc)) { Input->Description = NewDesc; ChangedFields.Add(TEXT("description")); }
    bool bUsePreview = false;
    if (Payload->TryGetBoolField(TEXT("usePreviewValueAsDefault"), bUsePreview)) { Input->bUsePreviewValueAsDefault = bUsePreview; ChangedFields.Add(TEXT("usePreviewValueAsDefault")); }
    double SortPriority = 0.0;
    if (Payload->TryGetNumberField(TEXT("sortPriority"), SortPriority)) { Input->SortPriority = (int32)SortPriority; ChangedFields.Add(TEXT("sortPriority")); }
    FString BlendRelevanceStr;
    if (Payload->TryGetStringField(TEXT("blendInputRelevance"), BlendRelevanceStr) && !BlendRelevanceStr.IsEmpty()) {
      if (BlendRelevanceStr == TEXT("Top") || BlendRelevanceStr == TEXT("Bottom")) {
        SendAutomationError(Socket, RequestId, TEXT("BlendInputRelevance Top/Bottom are diagnostics-only; use General for authoring."), TEXT("unsupported-type"));
        return true;
      }
      Input->BlendInputRelevance = EBlendInputRelevance::General; ChangedFields.Add(TEXT("blendInputRelevance"));
    }

    GraphOwner.Asset->PreEditChange(nullptr);
    GraphOwner.Asset->PostEditChange();
    GraphOwner.Asset->MarkPackageDirty();

    bool bSave = false; Payload->TryGetBoolField(TEXT("save"), bSave);
    bool bSaved = false;
    if (bSave) { bSaved = McpSafeAssetSave(GraphOwner.Asset); if (!bSaved) { SendAutomationError(Socket, RequestId, TEXT("Save failed."), TEXT("save-failed")); return true; } }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    TArray<TSharedPtr<FJsonValue>> ChangedArr;
    for (const FString& F : ChangedFields) ChangedArr.Add(MakeShared<FJsonValueString>(F));
    Result->SetArrayField(TEXT("changedFields"), ChangedArr);
    Result->SetStringField(TEXT("functionInputId"), Input->Id.ToString());
    const int32 ExprIdx = McpGraphExpressionIndex(GraphOwner, Input);
    Result->SetObjectField(TEXT("expressionIdentity"), McpBuildMaterialExpressionIdentity(GraphOwner, Input, ExprIdx));
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetBoolField(TEXT("dirty"), GraphOwner.Asset->GetOutermost()->IsDirty());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Function input updated."), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // update_function_output (task 2.3)
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("update_function_output"))
  {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("missing-field"));
      return true;
    }
    AssetPath = SanitizeProjectRelativePath(AssetPath);
    if (AssetPath.IsEmpty()) { SendAutomationError(Socket, RequestId, TEXT("Invalid assetPath."), TEXT("invalid-asset")); return true; }

    FMcpMaterialGraphOwner GraphOwner;
    FString GraphOwnerError;
    if (!McpResolveMaterialGraphOwner(AssetPath, GraphOwner, GraphOwnerError) || GraphOwner.bReadOnly) {
      SendAutomationError(Socket, RequestId, GraphOwnerError.IsEmpty() ? TEXT("Cannot mutate this asset.") : GraphOwnerError, TEXT("unsupported-operation"));
      return true;
    }

    UMaterialExpressionFunctionOutput* Output = Cast<UMaterialExpressionFunctionOutput>(McpAuthoringFindExpressionFromPayload_FunctionAuthoring(GraphOwner, Payload));
    if (!Output) { SendAutomationError(Socket, RequestId, TEXT("Target expression is not a UMaterialExpressionFunctionOutput."), TEXT("invalid-pin")); return true; }

    TArray<FString> ChangedFields;
    FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge", "UpdateFunctionOutput", "MCP update function output"));
    GraphOwner.Asset->Modify();
    Output->Modify();

    FString NewName;
    if (Payload->TryGetStringField(TEXT("outputName"), NewName) && !NewName.IsEmpty()) { Output->OutputName = FName(*NewName); ChangedFields.Add(TEXT("outputName")); }
    FString NewDesc;
    if (Payload->TryGetStringField(TEXT("description"), NewDesc)) { Output->Description = NewDesc; ChangedFields.Add(TEXT("description")); }

    GraphOwner.Asset->PreEditChange(nullptr);
    GraphOwner.Asset->PostEditChange();
    GraphOwner.Asset->MarkPackageDirty();

    bool bSave = false; Payload->TryGetBoolField(TEXT("save"), bSave);
    bool bSaved = false;
    if (bSave) { bSaved = McpSafeAssetSave(GraphOwner.Asset); if (!bSaved) { SendAutomationError(Socket, RequestId, TEXT("Save failed."), TEXT("save-failed")); return true; } }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    TArray<TSharedPtr<FJsonValue>> ChangedArr;
    for (const FString& F : ChangedFields) ChangedArr.Add(MakeShared<FJsonValueString>(F));
    Result->SetArrayField(TEXT("changedFields"), ChangedArr);
    Result->SetStringField(TEXT("functionOutputId"), Output->Id.ToString());
    const int32 ExprIdx = McpGraphExpressionIndex(GraphOwner, Output);
    Result->SetObjectField(TEXT("expressionIdentity"), McpBuildMaterialExpressionIdentity(GraphOwner, Output, ExprIdx));
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetBoolField(TEXT("dirty"), GraphOwner.Asset->GetOutermost()->IsDirty());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Function output updated."), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // add_material_function_call (task 2.5)
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_material_function_call"))
  {
    FString AssetPath, FunctionPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("missing-field")); return true;
    }
    if (!Payload->TryGetStringField(TEXT("functionPath"), FunctionPath) || FunctionPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'functionPath'."), TEXT("missing-field")); return true;
    }
    AssetPath = SanitizeProjectRelativePath(AssetPath);
    FunctionPath = SanitizeProjectRelativePath(FunctionPath);

    FMcpMaterialGraphOwner GraphOwner;
    FString GraphOwnerError;
    if (!McpResolveMaterialGraphOwner(AssetPath, GraphOwner, GraphOwnerError) || GraphOwner.bReadOnly) {
      SendAutomationError(Socket, RequestId, GraphOwnerError.IsEmpty() ? TEXT("Cannot mutate this asset.") : GraphOwnerError, TEXT("unsupported-operation")); return true;
    }

    UMaterialFunctionInterface* Func = LoadObject<UMaterialFunctionInterface>(nullptr, *FunctionPath);
    if (!Func) { SendAutomationError(Socket, RequestId, TEXT("Could not load material function."), TEXT("invalid-asset")); return true; }

    FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge", "AddMaterialFunctionCall", "MCP add material function call"));
    GraphOwner.Asset->Modify();

    UMaterialExpressionMaterialFunctionCall* FuncCall = NewObject<UMaterialExpressionMaterialFunctionCall>(
        GraphOwner.GraphSource ? GraphOwner.GraphSource : GraphOwner.Asset,
        UMaterialExpressionMaterialFunctionCall::StaticClass(), NAME_None, RF_Transactional);
    FuncCall->MaterialFunction = Func;
    FuncCall->MaterialExpressionGuid = FGuid::NewGuid();
    float X = 0, Y = 0;
    Payload->TryGetNumberField(TEXT("x"), X); Payload->TryGetNumberField(TEXT("y"), Y);
    FuncCall->MaterialExpressionEditorX = (int32)X; FuncCall->MaterialExpressionEditorY = (int32)Y;
    FuncCall->UpdateFromFunctionResource();

    TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressionsMutable(GraphOwner);
    if (Exprs) Exprs->Add(FuncCall);

    FString RebuildErr;
    McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

    bool bSave = false; Payload->TryGetBoolField(TEXT("save"), bSave);
    bool bSaved = false;
    if (bSave) { bSaved = McpSafeAssetSave(GraphOwner.Asset); if (!bSaved) { SendAutomationError(Socket, RequestId, TEXT("Save failed."), TEXT("save-failed")); return true; } }

    TArray<TSharedPtr<FJsonValue>> InputPins, OutputPins;
    for (const FFunctionExpressionInput& FEI : FuncCall->FunctionInputs) {
      TSharedPtr<FJsonObject> Pin = MakeShared<FJsonObject>();
      Pin->SetStringField(TEXT("id"), FEI.ExpressionInputId.ToString());
      Pin->SetStringField(TEXT("name"), FEI.Input.InputName.ToString());
      InputPins.Add(MakeShared<FJsonValueObject>(Pin));
    }
    for (const FFunctionExpressionOutput& FEO : FuncCall->FunctionOutputs) {
      TSharedPtr<FJsonObject> Pin = MakeShared<FJsonObject>();
      Pin->SetStringField(TEXT("id"), FEO.ExpressionOutputId.ToString());
      Pin->SetStringField(TEXT("name"), FEO.Output.OutputName.ToString());
      OutputPins.Add(MakeShared<FJsonValueObject>(Pin));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    const int32 ExprIdx = Exprs ? (Exprs->Num() - 1) : 0;
    Result->SetObjectField(TEXT("expressionIdentity"), McpBuildMaterialExpressionIdentity(GraphOwner, FuncCall, ExprIdx));
    Result->SetStringField(TEXT("functionPath"), Func->GetPathName());
    Result->SetArrayField(TEXT("inputPins"), InputPins);
    Result->SetArrayField(TEXT("outputPins"), OutputPins);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetBoolField(TEXT("dirty"), GraphOwner.Asset->GetOutermost()->IsDirty());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Material function call expression added."), Result);
    return true;
  }


  // --------------------------------------------------------------------------
  // update_material_function_call (task 2.5)
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("update_material_function_call"))
  {
    FString AssetPath, FunctionPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("missing-field")); return true;
    }
    if (!Payload->TryGetStringField(TEXT("functionPath"), FunctionPath) || FunctionPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'functionPath'."), TEXT("missing-field")); return true;
    }
    AssetPath = SanitizeProjectRelativePath(AssetPath);
    FunctionPath = SanitizeProjectRelativePath(FunctionPath);

    FMcpMaterialGraphOwner GraphOwner;
    FString GraphOwnerError;
    if (!McpResolveMaterialGraphOwner(AssetPath, GraphOwner, GraphOwnerError) || GraphOwner.bReadOnly) {
      SendAutomationError(Socket, RequestId, GraphOwnerError.IsEmpty() ? TEXT("Cannot mutate this asset.") : GraphOwnerError, TEXT("unsupported-operation")); return true;
    }

    UMaterialExpressionMaterialFunctionCall* Call = Cast<UMaterialExpressionMaterialFunctionCall>(McpAuthoringFindExpressionFromPayload_FunctionAuthoring(GraphOwner, Payload));
    if (!Call) { SendAutomationError(Socket, RequestId, TEXT("Target expression is not a UMaterialExpressionMaterialFunctionCall."), TEXT("invalid-pin")); return true; }

    UMaterialFunctionInterface* Func = LoadObject<UMaterialFunctionInterface>(nullptr, *FunctionPath);
    if (!Func) { SendAutomationError(Socket, RequestId, TEXT("Could not load material function."), TEXT("invalid-asset")); return true; }

    FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge", "UpdateMaterialFunctionCall", "MCP update material function call"));
    GraphOwner.Asset->Modify();
    Call->Modify();

    const int32 OldInputCount = Call->FunctionInputs.Num();
    const int32 OldOutputCount = Call->FunctionOutputs.Num();
    Call->MaterialFunction = Func;
    Call->UpdateFromFunctionResource();

    FString RebuildErr;
    McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

    bool bSave = false; Payload->TryGetBoolField(TEXT("save"), bSave);
    bool bSaved = false;
    if (bSave) { bSaved = McpSafeAssetSave(GraphOwner.Asset); if (!bSaved) { SendAutomationError(Socket, RequestId, TEXT("Save failed."), TEXT("save-failed")); return true; } }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    const int32 ExprIdx = McpGraphExpressionIndex(GraphOwner, Call);
    Result->SetObjectField(TEXT("expressionIdentity"), McpBuildMaterialExpressionIdentity(GraphOwner, Call, ExprIdx));
    Result->SetStringField(TEXT("functionPath"), Func->GetPathName());
    Result->SetNumberField(TEXT("changedInputs"), FMath::Abs(Call->FunctionInputs.Num() - OldInputCount));
    Result->SetNumberField(TEXT("changedOutputs"), FMath::Abs(Call->FunctionOutputs.Num() - OldOutputCount));
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetBoolField(TEXT("dirty"), GraphOwner.Asset->GetOutermost()->IsDirty());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Material function call updated."), Result);
    return true;
  }

  return false;
}

#undef LOAD_MATERIAL_OR_RETURN

#else // !WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_FunctionAuthoring(
    const FString& /*SubAction*/, const FString& /*RequestId*/,
    const TSharedPtr<FJsonObject>& /*Payload*/,
    TSharedPtr<FMcpBridgeWebSocket> /*Socket*/)
{
  return false;
}

#endif // WITH_EDITOR
