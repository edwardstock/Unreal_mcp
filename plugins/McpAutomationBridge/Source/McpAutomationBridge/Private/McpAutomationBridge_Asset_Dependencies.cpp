// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_Asset_Dependencies.cpp
//
// G.2: per-domain split of manage_asset handler bodies. The dispatcher in
// McpAutomationBridge_AssetWorkflowHandlers.cpp routes to these handlers by
// canonical plural action name.

#include "McpVersionCompatibility.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/ScopeExit.h"
#include "UObject/MetaData.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"
#include "McpSafeOperations.h"
#include "McpFunctionInputTypeName.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "AssetViewUtils.h"
#include "EditorAssetLibrary.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "IAssetTools.h"
#include "Engine/StaticMesh.h"
#include "ObjectTools.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/FileHelper.h"

#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlHelpers.h"
#include "SourceControlOperations.h"

#include "ImageUtils.h"

#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInstance.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVectorParameter.h"

// G.2: shared material-graph helpers defined in McpAutomationBridge_AssetWorkflowHandlers.cpp.
extern int32 McpExpressionIndex(const FMcpMaterialGraphOwner& Owner, const UMaterialExpression* Expr);
extern void McpAddExpressionIdentity(
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpression* Expr,
    int32 Index,
    const TSharedRef<FJsonObject>& Obj);
extern void McpEmitInputPinJson(
    const FMcpMaterialGraphOwner& Owner,
    const FExpressionInput* Input,
    const FString& PinName,
    const TSharedRef<FJsonObject>& Out);
extern FString McpGetOutputName(UMaterialExpression* Expression, int32 OutputIndex, bool& bOutResolved);
extern TConstArrayView<TObjectPtr<UMaterialExpressionComment>> McpGetGraphComments(const FMcpMaterialGraphOwner& Owner);
extern TSharedRef<FJsonObject> McpBuildCommentJson(UMaterialExpressionComment* Comment, int32 IndexHint);
#endif // WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleGetDependencies(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Validate path
  if (!IsValidAssetPath(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Invalid asset path"),
                           nullptr, TEXT("INVALID_PATH"));
    return true;
  }

  // Check if asset exists - return error for non-existent assets
  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationError(Socket, RequestId, 
                        FString::Printf(TEXT("Asset not found: %s"), *AssetPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  bool bRecursive = false;
  Payload->TryGetBoolField(TEXT("recursive"), bRecursive);

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  TArray<FName> Dependencies;
  UE::AssetRegistry::EDependencyCategory Category =
      UE::AssetRegistry::EDependencyCategory::Package;
  AssetRegistryModule.Get().GetDependencies(FName(*AssetPath), Dependencies);

  TArray<TSharedPtr<FJsonValue>> DepArray;
  for (const FName &Dep : Dependencies) {
    DepArray.Add(MakeShared<FJsonValueString>(Dep.ToString()));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetArrayField(TEXT("dependencies"), DepArray);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Dependencies retrieved"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// ============================================================================
// ANALYZE GRAPH
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleAnalyzeGraph(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("analyze_graph"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("analyze_graph payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), AssetPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (AssetPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Try to resolve as a material-type asset (UMaterial / UMaterialFunction / UMaterialFunctionInstance)
  FMcpMaterialGraphOwner GraphOwner;
  FString GraphOwnerError;
  if (McpResolveMaterialGraphOwner(AssetPath, GraphOwner, GraphOwnerError))
  {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, GraphOwner.Asset);
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetStringField(TEXT("assetClass"), GraphOwner.Asset->GetClass()->GetName());

    const TArray<TObjectPtr<UMaterialExpression>>* Expressions = McpGetGraphExpressions(GraphOwner);
    int32 NodeCount = Expressions ? Expressions->Num() : 0;
    int32 ParameterCount = 0;
    int32 TextureSampleCount = 0;
    TArray<FString> ParameterNames;

    if (Expressions)
    {
      for (UMaterialExpression* Expr : *Expressions)
      {
        if (!Expr) continue;
        if (UMaterialExpressionParameter* Param = Cast<UMaterialExpressionParameter>(Expr))
        {
          ParameterCount++;
          ParameterNames.Add(Param->ParameterName.ToString());
        }
        if (Cast<UMaterialExpressionTextureSample>(Expr))
          TextureSampleCount++;
      }
    }

    Result->SetNumberField(TEXT("nodeCount"), NodeCount);
    Result->SetNumberField(TEXT("parameterCount"), ParameterCount);
    Result->SetNumberField(TEXT("textureSampleCount"), TextureSampleCount);

    // NEW6: comment count (lives in EditorComments, separate from regular expressions)
    const TConstArrayView<TObjectPtr<UMaterialExpressionComment>> Comments = McpGetGraphComments(GraphOwner);
    Result->SetNumberField(TEXT("commentCount"), Comments.Num());

    TArray<TSharedPtr<FJsonValue>> ParamArray;
    for (const FString& ParamName : ParameterNames)
      ParamArray.Add(MakeShared<FJsonValueString>(ParamName));
    Result->SetArrayField(TEXT("parameters"), ParamArray);

    if (GraphOwner.Kind == EMcpMaterialGraphOwnerKind::Material)
    {
      UMaterial* Material = CastChecked<UMaterial>(GraphOwner.GraphSource);
      Result->SetStringField(TEXT("graphType"), TEXT("Material"));
      Result->SetBoolField(TEXT("isMaterialInstance"), false);
      Result->SetBoolField(TEXT("isTwoSided"), Material->TwoSided);
      Result->SetBoolField(TEXT("isMasked"), Material->IsMasked());
#if WITH_EDITORONLY_DATA
      Result->SetStringField(TEXT("blendMode"),
        StaticEnum<EBlendMode>()->GetNameStringByValue((int64)Material->GetBlendMode()));
      FString ShadingModelName = TEXT("Unknown");
      FMaterialShadingModelField ShadingModels = Material->GetShadingModels();
      if (ShadingModels.HasShadingModel(MSM_DefaultLit))       ShadingModelName = TEXT("DefaultLit");
      else if (ShadingModels.HasShadingModel(MSM_Subsurface))  ShadingModelName = TEXT("Subsurface");
      else if (ShadingModels.HasShadingModel(MSM_Unlit))       ShadingModelName = TEXT("Unlit");
      else if (ShadingModels.HasShadingModel(MSM_ClearCoat))   ShadingModelName = TEXT("ClearCoat");
      else if (ShadingModels.HasShadingModel(MSM_SubsurfaceProfile))   ShadingModelName = TEXT("SubsurfaceProfile");
      else if (ShadingModels.HasShadingModel(MSM_PreintegratedSkin))   ShadingModelName = TEXT("PreintegratedSkin");
      Result->SetStringField(TEXT("shadingModel"), ShadingModelName);
#endif
    }
    else if (GraphOwner.Kind == EMcpMaterialGraphOwnerKind::MaterialFunction)
    {
      UMaterialFunction* Func = CastChecked<UMaterialFunction>(GraphOwner.GraphSource);
      Result->SetStringField(TEXT("graphType"), TEXT("MaterialFunction"));
      Result->SetStringField(TEXT("description"), Func->Description);
      Result->SetBoolField(TEXT("exposedToLibrary"), Func->bExposeToLibrary != 0);

      int32 FunctionInputCount = 0;
      int32 FunctionOutputCount = 0;
      if (Expressions)
      {
        for (UMaterialExpression* Expr : *Expressions)
        {
          FunctionInputCount += Cast<UMaterialExpressionFunctionInput>(Expr) ? 1 : 0;
          FunctionOutputCount += Cast<UMaterialExpressionFunctionOutput>(Expr) ? 1 : 0;
        }
      }
      Result->SetNumberField(TEXT("functionInputCount"), FunctionInputCount);
      Result->SetNumberField(TEXT("functionOutputCount"), FunctionOutputCount);

      // NEW6: functionInterface (sorted inputs/outputs with names + types)
      {
        TArray<UMaterialExpressionFunctionInput*>  InputExprs;
        TArray<UMaterialExpressionFunctionOutput*> OutputExprs;
        if (Expressions)
        {
          for (UMaterialExpression* Expr : *Expressions)
          {
            if (auto* In  = Cast<UMaterialExpressionFunctionInput>(Expr))  InputExprs.Add(In);
            if (auto* Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) OutputExprs.Add(Out);
          }
        }
        InputExprs.Sort([](const UMaterialExpressionFunctionInput& A, const UMaterialExpressionFunctionInput& B){
          if (A.SortPriority != B.SortPriority) return A.SortPriority < B.SortPriority;
          return A.InputName.LexicalLess(B.InputName);
        });
        OutputExprs.Sort([](const UMaterialExpressionFunctionOutput& A, const UMaterialExpressionFunctionOutput& B){
          if (A.SortPriority != B.SortPriority) return A.SortPriority < B.SortPriority;
          return A.OutputName.LexicalLess(B.OutputName);
        });

        TSharedRef<FJsonObject> Iface = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> InArr, OutArr;
        for (UMaterialExpressionFunctionInput* In : InputExprs)
        {
          if (!In) continue;
          TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
          O->SetStringField(TEXT("name"),                     In->InputName.ToString());
          O->SetStringField(TEXT("type"),                     ::McpFunctionInputTypeName(In->InputType));
          O->SetStringField(TEXT("description"),              In->Description);
          O->SetNumberField(TEXT("sortPriority"),             In->SortPriority);
          O->SetBoolField  (TEXT("usePreviewValueAsDefault"), In->bUsePreviewValueAsDefault);
          InArr.Add(MakeShared<FJsonValueObject>(O));
        }
        for (UMaterialExpressionFunctionOutput* Out : OutputExprs)
        {
          if (!Out) continue;
          TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
          O->SetStringField(TEXT("name"),         Out->OutputName.ToString());
          O->SetStringField(TEXT("description"),  Out->Description);
          O->SetNumberField(TEXT("sortPriority"), Out->SortPriority);
          OutArr.Add(MakeShared<FJsonValueObject>(O));
        }
        Iface->SetArrayField(TEXT("inputs"),  InArr);
        Iface->SetArrayField(TEXT("outputs"), OutArr);
        Result->SetObjectField(TEXT("functionInterface"), Iface);
      }
    }
    else // MaterialFunctionInstance
    {
      UMaterialFunctionInstance* Inst = CastChecked<UMaterialFunctionInstance>(GraphOwner.Asset);
      UMaterialFunction* Base = CastChecked<UMaterialFunction>(GraphOwner.GraphSource);
      Result->SetStringField(TEXT("graphType"), TEXT("MaterialFunctionInstance"));
      Result->SetStringField(TEXT("parentAsset"), Base->GetPathName());

      int32 FunctionInputCount = 0;
      int32 FunctionOutputCount = 0;
      if (Expressions)
      {
        for (UMaterialExpression* Expr : *Expressions)
        {
          FunctionInputCount += Cast<UMaterialExpressionFunctionInput>(Expr) ? 1 : 0;
          FunctionOutputCount += Cast<UMaterialExpressionFunctionOutput>(Expr) ? 1 : 0;
        }
      }
      Result->SetNumberField(TEXT("functionInputCount"), FunctionInputCount);
      Result->SetNumberField(TEXT("functionOutputCount"), FunctionOutputCount);

      McpCollectFunctionInstanceOverrides(Inst, Result.ToSharedRef());
    }

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material graph analyzed"), Result, FString());
    return true;
  }

  // Load the asset for other graph types (Blueprint, etc.)
  UObject *Asset = LoadObject<UObject>(nullptr, *AssetPath);
  if (!Asset) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Asset not found: %s"), *AssetPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Result, Asset);
  Result->SetStringField(TEXT("assetPath"), AssetPath);
  Result->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetName());

  // Check if it's a blueprint
  UBlueprint *Blueprint = Cast<UBlueprint>(Asset);
  if (Blueprint) {
    TArray<UEdGraph *> AllGraphs;
    Blueprint->GetAllGraphs(AllGraphs);

    int32 TotalNodes = 0;
    TArray<TSharedPtr<FJsonValue>> GraphInfoArray;

    for (UEdGraph *Graph : AllGraphs) {
      if (!Graph) continue;
      TSharedPtr<FJsonObject> GraphInfo = McpHandlerUtils::CreateResultObject();
      GraphInfo->SetStringField(TEXT("name"), Graph->GetName());
      GraphInfo->SetNumberField(TEXT("nodeCount"), Graph->Nodes.Num());
      TotalNodes += Graph->Nodes.Num();
      GraphInfoArray.Add(MakeShared<FJsonValueObject>(GraphInfo));
    }

    Result->SetStringField(TEXT("graphType"), TEXT("Blueprint"));
    Result->SetStringField(TEXT("blueprintType"), Blueprint->BlueprintType == BPTYPE_Interface ? TEXT("Interface") :
                           Blueprint->BlueprintType == BPTYPE_MacroLibrary ? TEXT("MacroLibrary") :
                           Blueprint->BlueprintType == BPTYPE_FunctionLibrary ? TEXT("FunctionLibrary") : TEXT("Class"));
    Result->SetNumberField(TEXT("totalNodes"), TotalNodes);
    Result->SetNumberField(TEXT("graphCount"), AllGraphs.Num());
    Result->SetArrayField(TEXT("graphs"), GraphInfoArray);

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Blueprint graph analyzed"), Result, FString());
    return true;
  }

  // Generic asset - no graph
  Result->SetStringField(TEXT("graphType"), TEXT("None"));
  Result->SetStringField(TEXT("message"), TEXT("Asset does not have a graph structure"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("No graph to analyze for this asset type"), Result, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("analyze_graph requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// GET ASSET GRAPH
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleGetAssetGraph(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("get_asset_graph"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("get_asset_graph payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), AssetPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (AssetPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Load the asset
  UObject *Asset = LoadObject<UObject>(nullptr, *AssetPath);
  if (!Asset) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Asset not found: %s"), *AssetPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  // try resolving as material-type asset first (UMaterial / UMaterialFunction / instance)
  FMcpMaterialGraphOwner GraphOwner;
  FString GraphOwnerError;
  if (McpResolveMaterialGraphOwner(AssetPath, GraphOwner, GraphOwnerError))
  {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, GraphOwner.Asset);
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetStringField(TEXT("assetClass"), GraphOwner.Asset->GetClass()->GetName());

    bool bIncludeOutputPins = false;
    Payload->TryGetBoolField(TEXT("includeOutputPins"), bIncludeOutputPins);

    const TArray<TObjectPtr<UMaterialExpression>>* ExpressionsPtr = McpGetGraphExpressions(GraphOwner);
    static const TArray<TObjectPtr<UMaterialExpression>> EmptyExprs;
    const TArray<TObjectPtr<UMaterialExpression>>& Expressions = ExpressionsPtr ? *ExpressionsPtr : EmptyExprs;

    if (GraphOwner.Kind == EMcpMaterialGraphOwnerKind::MaterialFunctionInstance)
    {
      UMaterialFunctionInstance* Inst = CastChecked<UMaterialFunctionInstance>(GraphOwner.Asset);
      UMaterialFunction* Base = CastChecked<UMaterialFunction>(GraphOwner.GraphSource);
      Result->SetStringField(TEXT("graphType"), TEXT("MaterialFunctionInstance"));
      Result->SetStringField(TEXT("parentAsset"), Base->GetPathName());
      McpCollectFunctionInstanceOverrides(Inst, Result.ToSharedRef());
    }
    else
    {
      FString GraphTypeName = (GraphOwner.Kind == EMcpMaterialGraphOwnerKind::Material)
          ? TEXT("Material") : TEXT("MaterialFunction");
      Result->SetStringField(TEXT("graphType"), GraphTypeName);
    }

    // build node list (shared for Material and MaterialFunction)
    TArray<TSharedPtr<FJsonValue>> NodeList;

    for (int32 i = 0; i < Expressions.Num(); ++i)
    {
      UMaterialExpression* Expr = Expressions[i];
      if (!Expr) continue;

      TSharedPtr<FJsonObject> NodeObj = McpHandlerUtils::CreateResultObject();
      McpAddExpressionIdentity(GraphOwner, Expr, i, NodeObj.ToSharedRef());

      TArray<TSharedPtr<FJsonValue>> InputsArray;
      TSet<FName> EmittedPinNames;
      for (FProperty* Property = Expr->GetClass()->PropertyLink; Property;
           Property = Property->PropertyLinkNext)
      {
        if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
        {
          if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput")))
          {
            FExpressionInput* Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(Expr);
            TSharedPtr<FJsonObject> InputObj = McpHandlerUtils::CreateResultObject();
            McpEmitInputPinJson(GraphOwner, Input, Property->GetName(), InputObj.ToSharedRef());
            InputsArray.Add(MakeShared<FJsonValueObject>(InputObj));
            EmittedPinNames.Add(FName(*Property->GetName()));
          }
        }
      }

      if (UMaterialExpressionMaterialFunctionCall* FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expr))
      {
        for (int32 InputIndex = 0; InputIndex < FuncCall->FunctionInputs.Num(); ++InputIndex)
        {
          const FFunctionExpressionInput& FEI = FuncCall->FunctionInputs[InputIndex];
          const FName InputName = FuncCall->GetInputName(InputIndex);
          if (EmittedPinNames.Contains(InputName))
          {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
                   TEXT("MFC %s: function input '%s' collides with property-walked pin; skipped"),
                   *Expr->GetName(), *InputName.ToString());
            continue;
          }

          TSharedPtr<FJsonObject> InputObj = McpHandlerUtils::CreateResultObject();
          if (FEI.ExpressionInput)
          {
            InputObj->SetStringField(TEXT("functionInputId"), FEI.ExpressionInput->Id.ToString());
          }
          InputObj->SetBoolField(TEXT("isFunctionInput"), true);
          McpEmitInputPinJson(GraphOwner, const_cast<FExpressionInput*>(&FEI.Input), InputName.ToString(), InputObj.ToSharedRef());
          InputsArray.Add(MakeShared<FJsonValueObject>(InputObj));
          EmittedPinNames.Add(InputName);
        }
      }

      NodeObj->SetArrayField(TEXT("inputs"), InputsArray);

      if (bIncludeOutputPins)
      {
          TArray<TSharedPtr<FJsonValue>> OutputsArray;
          if (UMaterialExpressionMaterialFunctionCall* FC = Cast<UMaterialExpressionMaterialFunctionCall>(Expr))
          {
              for (int32 OutputIndex = 0; OutputIndex < FC->FunctionOutputs.Num(); ++OutputIndex)
              {
                  const FFunctionExpressionOutput& Out = FC->FunctionOutputs[OutputIndex];
                  TSharedPtr<FJsonObject> OutObj = McpHandlerUtils::CreateResultObject();
                  OutObj->SetNumberField(TEXT("index"), OutputIndex);
                  bool bResolvedName = false;
                  FString Name = McpGetOutputName(Expr, OutputIndex, bResolvedName);
                  if (Name.IsEmpty() && Out.ExpressionOutput)
                  {
                      Name = Out.ExpressionOutput->OutputName.ToString();
                      bResolvedName = !Name.IsEmpty();
                  }
                  if (!Name.IsEmpty()) OutObj->SetStringField(TEXT("name"), Name);
                  OutObj->SetBoolField(TEXT("nameResolved"), bResolvedName);
                  OutputsArray.Add(MakeShared<FJsonValueObject>(OutObj));
              }
          }
          else
          {
              TArray<FExpressionOutput>& Outputs = Expr->GetOutputs();
              for (int32 OutputIndex = 0; OutputIndex < Outputs.Num(); ++OutputIndex)
              {
                  const FExpressionOutput& Out = Outputs[OutputIndex];
                  TSharedPtr<FJsonObject> OutObj = McpHandlerUtils::CreateResultObject();
                  OutObj->SetNumberField(TEXT("index"), OutputIndex);
                  bool bResolvedName = false;
                  const FString Name = McpGetOutputName(Expr, OutputIndex, bResolvedName);
                  if (!Name.IsEmpty()) OutObj->SetStringField(TEXT("name"), Name);
                  OutObj->SetBoolField(TEXT("nameResolved"), bResolvedName);

                  // Output mask emitted as bool (intentional asymmetry with input-mask which is int - see spec §3.3).
                  TSharedPtr<FJsonObject> Mask = McpHandlerUtils::CreateResultObject();
                  Mask->SetBoolField(TEXT("useMask"), Out.Mask != 0);
                  Mask->SetBoolField(TEXT("r"), Out.MaskR != 0);
                  Mask->SetBoolField(TEXT("g"), Out.MaskG != 0);
                  Mask->SetBoolField(TEXT("b"), Out.MaskB != 0);
                  Mask->SetBoolField(TEXT("a"), Out.MaskA != 0);
                  OutObj->SetObjectField(TEXT("mask"), Mask);

                  OutputsArray.Add(MakeShared<FJsonValueObject>(OutObj));
              }
          }
          NodeObj->SetArrayField(TEXT("outputs"), OutputsArray);
      }

      if (UMaterialExpressionParameter* Param = Cast<UMaterialExpressionParameter>(Expr))
        NodeObj->SetStringField(TEXT("parameterName"), Param->ParameterName.ToString());

      NodeList.Add(MakeShared<FJsonValueObject>(NodeObj));
    }

    Result->SetNumberField(TEXT("nodeCount"), Expressions.Num());
    Result->SetArrayField(TEXT("nodes"), NodeList);

    TArray<TSharedPtr<FJsonValue>> CommentList;
    for (UMaterialExpressionComment* Comment : McpGetGraphComments(GraphOwner))
    {
      if (!Comment)
      {
        continue;
      }
      CommentList.Add(MakeShared<FJsonValueObject>(McpBuildCommentJson(Comment, INDEX_NONE)));
    }
    Result->SetNumberField(TEXT("commentCount"), CommentList.Num());
    Result->SetArrayField(TEXT("comments"), CommentList);

    FString Msg = (GraphOwner.Kind == EMcpMaterialGraphOwnerKind::Material)
        ? TEXT("Material graph retrieved")
        : TEXT("Material function graph retrieved");
    SendAutomationResponse(Socket, RequestId, true, Msg, Result, FString());
    return true;
  }

  // fall through to non-material graph types (Blueprint, etc.) - Asset already loaded above

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Result, Asset);
  Result->SetStringField(TEXT("assetPath"), AssetPath);
  Result->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetName());

  // Check if it's a blueprint
  UBlueprint *Blueprint = Cast<UBlueprint>(Asset);
  if (Blueprint) {
    TArray<UEdGraph *> AllGraphs;
    Blueprint->GetAllGraphs(AllGraphs);

    TArray<TSharedPtr<FJsonValue>> GraphList;

    for (UEdGraph *Graph : AllGraphs) {
      if (!Graph) continue;

      TSharedPtr<FJsonObject> GraphObj = McpHandlerUtils::CreateResultObject();
      GraphObj->SetStringField(TEXT("name"), Graph->GetName());
      GraphObj->SetStringField(TEXT("graphType"), Graph->GetClass()->GetName());

      TArray<TSharedPtr<FJsonValue>> NodeArray;
      for (UEdGraphNode *Node : Graph->Nodes) {
        if (!Node) continue;

        TSharedPtr<FJsonObject> NodeObj = McpHandlerUtils::CreateResultObject();
        NodeObj->SetStringField(TEXT("nodeId"), Node->NodeGuid.ToString());
        NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
        NodeObj->SetNumberField(TEXT("x"), Node->NodePosX);
        NodeObj->SetNumberField(TEXT("y"), Node->NodePosY);
        NodeObj->SetBoolField(TEXT("isDeprecated"), Node->IsDeprecated());

        // Get pins
        TArray<TSharedPtr<FJsonValue>> PinArray;
        for (UEdGraphPin *Pin : Node->Pins) {
          if (!Pin) continue;
          TSharedPtr<FJsonObject> PinObj = McpHandlerUtils::CreateResultObject();
          PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
          PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
          PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
          PinObj->SetBoolField(TEXT("isConnected"), Pin->LinkedTo.Num() > 0);
          PinArray.Add(MakeShared<FJsonValueObject>(PinObj));
        }
        NodeObj->SetArrayField(TEXT("pins"), PinArray);

        NodeArray.Add(MakeShared<FJsonValueObject>(NodeObj));
      }
      GraphObj->SetArrayField(TEXT("nodes"), NodeArray);
      GraphObj->SetNumberField(TEXT("nodeCount"), Graph->Nodes.Num());

      GraphList.Add(MakeShared<FJsonValueObject>(GraphObj));
    }

    Result->SetStringField(TEXT("graphType"), TEXT("Blueprint"));
    Result->SetNumberField(TEXT("graphCount"), AllGraphs.Num());
    Result->SetArrayField(TEXT("graphs"), GraphList);

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Blueprint graph retrieved"), Result, FString());
    return true;
  }

  Result->SetStringField(TEXT("graphType"), TEXT("None"));
  Result->SetStringField(TEXT("message"), TEXT("Asset does not have a graph structure"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("No graph for this asset type"), Result, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("get_asset_graph requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
