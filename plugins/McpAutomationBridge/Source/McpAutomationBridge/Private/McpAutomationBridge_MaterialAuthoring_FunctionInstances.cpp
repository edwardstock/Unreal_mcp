/**
 * McpAutomationBridge_MaterialAuthoring_FunctionInstances.cpp
 *
 * Phase 8: Material Authoring - function instance sub-actions.
 *
 * Decomposed from McpAutomationBridge_MaterialAuthoringHandlers.cpp.
 * Handles material function instance sub-actions.
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

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_FunctionInstances(
    const FString& SubAction, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{

  // --------------------------------------------------------------------------
  // Material function instance operations (tasks 4.1-4.5)
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("create_material_function_instance") ||
      SubAction == TEXT("get_material_function_instance_info") ||
      SubAction == TEXT("set_material_function_instance_parent") ||
      SubAction == TEXT("get_material_function_instance_parameters") ||
      SubAction == TEXT("set_material_function_instance_parameter") ||
      SubAction == TEXT("reset_material_function_instance_parameter") ||
      SubAction == TEXT("clear_material_function_instance_parameters") ||
      SubAction == TEXT("bulk_set_material_function_instance_parameters"))
  {
    if (SubAction == TEXT("create_material_function_instance"))
    {
      FString Name, Path, ParentPath, InstanceKind;
      if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'name'."), TEXT("missing-field")); return true;
      }
      if (!Payload->TryGetStringField(TEXT("path"), Path) || Path.IsEmpty()) {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'path'."), TEXT("missing-field")); return true;
      }
      if (!Payload->TryGetStringField(TEXT("parentPath"), ParentPath) || ParentPath.IsEmpty()) {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'parentPath'."), TEXT("missing-field")); return true;
      }
      Payload->TryGetStringField(TEXT("instanceKind"), InstanceKind);
      if (InstanceKind.IsEmpty()) InstanceKind = TEXT("function");

      ParentPath = SanitizeProjectRelativePath(ParentPath);
      UMaterialFunctionInterface* ParentFunc = LoadObject<UMaterialFunctionInterface>(nullptr, *ParentPath);
      if (!ParentFunc) {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Could not load parent function '%s'."), *ParentPath), TEXT("invalid-asset")); return true;
      }

      // Validate instanceKind vs parent type
      if (InstanceKind == TEXT("materialLayer") && !Cast<UMaterialFunctionMaterialLayer>(ParentFunc)) {
        SendAutomationError(Socket, RequestId, TEXT("Parent is not a UMaterialFunctionMaterialLayer for instanceKind=materialLayer."), TEXT("unsupported-asset-type")); return true;
      }
      if (InstanceKind == TEXT("materialLayerBlend") && !Cast<UMaterialFunctionMaterialLayerBlend>(ParentFunc)) {
        SendAutomationError(Socket, RequestId, TEXT("Parent is not a UMaterialFunctionMaterialLayerBlend for instanceKind=materialLayerBlend."), TEXT("unsupported-asset-type")); return true;
      }

      Path = SanitizeProjectRelativePath(Path);
      FString FullAssetPath = FString::Printf(TEXT("%s/%s"), *Path, *Name);

      UFactory* Factory = nullptr;
      if (InstanceKind == TEXT("materialLayer")) {
        auto* F = NewObject<UMaterialFunctionMaterialLayerInstanceFactory>();
        F->InitialParent = ParentFunc; Factory = F;
      } else if (InstanceKind == TEXT("materialLayerBlend")) {
        auto* F = NewObject<UMaterialFunctionMaterialLayerBlendInstanceFactory>();
        F->InitialParent = ParentFunc; Factory = F;
      } else {
        auto* F = NewObject<UMaterialFunctionInstanceFactory>();
        F->InitialParent = ParentFunc; Factory = F;
      }

      IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
      UObject* NewAsset = AssetTools.CreateAsset(Name, Path, nullptr, Factory);
      if (!NewAsset) {
        SendAutomationError(Socket, RequestId, TEXT("Failed to create material function instance asset."), TEXT("invalid-asset")); return true;
      }

      UMaterialFunctionInstance* NewInstance = Cast<UMaterialFunctionInstance>(NewAsset);
      if (!NewInstance) {
        SendAutomationError(Socket, RequestId, TEXT("Created asset is not a UMaterialFunctionInstance."), TEXT("invalid-asset")); return true;
      }

      NewInstance->UpdateParameterSet();
      NewInstance->MarkPackageDirty();

      bool bSave = false; Payload->TryGetBoolField(TEXT("save"), bSave);
      bool bSaved = false;
      if (bSave) { bSaved = McpSafeAssetSave(NewInstance); }

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("assetPath"), NewInstance->GetPathName());
      Result->SetStringField(TEXT("assetClass"), NewInstance->GetClass()->GetName());
      Result->SetStringField(TEXT("parentPath"), ParentFunc->GetPathName());
      UMaterialFunctionInterface* BaseFn = NewInstance->GetBaseFunction();
      Result->SetStringField(TEXT("baseFunctionPath"), BaseFn ? BaseFn->GetPathName() : TEXT(""));
      Result->SetBoolField(TEXT("saved"), bSaved);
      Result->SetBoolField(TEXT("dirty"), NewInstance->GetOutermost()->IsDirty());
      SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Material function instance '%s' created."), *Name), Result);
      return true;
    }

    // All remaining function instance subActions need an existing instance
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("missing-field")); return true;
    }
    AssetPath = SanitizeProjectRelativePath(AssetPath);
    UMaterialFunctionInstance* FuncInst = LoadObject<UMaterialFunctionInstance>(nullptr, *AssetPath);
    if (!FuncInst) { SendAutomationError(Socket, RequestId, TEXT("Could not load UMaterialFunctionInstance."), TEXT("invalid-asset")); return true; }

    if (SubAction == TEXT("get_material_function_instance_info"))
    {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("assetPath"), AssetPath);
      UMaterialFunctionInterface* Parent = FuncInst->Parent;
      Result->SetStringField(TEXT("parentPath"), Parent ? Parent->GetPathName() : TEXT(""));
      UMaterialFunctionInterface* Base = FuncInst->GetBaseFunction();
      Result->SetStringField(TEXT("baseFunctionPath"), Base ? Base->GetPathName() : TEXT(""));
      TArray<TSharedPtr<FJsonValue>> ParamNames;
      for (const auto& P : FuncInst->ScalarParameterValues) ParamNames.Add(MakeShared<FJsonValueString>(P.ParameterInfo.Name.ToString()));
      for (const auto& P : FuncInst->VectorParameterValues) ParamNames.Add(MakeShared<FJsonValueString>(P.ParameterInfo.Name.ToString()));
      for (const auto& P : FuncInst->TextureParameterValues) ParamNames.Add(MakeShared<FJsonValueString>(P.ParameterInfo.Name.ToString()));
      Result->SetArrayField(TEXT("parameterNames"), ParamNames);
      Result->SetNumberField(TEXT("explicitOverrides"), ParamNames.Num());
      SendAutomationResponse(Socket, RequestId, true, TEXT("Material function instance info retrieved."), Result);
      return true;
    }

    if (SubAction == TEXT("set_material_function_instance_parent"))
    {
      FString ParentPath;
      if (!Payload->TryGetStringField(TEXT("parentPath"), ParentPath) || ParentPath.IsEmpty()) {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'parentPath'."), TEXT("missing-field")); return true;
      }
      ParentPath = SanitizeProjectRelativePath(ParentPath);
      UMaterialFunctionInterface* NewParent = LoadObject<UMaterialFunctionInterface>(nullptr, *ParentPath);
      if (!NewParent) { SendAutomationError(Socket, RequestId, TEXT("Could not load parent function."), TEXT("invalid-asset")); return true; }
      if (NewParent == FuncInst) { SendAutomationError(Socket, RequestId, TEXT("Cannot parent to self."), TEXT("unsupported-operation")); return true; }

      // Cycle check
      for (UMaterialFunctionInterface* Cursor = NewParent; Cursor; ) {
        UMaterialFunctionInstance* CurInst = Cast<UMaterialFunctionInstance>(Cursor);
        if (!CurInst) break;
        if (CurInst == FuncInst) { SendAutomationError(Socket, RequestId, TEXT("Cycle detected in function instance parent chain."), TEXT("unsupported-operation")); return true; }
        Cursor = CurInst->Parent;
      }

      FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge", "SetFuncInstParent", "MCP set material function instance parent"));
      FuncInst->Modify();
      FuncInst->SetParent(NewParent);
      FuncInst->UpdateParameterSet();
      FuncInst->MarkPackageDirty();

      bool bSave = false; Payload->TryGetBoolField(TEXT("save"), bSave);
      bool bSaved = false;
      if (bSave) { bSaved = McpSafeAssetSave(FuncInst); if (!bSaved) { SendAutomationError(Socket, RequestId, TEXT("Save failed."), TEXT("save-failed")); return true; } }

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("assetPath"), AssetPath);
      Result->SetStringField(TEXT("parentPath"), NewParent->GetPathName());
      UMaterialFunctionInterface* Base = FuncInst->GetBaseFunction();
      Result->SetStringField(TEXT("baseFunctionPath"), Base ? Base->GetPathName() : TEXT(""));
      Result->SetArrayField(TEXT("invalidatedOverrideNames"), TArray<TSharedPtr<FJsonValue>>());
      Result->SetBoolField(TEXT("saved"), bSaved);
      Result->SetBoolField(TEXT("dirty"), FuncInst->GetOutermost()->IsDirty());
      SendAutomationResponse(Socket, RequestId, true, TEXT("Material function instance parent set."), Result);
      return true;
    }

    if (SubAction == TEXT("get_material_function_instance_parameters"))
    {
      TArray<TSharedPtr<FJsonValue>> Params;
      for (const auto& P : FuncInst->ScalarParameterValues) {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"), P.ParameterInfo.Name.ToString());
        Obj->SetStringField(TEXT("type"), TEXT("Scalar"));
        Obj->SetNumberField(TEXT("explicitValue"), P.ParameterValue);
        Obj->SetBoolField(TEXT("isOverride"), true);
        Params.Add(MakeShared<FJsonValueObject>(Obj));
      }
      for (const auto& P : FuncInst->VectorParameterValues) {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"), P.ParameterInfo.Name.ToString());
        Obj->SetStringField(TEXT("type"), TEXT("Vector"));
        TSharedPtr<FJsonObject> Col = MakeShared<FJsonObject>();
        Col->SetNumberField(TEXT("r"), P.ParameterValue.R); Col->SetNumberField(TEXT("g"), P.ParameterValue.G);
        Col->SetNumberField(TEXT("b"), P.ParameterValue.B); Col->SetNumberField(TEXT("a"), P.ParameterValue.A);
        Obj->SetObjectField(TEXT("explicitValue"), Col);
        Obj->SetBoolField(TEXT("isOverride"), true);
        Params.Add(MakeShared<FJsonValueObject>(Obj));
      }
      for (const auto& P : FuncInst->TextureParameterValues) {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"), P.ParameterInfo.Name.ToString());
        Obj->SetStringField(TEXT("type"), TEXT("Texture"));
        Obj->SetStringField(TEXT("explicitValue"), P.ParameterValue.Get() ? P.ParameterValue.Get()->GetPathName() : TEXT(""));
        Obj->SetBoolField(TEXT("isOverride"), true);
        Params.Add(MakeShared<FJsonValueObject>(Obj));
      }
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("assetPath"), AssetPath);
      Result->SetArrayField(TEXT("parameters"), Params);
      SendAutomationResponse(Socket, RequestId, true, TEXT("Material function instance parameters retrieved."), Result);
      return true;
    }

    if (SubAction == TEXT("clear_material_function_instance_parameters"))
    {
      FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge", "ClearFuncInstParams", "MCP clear function instance parameters"));
      FuncInst->Modify();
      const int32 Count = FuncInst->ScalarParameterValues.Num() + FuncInst->VectorParameterValues.Num() + FuncInst->TextureParameterValues.Num();
      FuncInst->ScalarParameterValues.Empty();
      FuncInst->VectorParameterValues.Empty();
      FuncInst->TextureParameterValues.Empty();
      FuncInst->UpdateParameterSet();
      FuncInst->MarkPackageDirty();

      bool bSave = false; Payload->TryGetBoolField(TEXT("save"), bSave);
      bool bSaved = false;
      if (bSave) { bSaved = McpSafeAssetSave(FuncInst); if (!bSaved) { SendAutomationError(Socket, RequestId, TEXT("Save failed."), TEXT("save-failed")); return true; } }

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("assetPath"), AssetPath);
      Result->SetNumberField(TEXT("clearedCount"), Count);
      Result->SetNumberField(TEXT("remainingExplicitOverrideCount"), 0);
      Result->SetBoolField(TEXT("saved"), bSaved);
      Result->SetBoolField(TEXT("dirty"), FuncInst->GetOutermost()->IsDirty());
      SendAutomationResponse(Socket, RequestId, true, TEXT("Material function instance parameters cleared."), Result);
      return true;
    }

    // Helper: apply function instance parameter override
    auto ApplyFuncInstOverride = [&](const TSharedPtr<FJsonObject>& ParamObj, const TSharedPtr<FJsonObject>& ValueObj) -> FString
    {
      if (!ParamObj || !ValueObj) return TEXT("Invalid parameter or value object.");
      FString ParamName; ParamObj->TryGetStringField(TEXT("name"), ParamName);
      FString TypeStr; ParamObj->TryGetStringField(TEXT("type"), TypeStr);
      FMaterialParameterInfo FInstParamInfo{FName(*ParamName)};

      if (TypeStr == TEXT("Scalar")) {
        double Val = 0; ValueObj->TryGetNumberField(TEXT("value"), Val);
        bool bFound = false;
        for (auto& P : FuncInst->ScalarParameterValues) { if (P.ParameterInfo.Name == FName(*ParamName)) { P.ParameterValue = (float)Val; bFound = true; break; } }
        if (!bFound) { FScalarParameterValue NP; NP.ParameterInfo = FInstParamInfo; NP.ParameterValue = (float)Val; FuncInst->ScalarParameterValues.Add(NP); }
      } else if (TypeStr == TEXT("Vector")) {
        double R = 0, G = 0, B = 0, A = 1;
        ValueObj->TryGetNumberField(TEXT("r"), R); ValueObj->TryGetNumberField(TEXT("g"), G);
        ValueObj->TryGetNumberField(TEXT("b"), B); ValueObj->TryGetNumberField(TEXT("a"), A);
        bool bFound = false;
        for (auto& P : FuncInst->VectorParameterValues) { if (P.ParameterInfo.Name == FName(*ParamName)) { P.ParameterValue = FLinearColor((float)R,(float)G,(float)B,(float)A); bFound = true; break; } }
        if (!bFound) { FVectorParameterValue NP; NP.ParameterInfo = FInstParamInfo; NP.ParameterValue = FLinearColor((float)R,(float)G,(float)B,(float)A); FuncInst->VectorParameterValues.Add(NP); }
      } else if (TypeStr == TEXT("Texture")) {
        FString TP; ValueObj->TryGetStringField(TEXT("assetPath"), TP);
        UTexture* Tex = LoadObject<UTexture>(nullptr, *SanitizeProjectRelativePath(TP));
        if (!Tex) return FString::Printf(TEXT("Could not load texture '%s'."), *TP);
        bool bFound = false;
        for (auto& P : FuncInst->TextureParameterValues) { if (P.ParameterInfo.Name == FName(*ParamName)) { P.ParameterValue = Tex; bFound = true; break; } }
        if (!bFound) { FTextureParameterValue NP; NP.ParameterInfo = FInstParamInfo; NP.ParameterValue = Tex; FuncInst->TextureParameterValues.Add(NP); }
      } else {
        return FString::Printf(TEXT("unsupported-type: '%s'."), *TypeStr);
      }
      return TEXT("");
    };

    if (SubAction == TEXT("set_material_function_instance_parameter"))
    {
      const TSharedPtr<FJsonObject>* ParamObj = nullptr;
      if (!Payload->TryGetObjectField(TEXT("parameter"), ParamObj) || !ParamObj) {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'parameter' object."), TEXT("missing-field")); return true;
      }
      const TSharedPtr<FJsonObject>* ValueObj = nullptr;
      Payload->TryGetObjectField(TEXT("value"), ValueObj);

      FString ParamName; (*ParamObj)->TryGetStringField(TEXT("name"), ParamName);

      FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge", "SetFuncInstParam", "MCP set function instance parameter"));
      FuncInst->Modify();
      FString Err = ApplyFuncInstOverride(*ParamObj, ValueObj ? *ValueObj : TSharedPtr<FJsonObject>());
      if (!Err.IsEmpty()) {
        if (Err.Contains(TEXT("unknown-parameter"))) SendAutomationError(Socket, RequestId, Err, TEXT("unknown-parameter"));
        else if (Err.Contains(TEXT("unsupported-type"))) SendAutomationError(Socket, RequestId, Err, TEXT("unsupported-type"));
        else SendAutomationError(Socket, RequestId, Err, TEXT("invalid-asset"));
        return true;
      }
      FuncInst->UpdateParameterSet();
      FuncInst->MarkPackageDirty();

      bool bSave = false; Payload->TryGetBoolField(TEXT("save"), bSave);
      bool bSaved = false;
      if (bSave) { bSaved = McpSafeAssetSave(FuncInst); if (!bSaved) { SendAutomationError(Socket, RequestId, TEXT("Save failed."), TEXT("save-failed")); return true; } }

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("assetPath"), AssetPath);
      Result->SetObjectField(TEXT("parameter"), *ParamObj);
      Result->SetBoolField(TEXT("saved"), bSaved);
      Result->SetBoolField(TEXT("dirty"), FuncInst->GetOutermost()->IsDirty());
      SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Function instance parameter '%s' set."), *ParamName), Result);
      return true;
    }

    if (SubAction == TEXT("reset_material_function_instance_parameter"))
    {
      const TSharedPtr<FJsonObject>* ParamObj = nullptr;
      if (!Payload->TryGetObjectField(TEXT("parameter"), ParamObj) || !ParamObj) {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'parameter' object."), TEXT("missing-field")); return true;
      }
      FString ParamName; (*ParamObj)->TryGetStringField(TEXT("name"), ParamName);
      const FName PN(*ParamName);

      FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge", "ResetFuncInstParam", "MCP reset function instance parameter"));
      FuncInst->Modify();
      FuncInst->ScalarParameterValues.RemoveAll([&PN](const FScalarParameterValue& V){ return V.ParameterInfo.Name == PN; });
      FuncInst->VectorParameterValues.RemoveAll([&PN](const FVectorParameterValue& V){ return V.ParameterInfo.Name == PN; });
      FuncInst->TextureParameterValues.RemoveAll([&PN](const FTextureParameterValue& V){ return V.ParameterInfo.Name == PN; });
      FuncInst->UpdateParameterSet();
      FuncInst->MarkPackageDirty();

      bool bSave = false; Payload->TryGetBoolField(TEXT("save"), bSave);
      bool bSaved = false;
      if (bSave) { bSaved = McpSafeAssetSave(FuncInst); if (!bSaved) { SendAutomationError(Socket, RequestId, TEXT("Save failed."), TEXT("save-failed")); return true; } }

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("assetPath"), AssetPath);
      Result->SetObjectField(TEXT("parameter"), *ParamObj);
      Result->SetBoolField(TEXT("saved"), bSaved);
      Result->SetBoolField(TEXT("dirty"), FuncInst->GetOutermost()->IsDirty());
      SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Function instance parameter '%s' reset."), *ParamName), Result);
      return true;
    }

    if (SubAction == TEXT("bulk_set_material_function_instance_parameters"))
    {
      const TArray<TSharedPtr<FJsonValue>>* Overrides = nullptr;
      if (!Payload->TryGetArrayField(TEXT("overrides"), Overrides) || !Overrides) {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'overrides' array."), TEXT("missing-field")); return true;
      }

      // Pre-validate
      for (const auto& OV : *Overrides) {
        const TSharedPtr<FJsonObject>* OObj = nullptr;
        if (!OV->TryGetObject(OObj) || !OObj) {
          SendAutomationError(Socket, RequestId, TEXT("Invalid override entry."), TEXT("partial-failure-not-allowed")); return true;
        }
        const TSharedPtr<FJsonObject>* PObj = nullptr;
        (*OObj)->TryGetObjectField(TEXT("parameter"), PObj);
        const TSharedPtr<FJsonObject>* VObj = nullptr;
        (*OObj)->TryGetObjectField(TEXT("value"), VObj);
        if (!PObj || !VObj) { SendAutomationError(Socket, RequestId, TEXT("Override missing parameter or value."), TEXT("partial-failure-not-allowed")); return true; }
        FString PN; (*PObj)->TryGetStringField(TEXT("name"), PN);
        if (PN.IsEmpty()) { SendAutomationError(Socket, RequestId, TEXT("Override missing parameter name."), TEXT("partial-failure-not-allowed")); return true; }
      }

      FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge", "BulkSetFuncInstParams", "MCP bulk set function instance parameters"));
      FuncInst->Modify();
      TArray<TSharedPtr<FJsonValue>> AppliedParams;
      for (const auto& OV : *Overrides) {
        const TSharedPtr<FJsonObject>* OObj = nullptr; OV->TryGetObject(OObj);
        const TSharedPtr<FJsonObject>* PObj = nullptr; (*OObj)->TryGetObjectField(TEXT("parameter"), PObj);
        const TSharedPtr<FJsonObject>* VObj = nullptr; (*OObj)->TryGetObjectField(TEXT("value"), VObj);
        ApplyFuncInstOverride(*PObj, *VObj);
        AppliedParams.Add(MakeShared<FJsonValueObject>(*PObj));
      }
      FuncInst->UpdateParameterSet();
      FuncInst->MarkPackageDirty();

      bool bSave = false; Payload->TryGetBoolField(TEXT("save"), bSave);
      bool bSaved = false;
      if (bSave) { bSaved = McpSafeAssetSave(FuncInst); if (!bSaved) { SendAutomationError(Socket, RequestId, TEXT("Save failed."), TEXT("save-failed")); return true; } }

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("assetPath"), AssetPath);
      Result->SetNumberField(TEXT("appliedCount"), AppliedParams.Num());
      Result->SetArrayField(TEXT("parameters"), AppliedParams);
      Result->SetBoolField(TEXT("saved"), bSaved);
      Result->SetBoolField(TEXT("dirty"), FuncInst->GetOutermost()->IsDirty());
      SendAutomationResponse(Socket, RequestId, true, TEXT("Bulk function instance parameters applied."), Result);
      return true;
    }
  }

  return false;
}

#else // !WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_FunctionInstances(
    const FString& /*SubAction*/, const FString& /*RequestId*/,
    const TSharedPtr<FJsonObject>& /*Payload*/,
    TSharedPtr<FMcpBridgeWebSocket> /*Socket*/)
{
  return false;
}

#endif // WITH_EDITOR
