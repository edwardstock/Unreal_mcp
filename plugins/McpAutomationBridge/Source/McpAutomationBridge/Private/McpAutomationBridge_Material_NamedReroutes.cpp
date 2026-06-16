// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_Material_NamedReroutes.cpp
//
// Task D.2 - named reroutes migration.
// Plan: docs/superpowers/plans/2026-05-07-mcp-material-tools-redesign.md
//
// Houses the named-reroute handlers moved out of
// McpAutomationBridge_AssetWorkflowHandlers.cpp:
//   - HandleCreateNamedReroute                    (answers create_named_reroutes)
//   - HandleUseNamedReroute                       (answers use_named_reroutes)
//   - HandleReplaceLongConnectionWithNamedReroute (answers replace_long_connections_with_named_reroutes)
//
// D.2 array support: each handler accepts EITHER the legacy single-item shape
// (top-level assetPath + the singular fields) OR a plural shape with an array
// (`reroutes[]` / `usages[]` / `replacements[]`). When the array is present,
// each item is processed independently; per-item errors do not abort the batch.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR

#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionNamedReroute.h"

extern UMaterialExpression* McpFindGraphExpressionFromPayload(
    const FMcpMaterialGraphOwner& Owner,
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* IndexField,
    const TCHAR* IdField,
    const TCHAR* PathField);
extern int32 McpExpressionIndex(const FMcpMaterialGraphOwner& Owner, const UMaterialExpression* Expression);
extern void McpAddExpressionIdentity(
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpression* Expression,
    int32 Index,
    const TSharedRef<FJsonObject>& Out);
extern FIntPoint McpEstimateExpressionSize(UMaterialExpression* Expr);
extern FIntPoint McpFindFreePosition(
    const FMcpMaterialGraphOwner& Owner,
    const FIntPoint& Start,
    const FIntPoint& Size,
    int32 StepY);
extern FIntPoint McpResolvePlacement(
    const FMcpMaterialGraphOwner& Owner,
    const TSharedPtr<FJsonObject>& Payload,
    UMaterialExpression* NewExpression);
extern bool McpAddExpressionToGraph(const FMcpMaterialGraphOwner& Owner, UMaterialExpression* Expression);
extern UMaterialExpressionNamedRerouteDeclaration* McpFindNamedRerouteDeclaration(
    const FMcpMaterialGraphOwner& Owner,
    const FString& NameOrGuid);
extern FExpressionInput* McpFindExpressionInputByName(UMaterialExpression* Expression, FString& InOutInputName);

// Per-item: create a named reroute declaration sourced from a node.
static TSharedPtr<FJsonObject> McpCreateNamedRerouteItem(
    const FMcpMaterialGraphOwner& GraphOwner,
    const TSharedPtr<FJsonObject>& Item,
    FString& OutError,
    FString& OutErrorCode)
{
  if (!Item.IsValid()) {
    OutError = TEXT("reroute item missing");
    OutErrorCode = TEXT("INVALID_PAYLOAD");
    return nullptr;
  }

  FString RerouteName;
  if (!Item->TryGetStringField(TEXT("name"), RerouteName) || RerouteName.IsEmpty()) {
    OutError = TEXT("name is required");
    OutErrorCode = TEXT("INVALID_ARGUMENT");
    return nullptr;
  }

  UMaterialExpression* Source = McpFindGraphExpressionFromPayload(GraphOwner, Item, TEXT("sourceExpressionIndex"), TEXT("sourceNodeId"), TEXT("sourceExpressionPath"));
  if (!Source) {
    Source = McpFindGraphExpressionFromPayload(GraphOwner, Item, TEXT("expressionIndex"), TEXT("nodeId"), TEXT("expressionPath"));
  }
  if (!Source) {
    OutError = TEXT("Source node not found");
    OutErrorCode = TEXT("SOURCE_NODE_NOT_FOUND");
    return nullptr;
  }

  int32 SourceOutputIndex = 0;
  Item->TryGetNumberField(TEXT("sourceOutputIndex"), SourceOutputIndex);

  UMaterialExpressionNamedRerouteDeclaration* Declaration =
      NewObject<UMaterialExpressionNamedRerouteDeclaration>(GraphOwner.GraphSource, UMaterialExpressionNamedRerouteDeclaration::StaticClass(), NAME_None, RF_Transactional);
  Declaration->Name = FName(*RerouteName);
  Declaration->Input.Expression = Source;
  Declaration->Input.OutputIndex = SourceOutputIndex;
  const FIntPoint Position = McpResolvePlacement(GraphOwner, Item, Declaration);
  Declaration->MaterialExpressionEditorX = Position.X;
  Declaration->MaterialExpressionEditorY = Position.Y;
  McpAddExpressionToGraph(GraphOwner, Declaration);

  TSharedPtr<FJsonObject> ItemResult = McpHandlerUtils::CreateResultObject();
  McpAddExpressionIdentity(GraphOwner, Declaration, McpExpressionIndex(GraphOwner, Declaration), ItemResult.ToSharedRef());
  ItemResult->SetStringField(TEXT("rerouteName"), Declaration->Name.ToString());
  ItemResult->SetStringField(TEXT("rerouteGuid"), Declaration->VariableGuid.ToString());
  ItemResult->SetNumberField(TEXT("sourceExpressionIndex"), McpExpressionIndex(GraphOwner, Source));
  return ItemResult;
}

// Per-item: insert a named reroute usage that feeds a target input pin.
static TSharedPtr<FJsonObject> McpUseNamedRerouteItem(
    const FMcpMaterialGraphOwner& GraphOwner,
    const TSharedPtr<FJsonObject>& Item,
    FString& OutError,
    FString& OutErrorCode)
{
  if (!Item.IsValid()) {
    OutError = TEXT("usage item missing");
    OutErrorCode = TEXT("INVALID_PAYLOAD");
    return nullptr;
  }

  FString DeclarationRef;
  Item->TryGetStringField(TEXT("declarationId"), DeclarationRef);
  if (DeclarationRef.IsEmpty()) Item->TryGetStringField(TEXT("declarationGuid"), DeclarationRef);
  if (DeclarationRef.IsEmpty()) Item->TryGetStringField(TEXT("declarationName"), DeclarationRef);
  if (DeclarationRef.IsEmpty()) Item->TryGetStringField(TEXT("name"), DeclarationRef);
  UMaterialExpressionNamedRerouteDeclaration* Declaration = McpFindNamedRerouteDeclaration(GraphOwner, DeclarationRef);
  if (!Declaration) {
    OutError = TEXT("Named reroute declaration not found");
    OutErrorCode = TEXT("DECLARATION_NOT_FOUND");
    return nullptr;
  }

  UMaterialExpression* Target = McpFindGraphExpressionFromPayload(GraphOwner, Item, TEXT("targetExpressionIndex"), TEXT("targetNodeId"), TEXT("targetExpressionPath"));
  if (!Target) {
    OutError = TEXT("Target node not found");
    OutErrorCode = TEXT("TARGET_NODE_NOT_FOUND");
    return nullptr;
  }

  FString TargetInputName;
  Item->TryGetStringField(TEXT("targetInputPin"), TargetInputName);
  if (TargetInputName.IsEmpty()) Item->TryGetStringField(TEXT("targetPin"), TargetInputName);
  if (TargetInputName.IsEmpty()) Item->TryGetStringField(TEXT("inputName"), TargetInputName);
  FExpressionInput* TargetInput = McpFindExpressionInputByName(Target, TargetInputName);
  if (!TargetInput) {
    OutError = TEXT("Target input pin not found");
    OutErrorCode = TEXT("INPUT_NOT_FOUND");
    return nullptr;
  }

  UMaterialExpressionNamedRerouteUsage* Usage =
      NewObject<UMaterialExpressionNamedRerouteUsage>(GraphOwner.GraphSource, UMaterialExpressionNamedRerouteUsage::StaticClass(), NAME_None, RF_Transactional);
  Usage->Declaration = Declaration;
  Usage->DeclarationGuid = Declaration->VariableGuid;
  const FIntPoint Position = McpResolvePlacement(GraphOwner, Item, Usage);
  Usage->MaterialExpressionEditorX = Position.X;
  Usage->MaterialExpressionEditorY = Position.Y;
  McpAddExpressionToGraph(GraphOwner, Usage);
  TargetInput->Expression = Usage;
  TargetInput->OutputIndex = 0;

  TSharedPtr<FJsonObject> ItemResult = McpHandlerUtils::CreateResultObject();
  McpAddExpressionIdentity(GraphOwner, Usage, McpExpressionIndex(GraphOwner, Usage), ItemResult.ToSharedRef());
  ItemResult->SetStringField(TEXT("targetInputPin"), TargetInputName);
  ItemResult->SetNumberField(TEXT("targetExpressionIndex"), McpExpressionIndex(GraphOwner, Target));
  return ItemResult;
}

// Per-item: replace a long source->target connection with a declaration/usage pair.
static TSharedPtr<FJsonObject> McpReplaceLongConnectionItem(
    const FMcpMaterialGraphOwner& GraphOwner,
    const TSharedPtr<FJsonObject>& Item,
    FString& OutError,
    FString& OutErrorCode)
{
  if (!Item.IsValid()) {
    OutError = TEXT("replacement item missing");
    OutErrorCode = TEXT("INVALID_PAYLOAD");
    return nullptr;
  }

  FString RerouteName;
  if (!Item->TryGetStringField(TEXT("name"), RerouteName) || RerouteName.IsEmpty()) {
    OutError = TEXT("name is required");
    OutErrorCode = TEXT("INVALID_ARGUMENT");
    return nullptr;
  }

  UMaterialExpression* Source = McpFindGraphExpressionFromPayload(GraphOwner, Item, TEXT("sourceExpressionIndex"), TEXT("sourceNodeId"), TEXT("sourceExpressionPath"));
  UMaterialExpression* Target = McpFindGraphExpressionFromPayload(GraphOwner, Item, TEXT("targetExpressionIndex"), TEXT("targetNodeId"), TEXT("targetExpressionPath"));
  if (!Source || !Target) {
    OutError = TEXT("Source or target node not found");
    OutErrorCode = TEXT("NODE_NOT_FOUND");
    return nullptr;
  }

  FString TargetInputName;
  Item->TryGetStringField(TEXT("targetInputPin"), TargetInputName);
  if (TargetInputName.IsEmpty()) Item->TryGetStringField(TEXT("targetPin"), TargetInputName);
  if (TargetInputName.IsEmpty()) Item->TryGetStringField(TEXT("inputName"), TargetInputName);
  FExpressionInput* TargetInput = McpFindExpressionInputByName(Target, TargetInputName);
  if (!TargetInput) {
    OutError = TEXT("Target input pin not found");
    OutErrorCode = TEXT("INPUT_NOT_FOUND");
    return nullptr;
  }

  double MinDistance = 0.0;
  Item->TryGetNumberField(TEXT("minDistance"), MinDistance);
  const int32 Distance = FMath::Abs(Source->MaterialExpressionEditorX - Target->MaterialExpressionEditorX);
  if (MinDistance > 0.0 && Distance < MinDistance) {
    OutError = TEXT("Connection is shorter than minDistance");
    OutErrorCode = TEXT("DISTANCE_BELOW_THRESHOLD");
    return nullptr;
  }

  int32 SourceOutputIndex = TargetInput->OutputIndex;
  Item->TryGetNumberField(TEXT("sourceOutputIndex"), SourceOutputIndex);

  UMaterialExpressionNamedRerouteDeclaration* Declaration =
      NewObject<UMaterialExpressionNamedRerouteDeclaration>(GraphOwner.GraphSource, UMaterialExpressionNamedRerouteDeclaration::StaticClass(), NAME_None, RF_Transactional);
  Declaration->Name = FName(*RerouteName);
  Declaration->Input.Expression = Source;
  Declaration->Input.OutputIndex = SourceOutputIndex;
  Declaration->MaterialExpressionEditorX = Source->MaterialExpressionEditorX + McpEstimateExpressionSize(Source).X + 220;
  Declaration->MaterialExpressionEditorY = Source->MaterialExpressionEditorY;
  const FIntPoint DeclarationPos = McpFindFreePosition(GraphOwner, FIntPoint(Declaration->MaterialExpressionEditorX, Declaration->MaterialExpressionEditorY), McpEstimateExpressionSize(Declaration), 180);
  Declaration->MaterialExpressionEditorX = DeclarationPos.X;
  Declaration->MaterialExpressionEditorY = DeclarationPos.Y;
  McpAddExpressionToGraph(GraphOwner, Declaration);

  UMaterialExpressionNamedRerouteUsage* Usage =
      NewObject<UMaterialExpressionNamedRerouteUsage>(GraphOwner.GraphSource, UMaterialExpressionNamedRerouteUsage::StaticClass(), NAME_None, RF_Transactional);
  Usage->Declaration = Declaration;
  Usage->DeclarationGuid = Declaration->VariableGuid;
  Usage->MaterialExpressionEditorX = Target->MaterialExpressionEditorX - 260;
  Usage->MaterialExpressionEditorY = Target->MaterialExpressionEditorY;
  const FIntPoint UsagePos = McpFindFreePosition(GraphOwner, FIntPoint(Usage->MaterialExpressionEditorX, Usage->MaterialExpressionEditorY), McpEstimateExpressionSize(Usage), 180);
  Usage->MaterialExpressionEditorX = UsagePos.X;
  Usage->MaterialExpressionEditorY = UsagePos.Y;
  McpAddExpressionToGraph(GraphOwner, Usage);
  TargetInput->Expression = Usage;
  TargetInput->OutputIndex = 0;

  TSharedPtr<FJsonObject> ItemResult = McpHandlerUtils::CreateResultObject();
  ItemResult->SetStringField(TEXT("rerouteName"), Declaration->Name.ToString());
  ItemResult->SetStringField(TEXT("rerouteGuid"), Declaration->VariableGuid.ToString());
  ItemResult->SetNumberField(TEXT("declarationExpressionIndex"), McpExpressionIndex(GraphOwner, Declaration));
  ItemResult->SetNumberField(TEXT("usageExpressionIndex"), McpExpressionIndex(GraphOwner, Usage));
  ItemResult->SetNumberField(TEXT("sourceExpressionIndex"), McpExpressionIndex(GraphOwner, Source));
  ItemResult->SetNumberField(TEXT("targetExpressionIndex"), McpExpressionIndex(GraphOwner, Target));
  ItemResult->SetStringField(TEXT("targetInputPin"), TargetInputName);
  return ItemResult;
}

// Generic dispatch over either legacy single-item payloads or the plural array
// shape. PerItemFn returns nullptr on error and fills OutError/OutErrorCode;
// returns a result JsonObject on success. ArrayFieldName is the plural array
// field (e.g. "reroutes"). When the array is absent the entire payload is
// processed once as a single item.
template <typename TPerItemFn>
static void McpDispatchPlural(
    const FMcpMaterialGraphOwner& GraphOwner,
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* ArrayFieldName,
    TArray<TSharedPtr<FJsonValue>>& OutResults,
    TArray<TSharedPtr<FJsonValue>>& OutErrors,
    int32& OutSuccessCount,
    TPerItemFn PerItemFn)
{
  const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
  const bool bHasArray = Payload->TryGetArrayField(ArrayFieldName, Items) && Items;

  auto Run = [&](const TSharedPtr<FJsonObject>& Item, int32 Index)
  {
    FString Err, ErrCode;
    TSharedPtr<FJsonObject> ItemResult = PerItemFn(GraphOwner, Item, Err, ErrCode);
    if (ItemResult.IsValid())
    {
      ItemResult->SetNumberField(TEXT("index"), Index);
      OutResults.Add(MakeShared<FJsonValueObject>(ItemResult));
      ++OutSuccessCount;
    }
    else
    {
      TSharedPtr<FJsonObject> ErrItem = MakeShared<FJsonObject>();
      ErrItem->SetNumberField(TEXT("index"), Index);
      ErrItem->SetStringField(TEXT("code"), ErrCode);
      ErrItem->SetStringField(TEXT("message"), Err);
      OutErrors.Add(MakeShared<FJsonValueObject>(ErrItem));
    }
  };

  if (!bHasArray)
  {
    Run(Payload, 0);
    return;
  }

  for (int32 i = 0; i < Items->Num(); ++i)
  {
    const TSharedPtr<FJsonObject>* ItemObj = nullptr;
    if (!(*Items)[i].IsValid() || !(*Items)[i]->TryGetObject(ItemObj) || !ItemObj)
    {
      TSharedPtr<FJsonObject> ErrItem = MakeShared<FJsonObject>();
      ErrItem->SetNumberField(TEXT("index"), i);
      ErrItem->SetStringField(TEXT("code"), TEXT("INVALID_PAYLOAD"));
      ErrItem->SetStringField(TEXT("message"), TEXT("item is not an object"));
      OutErrors.Add(MakeShared<FJsonValueObject>(ErrItem));
      continue;
    }
    Run(*ItemObj, i);
  }
}

#endif // WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleCreateNamedReroute(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("create_named_reroutes"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("create_named_reroutes payload missing"), TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    SendAutomationError(Socket, RequestId, TEXT("assetPath or materialPath is required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FMcpMaterialGraphOwner GraphOwner;
  FString GraphOwnerError;
  if (!McpResolveMaterialGraphOwner(MaterialPath, GraphOwner, GraphOwnerError)) {
    SendAutomationError(Socket, RequestId, GraphOwnerError,
                        GraphOwnerError.Contains(TEXT("not found")) ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE"));
    return true;
  }
  if (GraphOwner.bReadOnly) {
    SendAutomationError(Socket, RequestId, TEXT("Cannot create named reroutes on a MaterialFunctionInstance - edit the parent function instead"), TEXT("UNSUPPORTED_OPERATION"));
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> Results;
  TArray<TSharedPtr<FJsonValue>> Errors;
  int32 SuccessCount = 0;
  McpDispatchPlural(GraphOwner, Payload, TEXT("reroutes"), Results, Errors, SuccessCount, &McpCreateNamedRerouteItem);

  FString RebuildErr;
  McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Resp, GraphOwner.Asset);
  Resp->SetArrayField(TEXT("results"), Results);
  if (Errors.Num() > 0) {
    Resp->SetArrayField(TEXT("errors"), Errors);
  }
  Resp->SetNumberField(TEXT("affectedCount"), SuccessCount);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Named reroute declarations created"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false, TEXT("create_named_reroutes requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleUseNamedReroute(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("use_named_reroutes"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("use_named_reroutes payload missing"), TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    SendAutomationError(Socket, RequestId, TEXT("assetPath or materialPath is required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FMcpMaterialGraphOwner GraphOwner;
  FString GraphOwnerError;
  if (!McpResolveMaterialGraphOwner(MaterialPath, GraphOwner, GraphOwnerError)) {
    SendAutomationError(Socket, RequestId, GraphOwnerError,
                        GraphOwnerError.Contains(TEXT("not found")) ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE"));
    return true;
  }
  if (GraphOwner.bReadOnly) {
    SendAutomationError(Socket, RequestId, TEXT("Cannot create named reroute usages on a MaterialFunctionInstance - edit the parent function instead"), TEXT("UNSUPPORTED_OPERATION"));
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> Results;
  TArray<TSharedPtr<FJsonValue>> Errors;
  int32 SuccessCount = 0;
  McpDispatchPlural(GraphOwner, Payload, TEXT("usages"), Results, Errors, SuccessCount, &McpUseNamedRerouteItem);

  FString RebuildErr;
  McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Resp, GraphOwner.Asset);
  Resp->SetArrayField(TEXT("results"), Results);
  if (Errors.Num() > 0) {
    Resp->SetArrayField(TEXT("errors"), Errors);
  }
  Resp->SetNumberField(TEXT("affectedCount"), SuccessCount);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Named reroute usages created"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false, TEXT("use_named_reroutes requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleReplaceLongConnectionWithNamedReroute(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("replace_long_connections_with_named_reroutes"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("replace_long_connections_with_named_reroutes payload missing"), TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    SendAutomationError(Socket, RequestId, TEXT("assetPath or materialPath is required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FMcpMaterialGraphOwner GraphOwner;
  FString GraphOwnerError;
  if (!McpResolveMaterialGraphOwner(MaterialPath, GraphOwner, GraphOwnerError)) {
    SendAutomationError(Socket, RequestId, GraphOwnerError,
                        GraphOwnerError.Contains(TEXT("not found")) ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE"));
    return true;
  }
  if (GraphOwner.bReadOnly) {
    SendAutomationError(Socket, RequestId, TEXT("Cannot edit named reroutes on a MaterialFunctionInstance - edit the parent function instead"), TEXT("UNSUPPORTED_OPERATION"));
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> Results;
  TArray<TSharedPtr<FJsonValue>> Errors;
  int32 SuccessCount = 0;
  McpDispatchPlural(GraphOwner, Payload, TEXT("replacements"), Results, Errors, SuccessCount, &McpReplaceLongConnectionItem);

  FString RebuildErr;
  McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Resp, GraphOwner.Asset);
  Resp->SetArrayField(TEXT("results"), Results);
  if (Errors.Num() > 0) {
    Resp->SetArrayField(TEXT("errors"), Errors);
  }
  Resp->SetNumberField(TEXT("affectedCount"), SuccessCount);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Long material connections replaced with named reroutes"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false, TEXT("replace_long_connections_with_named_reroutes requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
