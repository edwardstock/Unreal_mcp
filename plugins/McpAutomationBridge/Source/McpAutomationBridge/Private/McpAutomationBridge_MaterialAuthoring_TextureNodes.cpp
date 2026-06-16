/**
 * McpAutomationBridge_MaterialAuthoring_TextureNodes.cpp
 *
 * Phase 8: Material Authoring - texture node sub-actions.
 *
 * Decomposed from McpAutomationBridge_MaterialAuthoringHandlers.cpp.
 * Handles texture expression sub-actions.
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
//   UObject* Material      // alias to GraphOwner.GraphSource for backward-compat:
//                          // NewObject(), PostEditChange() and MarkPackageDirty()
//                          // all work via virtual dispatch on UObject*. For
//                          // expression-collection access use
//                          // McpGetGraphExpressionsMutable(GraphOwner)
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

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_TextureNodes(
    const FString& SubAction,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {

    // --------------------------------------------------------------------------
    // add_texture_sample
    // --------------------------------------------------------------------------
    if (SubAction == TEXT("add_texture_sample")) {
        LOAD_GRAPH_OWNER_OR_RETURN();

        FString TexturePath, ParameterName, SamplerType;
        Payload->TryGetStringField(TEXT("texturePath"), TexturePath);
        Payload->TryGetStringField(TEXT("parameterName"), ParameterName);
        Payload->TryGetStringField(TEXT("samplerType"), SamplerType);

        // SECURITY: Validate texturePath if provided
        if (!TexturePath.IsEmpty()) {
            FString ValidatedTexturePath = SanitizeProjectRelativePath(TexturePath);
            if (ValidatedTexturePath.IsEmpty()) {
                SendAutomationError(Socket,
                    RequestId,
                    FString::Printf(TEXT("Invalid texturePath '%s': contains traversal sequences or invalid root"), *TexturePath),
                    TEXT("INVALID_PATH"));
                return true;
            }
            TexturePath = ValidatedTexturePath;
        }
        UMaterialExpressionTextureSampleParameter2D* TexSample = nullptr;
        if (!ParameterName.IsEmpty()) {
            TexSample = NewObject<UMaterialExpressionTextureSampleParameter2D>(
                Material,
                UMaterialExpressionTextureSampleParameter2D::StaticClass(),
                NAME_None,
                RF_Transactional);
            TexSample->ParameterName = FName(*ParameterName);
        }
        else {
            // Create a plain texture sample and cast to base type for the TexSample pointer
            UMaterialExpressionTextureSample* PlainSample = NewObject<UMaterialExpressionTextureSample>(
                Material,
                UMaterialExpressionTextureSample::StaticClass(),
                NAME_None,
                RF_Transactional);
            // Since we need to use TexSample for the rest of the code, we need to handle this separately
            if (!PlainSample) {
                SendAutomationError(Socket, RequestId, TEXT("Failed to create texture sample expression"), TEXT("CREATION_FAILED"));
                return true;
            }

            if (!TexturePath.IsEmpty()) {
                UTexture* Texture = LoadObject<UTexture>(nullptr, *TexturePath);
                if (Texture) {
                    PlainSample->Texture = Texture;
                }
            }

            // N6: auto-detect samplerType from texture when not provided
            if (!SamplerType.IsEmpty()) {
                PlainSample->SamplerType = McpParseSamplerTypeString(SamplerType);
            }
            else if (PlainSample->Texture) {
                PlainSample->SamplerType = McpInferSamplerTypeFromTexture(PlainSample->Texture);
            }

            PlainSample->MaterialExpressionEditorX = (int32)X;
            PlainSample->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
            if (auto* ExprArr = McpGetGraphExpressionsMutable(GraphOwner)) ExprArr->Add(PlainSample);
#endif

            {
                FString RebuildErr;
                McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);
            }

            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("nodeId"), PlainSample->MaterialExpressionGuid.ToString());
            SendAutomationResponse(Socket, RequestId, true, TEXT("Texture sample added."), Result);
            return true;
        }

        if (!TexSample) {
            SendAutomationError(Socket, RequestId, TEXT("Failed to create texture sample expression"), TEXT("CREATION_FAILED"));
            return true;
        }

        if (!TexturePath.IsEmpty()) {
            UTexture* Texture = LoadObject<UTexture>(nullptr, *TexturePath);
            if (Texture) {
                TexSample->Texture = Texture;
            }
        }

        // N6: auto-detect samplerType from texture when not provided
        if (!SamplerType.IsEmpty()) {
            TexSample->SamplerType = McpParseSamplerTypeString(SamplerType);
        }
        else if (TexSample->Texture) {
            TexSample->SamplerType = McpInferSamplerTypeFromTexture(TexSample->Texture);
        }

        TexSample->MaterialExpressionEditorX = (int32)X;
        TexSample->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
        if (auto* ExprArr = McpGetGraphExpressionsMutable(GraphOwner)) ExprArr->Add(TexSample);
#endif

        {
            FString RebuildErr;
            McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("nodeId"),
            TexSample->MaterialExpressionGuid.ToString());
        SendAutomationResponse(Socket,
            RequestId,
            true,
            TEXT("Texture sample added."),
            Result);
        return true;
    }


    // --------------------------------------------------------------------------
    // add_texture_coordinate
    // --------------------------------------------------------------------------
    if (SubAction == TEXT("add_texture_coordinate")) {
        LOAD_GRAPH_OWNER_OR_RETURN();

        int32 CoordIndex = 0;
        double UTiling = 1.0, VTiling = 1.0;
        Payload->TryGetNumberField(TEXT("coordinateIndex"), CoordIndex);
        Payload->TryGetNumberField(TEXT("uTiling"), UTiling);
        Payload->TryGetNumberField(TEXT("vTiling"), VTiling);

        UMaterialExpressionTextureCoordinate* TexCoord =
            NewObject<UMaterialExpressionTextureCoordinate>(
                Material,
                UMaterialExpressionTextureCoordinate::StaticClass(),
                NAME_None,
                RF_Transactional);
        TexCoord->CoordinateIndex = CoordIndex;
        TexCoord->UTiling = UTiling;
        TexCoord->VTiling = VTiling;
        TexCoord->MaterialExpressionEditorX = (int32)X;
        TexCoord->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
        if (auto* ExprArr = McpGetGraphExpressionsMutable(GraphOwner)) ExprArr->Add(TexCoord);
#endif

        {
            FString RebuildErr;
            McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("nodeId"),
            TexCoord->MaterialExpressionGuid.ToString());
        SendAutomationResponse(Socket,
            RequestId,
            true,
            TEXT("Texture coordinate added."),
            Result);
        return true;
    }


    // --------------------------------------------------------------------------
    // add_texture_object (task 2.4)
    // --------------------------------------------------------------------------
    if (SubAction == TEXT("add_texture_object")) {
        FString AssetPath, TexturePath;
        if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
            SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("missing-field"));
            return true;
        }
        if (!Payload->TryGetStringField(TEXT("texturePath"), TexturePath) || TexturePath.IsEmpty()) {
            SendAutomationError(Socket, RequestId, TEXT("Missing 'texturePath'."), TEXT("missing-field"));
            return true;
        }
        AssetPath = SanitizeProjectRelativePath(AssetPath);
        TexturePath = SanitizeProjectRelativePath(TexturePath);

        FMcpMaterialGraphOwner GraphOwner;
        FString GraphOwnerError;
        if (!McpResolveMaterialGraphOwner(AssetPath, GraphOwner, GraphOwnerError) || GraphOwner.bReadOnly) {
            SendAutomationError(Socket,
                RequestId,
                GraphOwnerError.IsEmpty() ? TEXT("Cannot mutate this asset.") : GraphOwnerError,
                TEXT("unsupported-operation"));
            return true;
        }

        UTexture* Texture = LoadObject<UTexture>(nullptr, *TexturePath);
        if (!Texture) {
            SendAutomationError(Socket, RequestId, TEXT("Could not load texture asset."), TEXT("invalid-asset"));
            return true;
        }

        FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge", "AddTextureObject", "MCP add texture object"));
        GraphOwner.Asset->Modify();

        UMaterialExpressionTextureObject* TexObj = NewObject<UMaterialExpressionTextureObject>(
            GraphOwner.GraphSource ? GraphOwner.GraphSource : GraphOwner.Asset,
            UMaterialExpressionTextureObject::StaticClass(),
            NAME_None,
            RF_Transactional);
        TexObj->Texture = Texture;
        float X = 0, Y = 0;
        Payload->TryGetNumberField(TEXT("x"), X);
        Payload->TryGetNumberField(TEXT("y"), Y);
        TexObj->MaterialExpressionEditorX = (int32)X;
        TexObj->MaterialExpressionEditorY = (int32)Y;
        TexObj->MaterialExpressionGuid = FGuid::NewGuid();

        TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressionsMutable(GraphOwner);
        if (Exprs) Exprs->Add(TexObj);

        FString RebuildErr;
        McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

        bool bSave = false;
        Payload->TryGetBoolField(TEXT("save"), bSave);
        bool bSaved = false;
        if (bSave) {
            bSaved = McpSafeAssetSave(GraphOwner.Asset);
            if (!bSaved) {
                SendAutomationError(Socket, RequestId, TEXT("Save failed."), TEXT("save-failed"));
                return true;
            }
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), AssetPath);
        const int32 ExprIdx = Exprs ? (Exprs->Num() - 1) : 0;
        Result->SetObjectField(TEXT("expressionIdentity"), McpBuildMaterialExpressionIdentity(GraphOwner, TexObj, ExprIdx));
        Result->SetBoolField(TEXT("saved"), bSaved);
        Result->SetBoolField(TEXT("dirty"), GraphOwner.Asset->GetOutermost()->IsDirty());
        SendAutomationResponse(Socket, RequestId, true, TEXT("Texture object expression added."), Result);
        return true;
    }


    // --------------------------------------------------------------------------
    // add_texture_object_parameter (task 2.4)
    // --------------------------------------------------------------------------
    if (SubAction == TEXT("add_texture_object_parameter")) {
        FString AssetPath, ParameterName, TexturePath;
        if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
            SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("missing-field"));
            return true;
        }
        if (!Payload->TryGetStringField(TEXT("parameterName"), ParameterName) || ParameterName.IsEmpty()) {
            SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."), TEXT("missing-field"));
            return true;
        }
        // N8: texturePath is optional — parameter can be declared without a default texture
        Payload->TryGetStringField(TEXT("texturePath"), TexturePath);
        AssetPath = SanitizeProjectRelativePath(AssetPath);

        FMcpMaterialGraphOwner GraphOwner;
        FString GraphOwnerError;
        if (!McpResolveMaterialGraphOwner(AssetPath, GraphOwner, GraphOwnerError) || GraphOwner.bReadOnly) {
            SendAutomationError(Socket,
                RequestId,
                GraphOwnerError.IsEmpty() ? TEXT("Cannot mutate this asset.") : GraphOwnerError,
                TEXT("unsupported-operation"));
            return true;
        }

        UTexture* Texture = nullptr;
        if (!TexturePath.IsEmpty()) {
            TexturePath = SanitizeProjectRelativePath(TexturePath);
            Texture = LoadObject<UTexture>(nullptr, *TexturePath);
            if (!Texture) {
                SendAutomationError(Socket,
                    RequestId,
                    FString::Printf(TEXT("Could not load texture '%s'."), *TexturePath),
                    TEXT("invalid-asset"));
                return true;
            }
        }

        FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge", "AddTextureObjectParameter", "MCP add texture object parameter"));
        GraphOwner.Asset->Modify();

        UMaterialExpressionTextureObjectParameter* TexParam = NewObject<UMaterialExpressionTextureObjectParameter>(
            GraphOwner.GraphSource ? GraphOwner.GraphSource : GraphOwner.Asset,
            UMaterialExpressionTextureObjectParameter::StaticClass(),
            NAME_None,
            RF_Transactional);
        TexParam->ParameterName = FName(*ParameterName);
        TexParam->Texture = Texture;
        {
            FString SamplerTypeStr;
            if (Payload->TryGetStringField(TEXT("samplerType"), SamplerTypeStr) && !SamplerTypeStr.IsEmpty()) {
                TexParam->SamplerType = McpParseSamplerTypeString(SamplerTypeStr);
            }
            else {
                TexParam->SamplerType = McpInferSamplerTypeFromTexture(Texture);
            }
        }

        FString Group;
        Payload->TryGetStringField(TEXT("group"), Group);
        if (!Group.IsEmpty()) TexParam->Group = FName(*Group);
        double SortPriority = 0;
        Payload->TryGetNumberField(TEXT("sortPriority"), SortPriority);
        TexParam->SortPriority = (int32)SortPriority;
        float X = 0, Y = 0;
        Payload->TryGetNumberField(TEXT("x"), X);
        Payload->TryGetNumberField(TEXT("y"), Y);
        TexParam->MaterialExpressionEditorX = (int32)X;
        TexParam->MaterialExpressionEditorY = (int32)Y;
        TexParam->MaterialExpressionGuid = FGuid::NewGuid();

        TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressionsMutable(GraphOwner);
        if (Exprs) Exprs->Add(TexParam);

        FString RebuildErr;
        McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

        bool bSave = false;
        Payload->TryGetBoolField(TEXT("save"), bSave);
        bool bSaved = false;
        if (bSave) {
            bSaved = McpSafeAssetSave(GraphOwner.Asset);
            if (!bSaved) {
                SendAutomationError(Socket, RequestId, TEXT("Save failed."), TEXT("save-failed"));
                return true;
            }
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), AssetPath);
        const int32 ExprIdx = Exprs ? (Exprs->Num() - 1) : 0;
        Result->SetObjectField(TEXT("expressionIdentity"), McpBuildMaterialExpressionIdentity(GraphOwner, TexParam, ExprIdx));
        TSharedPtr<FJsonObject> ParamInfo = MakeShared<FJsonObject>();
        ParamInfo->SetStringField(TEXT("name"), ParameterName);
        ParamInfo->SetStringField(TEXT("group"), Group);
        ParamInfo->SetNumberField(TEXT("sortPriority"), (double)SortPriority);
        Result->SetObjectField(TEXT("parameterInfo"), ParamInfo);
        Result->SetBoolField(TEXT("saved"), bSaved);
        Result->SetBoolField(TEXT("dirty"), GraphOwner.Asset->GetOutermost()->IsDirty());
        SendAutomationResponse(Socket, RequestId, true, TEXT("Texture object parameter expression added."), Result);
        return true;
    }


    // --------------------------------------------------------------------------
    // add_texture_sample_parameter (task 2.4)
    // --------------------------------------------------------------------------
    if (SubAction == TEXT("add_texture_sample_parameter")) {
        FString AssetPath, ParameterName, TexturePath;
        if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
            SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("missing-field"));
            return true;
        }
        if (!Payload->TryGetStringField(TEXT("parameterName"), ParameterName) || ParameterName.IsEmpty()) {
            SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."), TEXT("missing-field"));
            return true;
        }
        if (!Payload->TryGetStringField(TEXT("texturePath"), TexturePath) || TexturePath.IsEmpty()) {
            SendAutomationError(Socket, RequestId, TEXT("Missing 'texturePath'."), TEXT("missing-field"));
            return true;
        }
        AssetPath = SanitizeProjectRelativePath(AssetPath);
        TexturePath = SanitizeProjectRelativePath(TexturePath);

        FMcpMaterialGraphOwner GraphOwner;
        FString GraphOwnerError;
        if (!McpResolveMaterialGraphOwner(AssetPath, GraphOwner, GraphOwnerError) || GraphOwner.bReadOnly) {
            SendAutomationError(Socket,
                RequestId,
                GraphOwnerError.IsEmpty() ? TEXT("Cannot mutate this asset.") : GraphOwnerError,
                TEXT("unsupported-operation"));
            return true;
        }

        UTexture* Texture = LoadObject<UTexture>(nullptr, *TexturePath);
        if (!Texture) {
            SendAutomationError(Socket, RequestId, TEXT("Could not load texture asset."), TEXT("invalid-asset"));
            return true;
        }

        FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge", "AddTextureSampleParameter", "MCP add texture sample parameter"));
        GraphOwner.Asset->Modify();

        UMaterialExpressionTextureSampleParameter2D* TexSampleParam = NewObject<UMaterialExpressionTextureSampleParameter2D>(
            GraphOwner.GraphSource ? GraphOwner.GraphSource : GraphOwner.Asset,
            UMaterialExpressionTextureSampleParameter2D::StaticClass(),
            NAME_None,
            RF_Transactional);
        TexSampleParam->ParameterName = FName(*ParameterName);
        TexSampleParam->Texture = Texture;
        // N6: auto-detect samplerType from texture when not provided
        {
            FString SamplerTypeStr;
            if (Payload->TryGetStringField(TEXT("samplerType"), SamplerTypeStr) && !SamplerTypeStr.IsEmpty()) {
                TexSampleParam->SamplerType = McpParseSamplerTypeString(SamplerTypeStr);
            }
            else {
                TexSampleParam->SamplerType = McpInferSamplerTypeFromTexture(Texture);
            }
        }
        FString Group;
        Payload->TryGetStringField(TEXT("group"), Group);
        if (!Group.IsEmpty()) TexSampleParam->Group = FName(*Group);
        double SortPriority = 0;
        Payload->TryGetNumberField(TEXT("sortPriority"), SortPriority);
        TexSampleParam->SortPriority = (int32)SortPriority;
        float X = 0, Y = 0;
        Payload->TryGetNumberField(TEXT("x"), X);
        Payload->TryGetNumberField(TEXT("y"), Y);
        TexSampleParam->MaterialExpressionEditorX = (int32)X;
        TexSampleParam->MaterialExpressionEditorY = (int32)Y;
        TexSampleParam->MaterialExpressionGuid = FGuid::NewGuid();

        TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressionsMutable(GraphOwner);
        if (Exprs) Exprs->Add(TexSampleParam);

        FString RebuildErr;
        McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

        bool bSave = false;
        Payload->TryGetBoolField(TEXT("save"), bSave);
        bool bSaved = false;
        if (bSave) {
            bSaved = McpSafeAssetSave(GraphOwner.Asset);
            if (!bSaved) {
                SendAutomationError(Socket, RequestId, TEXT("Save failed."), TEXT("save-failed"));
                return true;
            }
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), AssetPath);
        const int32 ExprIdx = Exprs ? (Exprs->Num() - 1) : 0;
        Result->SetObjectField(TEXT("expressionIdentity"), McpBuildMaterialExpressionIdentity(GraphOwner, TexSampleParam, ExprIdx));
        TSharedPtr<FJsonObject> ParamInfo = MakeShared<FJsonObject>();
        ParamInfo->SetStringField(TEXT("name"), ParameterName);
        ParamInfo->SetStringField(TEXT("group"), Group);
        ParamInfo->SetNumberField(TEXT("sortPriority"), (double)SortPriority);
        Result->SetObjectField(TEXT("parameterInfo"), ParamInfo);
        Result->SetStringField(TEXT("samplerType"),
            TexSampleParam->SamplerType == SAMPLERTYPE_Color
                ? TEXT("Color")
                : TexSampleParam->SamplerType == SAMPLERTYPE_Normal
                ? TEXT("Normal")
                : TexSampleParam->SamplerType == SAMPLERTYPE_Masks
                ? TEXT("Masks")
                : TexSampleParam->SamplerType == SAMPLERTYPE_Grayscale
                ? TEXT("Grayscale")
                : TEXT("Default"));
        Result->SetBoolField(TEXT("saved"), bSaved);
        Result->SetBoolField(TEXT("dirty"), GraphOwner.Asset->GetOutermost()->IsDirty());
        SendAutomationResponse(Socket, RequestId, true, TEXT("Texture sample parameter expression added."), Result);
        return true;
    }

    return false;
}

#undef LOAD_GRAPH_OWNER_OR_RETURN

#else // !WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleAuthoring_TextureNodes(
    const FString& /*SubAction*/,
    const FString& /*RequestId*/,
    const TSharedPtr<FJsonObject>& /*Payload*/,
    TSharedPtr<FMcpBridgeWebSocket> /*Socket*/) {
    return false;
}

#endif // WITH_EDITOR
