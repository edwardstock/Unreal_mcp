// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_Material_Comments.cpp
//
// Task D.2 - comments migration.
// Plan: docs/superpowers/plans/2026-05-07-mcp-material-tools-redesign.md
//
// Houses the material-comment handlers moved out of
// McpAutomationBridge_AssetWorkflowHandlers.cpp:
//   - HandleCreateMaterialComment      (answers create_material_comments)
//   - HandleWrapMaterialNodesInComment (answers wrap_material_nodes_in_comments)
//
// D.2 array support: each handler accepts EITHER the legacy single-item shape
// (top-level assetPath + the singular fields) OR a plural shape with an array
// (`comments[]` / `wraps[]`). When the array is present, each item is processed
// independently; per-item errors do not abort the batch.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR

#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionComment.h"

extern UMaterialExpression* McpFindGraphExpressionFromPayload(
    const FMcpMaterialGraphOwner& Owner,
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* IndexField,
    const TCHAR* IdField,
    const TCHAR* PathField);
extern FIntPoint McpEstimateExpressionSize(UMaterialExpression* Expr);
extern bool McpAddCommentToGraph(const FMcpMaterialGraphOwner& Owner, UMaterialExpressionComment* Comment);

// Per-item create logic; runs on a single payload object that contains the
// per-item fields. Returns the per-item result on success, or fills OutError.
// Returns nullptr on error.
static TSharedPtr<FJsonObject> McpCreateMaterialCommentItem(
    const FMcpMaterialGraphOwner& GraphOwner,
    const TSharedPtr<FJsonObject>& Item,
    FString& OutError,
    FString& OutErrorCode)
{
  if (!Item.IsValid())
  {
    OutError = TEXT("comment item missing");
    OutErrorCode = TEXT("INVALID_PAYLOAD");
    return nullptr;
  }

  FString Text;
  Item->TryGetStringField(TEXT("text"), Text);
  if (Text.IsEmpty()) {
    Item->TryGetStringField(TEXT("comment"), Text);
  }

  double X = 0.0, Y = 0.0, Width = 800.0, Height = 400.0;
  Item->TryGetNumberField(TEXT("x"), X);
  Item->TryGetNumberField(TEXT("y"), Y);
  Item->TryGetNumberField(TEXT("width"), Width);
  Item->TryGetNumberField(TEXT("height"), Height);

  UMaterialExpressionComment* Comment = NewObject<UMaterialExpressionComment>(GraphOwner.GraphSource, UMaterialExpressionComment::StaticClass(), NAME_None, RF_Transactional);
  Comment->Text = Text;
  Comment->MaterialExpressionEditorX = static_cast<int32>(X);
  Comment->MaterialExpressionEditorY = static_cast<int32>(Y);
  Comment->SizeX = static_cast<int32>(Width);
  Comment->SizeY = static_cast<int32>(Height);

  bool bGroupMode = true;
  Item->TryGetBoolField(TEXT("groupMode"), bGroupMode);
  Comment->bGroupMode = bGroupMode;

  const TSharedPtr<FJsonObject>* ColorObj = nullptr;
  if (Item->TryGetObjectField(TEXT("color"), ColorObj) && ColorObj) {
    double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
    (*ColorObj)->TryGetNumberField(TEXT("r"), R);
    (*ColorObj)->TryGetNumberField(TEXT("g"), G);
    (*ColorObj)->TryGetNumberField(TEXT("b"), B);
    (*ColorObj)->TryGetNumberField(TEXT("a"), A);
    Comment->CommentColor = FLinearColor(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B), static_cast<float>(A));
  }

  if (!McpAddCommentToGraph(GraphOwner, Comment)) {
    OutError = TEXT("Material comments require UE 5.1+ expression collections");
    OutErrorCode = TEXT("UNSUPPORTED_OPERATION");
    return nullptr;
  }

  TSharedPtr<FJsonObject> ItemResult = McpHandlerUtils::CreateResultObject();
  ItemResult->SetStringField(TEXT("commentId"), Comment->GetPathName());
  ItemResult->SetStringField(TEXT("text"), Comment->Text);
  ItemResult->SetNumberField(TEXT("x"), Comment->MaterialExpressionEditorX);
  ItemResult->SetNumberField(TEXT("y"), Comment->MaterialExpressionEditorY);
  ItemResult->SetNumberField(TEXT("width"), Comment->SizeX);
  ItemResult->SetNumberField(TEXT("height"), Comment->SizeY);
  ItemResult->SetBoolField(TEXT("groupMode"), Comment->bGroupMode);
  return ItemResult;
}

// Wrap a set of nodes in a comment; returns the synthesized per-item comment
// payload to feed into McpCreateMaterialCommentItem. Fills OutError on resolve
// failure.
static TSharedPtr<FJsonObject> McpBuildWrapCommentPayload(
    const FMcpMaterialGraphOwner& GraphOwner,
    const TSharedPtr<FJsonObject>& Item,
    const FString& MaterialPath,
    FString& OutError,
    FString& OutErrorCode)
{
  const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
  if (!Item.IsValid() || !Item->TryGetArrayField(TEXT("nodes"), Nodes) || !Nodes || Nodes->Num() == 0) {
    OutError = TEXT("nodes array is required");
    OutErrorCode = TEXT("INVALID_ARGUMENT");
    return nullptr;
  }

  int32 MinX = TNumericLimits<int32>::Max();
  int32 MinY = TNumericLimits<int32>::Max();
  int32 MaxX = TNumericLimits<int32>::Min();
  int32 MaxY = TNumericLimits<int32>::Min();
  int32 ResolvedCount = 0;

  for (const TSharedPtr<FJsonValue>& NodeValue : *Nodes) {
    const TSharedPtr<FJsonObject>* NodeObj = nullptr;
    if (!NodeValue.IsValid() || !NodeValue->TryGetObject(NodeObj) || !NodeObj) {
      continue;
    }
    UMaterialExpression* Expr = McpFindGraphExpressionFromPayload(GraphOwner, *NodeObj, TEXT("expressionIndex"), TEXT("nodeId"), TEXT("expressionPath"));
    if (!Expr) {
      continue;
    }
    const FIntPoint Size = McpEstimateExpressionSize(Expr);
    MinX = FMath::Min(MinX, Expr->MaterialExpressionEditorX);
    MinY = FMath::Min(MinY, Expr->MaterialExpressionEditorY);
    MaxX = FMath::Max(MaxX, Expr->MaterialExpressionEditorX + Size.X);
    MaxY = FMath::Max(MaxY, Expr->MaterialExpressionEditorY + Size.Y);
    ResolvedCount++;
  }

  if (ResolvedCount == 0) {
    OutError = TEXT("No nodes could be resolved");
    OutErrorCode = TEXT("NODE_NOT_FOUND");
    return nullptr;
  }

  double Padding = 80.0;
  Item->TryGetNumberField(TEXT("padding"), Padding);

  TSharedPtr<FJsonObject> LocalPayload = McpHandlerUtils::CreateResultObject();
  LocalPayload->SetStringField(TEXT("assetPath"), MaterialPath);
  FString Text;
  Item->TryGetStringField(TEXT("text"), Text);
  if (Text.IsEmpty()) {
    Item->TryGetStringField(TEXT("comment"), Text);
  }
  LocalPayload->SetStringField(TEXT("text"), Text);
  LocalPayload->SetNumberField(TEXT("x"), MinX - Padding);
  LocalPayload->SetNumberField(TEXT("y"), MinY - Padding);
  LocalPayload->SetNumberField(TEXT("width"), (MaxX - MinX) + Padding * 2.0);
  LocalPayload->SetNumberField(TEXT("height"), (MaxY - MinY) + Padding * 2.0);
  bool bGroupMode = true;
  Item->TryGetBoolField(TEXT("groupMode"), bGroupMode);
  LocalPayload->SetBoolField(TEXT("groupMode"), bGroupMode);
  return LocalPayload;
}

#endif // WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleCreateMaterialComment(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("create_material_comments"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("create_material_comments payload missing"), TEXT("INVALID_PAYLOAD"));
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
    SendAutomationError(Socket, RequestId, TEXT("Cannot create comments on a MaterialFunctionInstance - edit the parent function instead"), TEXT("UNSUPPORTED_OPERATION"));
    return true;
  }

  // Detect plural shape (`comments[]`); if absent, treat the whole payload as a
  // single-item legacy request.
  const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
  const bool bHasArray = Payload->TryGetArrayField(TEXT("comments"), Items) && Items;

  TArray<TSharedPtr<FJsonValue>> Results;
  TArray<TSharedPtr<FJsonValue>> Errors;
  int32 SuccessCount = 0;

  if (!bHasArray)
  {
    FString Err, ErrCode;
    TSharedPtr<FJsonObject> ItemResult = McpCreateMaterialCommentItem(GraphOwner, Payload, Err, ErrCode);
    if (ItemResult.IsValid())
    {
      Results.Add(MakeShared<FJsonValueObject>(ItemResult));
      ++SuccessCount;
    }
    else
    {
      TSharedPtr<FJsonObject> ErrItem = MakeShared<FJsonObject>();
      ErrItem->SetNumberField(TEXT("index"), 0);
      ErrItem->SetStringField(TEXT("code"), ErrCode);
      ErrItem->SetStringField(TEXT("message"), Err);
      Errors.Add(MakeShared<FJsonValueObject>(ErrItem));
    }
  }
  else
  {
    for (int32 i = 0; i < Items->Num(); ++i)
    {
      const TSharedPtr<FJsonObject>* ItemObj = nullptr;
      if (!(*Items)[i].IsValid() || !(*Items)[i]->TryGetObject(ItemObj) || !ItemObj)
      {
        TSharedPtr<FJsonObject> ErrItem = MakeShared<FJsonObject>();
        ErrItem->SetNumberField(TEXT("index"), i);
        ErrItem->SetStringField(TEXT("code"), TEXT("INVALID_PAYLOAD"));
        ErrItem->SetStringField(TEXT("message"), TEXT("comment item is not an object"));
        Errors.Add(MakeShared<FJsonValueObject>(ErrItem));
        continue;
      }
      FString Err, ErrCode;
      TSharedPtr<FJsonObject> ItemResult = McpCreateMaterialCommentItem(GraphOwner, *ItemObj, Err, ErrCode);
      if (ItemResult.IsValid())
      {
        ItemResult->SetNumberField(TEXT("index"), i);
        Results.Add(MakeShared<FJsonValueObject>(ItemResult));
        ++SuccessCount;
      }
      else
      {
        TSharedPtr<FJsonObject> ErrItem = MakeShared<FJsonObject>();
        ErrItem->SetNumberField(TEXT("index"), i);
        ErrItem->SetStringField(TEXT("code"), ErrCode);
        ErrItem->SetStringField(TEXT("message"), Err);
        Errors.Add(MakeShared<FJsonValueObject>(ErrItem));
      }
    }
  }

  FString RebuildErr;
  McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Resp, GraphOwner.Asset);
  Resp->SetArrayField(TEXT("results"), Results);
  if (Errors.Num() > 0)
  {
    Resp->SetArrayField(TEXT("errors"), Errors);
  }
  Resp->SetNumberField(TEXT("affectedCount"), SuccessCount);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Material comments created"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false, TEXT("create_material_comments requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleWrapMaterialNodesInComment(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("wrap_material_nodes_in_comments"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("wrap_material_nodes_in_comments payload missing"), TEXT("INVALID_PAYLOAD"));
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
    SendAutomationError(Socket, RequestId, TEXT("Cannot create comments on a MaterialFunctionInstance - edit the parent function instead"), TEXT("UNSUPPORTED_OPERATION"));
    return true;
  }

  // Detect plural shape (`wraps[]`); each entry is a wrap descriptor that
  // owns its own `nodes[]` array. If absent, treat the whole payload as a
  // single legacy wrap request (its `nodes[]` is at the top level).
  const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
  const bool bHasArray = Payload->TryGetArrayField(TEXT("wraps"), Items) && Items;

  TArray<TSharedPtr<FJsonValue>> Results;
  TArray<TSharedPtr<FJsonValue>> Errors;
  int32 SuccessCount = 0;

  auto ProcessWrapItem = [&](const TSharedPtr<FJsonObject>& Item, int32 Index)
  {
    FString Err, ErrCode;
    TSharedPtr<FJsonObject> CommentPayload = McpBuildWrapCommentPayload(GraphOwner, Item, MaterialPath, Err, ErrCode);
    if (!CommentPayload.IsValid())
    {
      TSharedPtr<FJsonObject> ErrItem = MakeShared<FJsonObject>();
      ErrItem->SetNumberField(TEXT("index"), Index);
      ErrItem->SetStringField(TEXT("code"), ErrCode);
      ErrItem->SetStringField(TEXT("message"), Err);
      Errors.Add(MakeShared<FJsonValueObject>(ErrItem));
      return;
    }
    TSharedPtr<FJsonObject> ItemResult = McpCreateMaterialCommentItem(GraphOwner, CommentPayload, Err, ErrCode);
    if (ItemResult.IsValid())
    {
      ItemResult->SetNumberField(TEXT("index"), Index);
      Results.Add(MakeShared<FJsonValueObject>(ItemResult));
      ++SuccessCount;
    }
    else
    {
      TSharedPtr<FJsonObject> ErrItem = MakeShared<FJsonObject>();
      ErrItem->SetNumberField(TEXT("index"), Index);
      ErrItem->SetStringField(TEXT("code"), ErrCode);
      ErrItem->SetStringField(TEXT("message"), Err);
      Errors.Add(MakeShared<FJsonValueObject>(ErrItem));
    }
  };

  if (!bHasArray)
  {
    ProcessWrapItem(Payload, 0);
  }
  else
  {
    for (int32 i = 0; i < Items->Num(); ++i)
    {
      const TSharedPtr<FJsonObject>* ItemObj = nullptr;
      if (!(*Items)[i].IsValid() || !(*Items)[i]->TryGetObject(ItemObj) || !ItemObj)
      {
        TSharedPtr<FJsonObject> ErrItem = MakeShared<FJsonObject>();
        ErrItem->SetNumberField(TEXT("index"), i);
        ErrItem->SetStringField(TEXT("code"), TEXT("INVALID_PAYLOAD"));
        ErrItem->SetStringField(TEXT("message"), TEXT("wrap item is not an object"));
        Errors.Add(MakeShared<FJsonValueObject>(ErrItem));
        continue;
      }
      ProcessWrapItem(*ItemObj, i);
    }
  }

  FString RebuildErr;
  McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Resp, GraphOwner.Asset);
  Resp->SetArrayField(TEXT("results"), Results);
  if (Errors.Num() > 0)
  {
    Resp->SetArrayField(TEXT("errors"), Errors);
  }
  Resp->SetNumberField(TEXT("affectedCount"), SuccessCount);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Material node groups wrapped in comments"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false, TEXT("wrap_material_nodes_in_comments requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
