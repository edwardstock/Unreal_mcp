// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_Material_NodePositioning.cpp
//
// Task D.1 - position + align migration.
// Plan: docs/superpowers/plans/2026-05-07-mcp-material-tools-redesign.md
//
// Houses the node-positioning handlers moved out of
// McpAutomationBridge_AssetWorkflowHandlers.cpp:
//   - HandleSetMaterialNodePositions (canonical plural; replaces the singular
//     set_material_node_position / move_material_node aliases and the bulk
//     bulk_set_material_node_positions / bulk_move_material_nodes aliases).
//   - HandleAlignMaterialNodes (already array-shaped; only the file moved).

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR

#include "Materials/MaterialExpression.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "GraphEditor.h"

// Helpers defined as file-scope externs in McpAutomationBridge_AssetWorkflowHandlers.cpp.
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

#endif // WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleSetMaterialNodePositions(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("set_material_node_positions"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("set_material_node_positions payload missing"), TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    SendAutomationError(Socket, RequestId, TEXT("assetPath or materialPath is required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
  if (!Payload->TryGetArrayField(TEXT("nodes"), Nodes) || !Nodes || Nodes->Num() == 0) {
    SendAutomationError(Socket, RequestId, TEXT("nodes array is required"), TEXT("INVALID_ARGUMENT"));
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
    SendAutomationError(Socket, RequestId, TEXT("Cannot move nodes on a MaterialFunctionInstance - edit the parent function instead"), TEXT("UNSUPPORTED_OPERATION"));
    return true;
  }

  int32 UpdatedCount = 0;
  TArray<TSharedPtr<FJsonValue>> UpdatedNodes;
  for (const TSharedPtr<FJsonValue>& NodeValue : *Nodes) {
    const TSharedPtr<FJsonObject>* NodeObj = nullptr;
    if (!NodeValue.IsValid() || !NodeValue->TryGetObject(NodeObj) || !NodeObj) {
      continue;
    }

    UMaterialExpression* Expression = McpFindGraphExpressionFromPayload(GraphOwner, *NodeObj, TEXT("expressionIndex"), TEXT("nodeId"), TEXT("expressionPath"));
    double NewX = 0.0;
    double NewY = 0.0;
    const bool bHasX = (*NodeObj)->TryGetNumberField(TEXT("x"), NewX) || (*NodeObj)->TryGetNumberField(TEXT("posX"), NewX);
    const bool bHasY = (*NodeObj)->TryGetNumberField(TEXT("y"), NewY) || (*NodeObj)->TryGetNumberField(TEXT("posY"), NewY);
    if (!Expression || !bHasX || !bHasY) {
      continue;
    }

    Expression->Modify();
    Expression->MaterialExpressionEditorX = static_cast<int32>(NewX);
    Expression->MaterialExpressionEditorY = static_cast<int32>(NewY);
    UpdatedCount++;

    TSharedPtr<FJsonObject> UpdatedObj = McpHandlerUtils::CreateResultObject();
    McpAddExpressionIdentity(GraphOwner, Expression, McpExpressionIndex(GraphOwner, Expression), UpdatedObj.ToSharedRef());
    UpdatedNodes.Add(MakeShared<FJsonValueObject>(UpdatedObj));
  }

  FString RebuildErr;
  McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Resp, GraphOwner.Asset);
  Resp->SetNumberField(TEXT("affectedNodeCount"), UpdatedCount);
  Resp->SetArrayField(TEXT("nodes"), UpdatedNodes);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Material node positions updated"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false, TEXT("set_material_node_positions requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleAlignMaterialNodes(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("align_material_nodes"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("align_material_nodes payload missing"), TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Backend = TEXT("native");
  Payload->TryGetStringField(TEXT("backend"), Backend);

  FString MaterialPath, Operation;
  if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    SendAutomationError(Socket, RequestId, TEXT("assetPath or materialPath is required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("operation"), Operation) || Operation.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("operation is required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
  if (!Payload->TryGetArrayField(TEXT("nodes"), Nodes) || !Nodes || Nodes->Num() < 2) {
    SendAutomationError(Socket, RequestId, TEXT("nodes array with at least two nodes is required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FMcpMaterialGraphOwner GraphOwner;
  FString GraphOwnerError;
  if (!McpResolveMaterialGraphOwner(MaterialPath, GraphOwner, GraphOwnerError)) {
    SendAutomationError(Socket, RequestId, GraphOwnerError,
                        GraphOwnerError.Contains(TEXT("not found")) ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE"));
    return true;
  }

  TArray<UMaterialExpression*> Resolved;
  for (const TSharedPtr<FJsonValue>& NodeValue : *Nodes) {
    const TSharedPtr<FJsonObject>* NodeObj = nullptr;
    if (NodeValue.IsValid() && NodeValue->TryGetObject(NodeObj) && NodeObj) {
      if (UMaterialExpression* Expr = McpFindGraphExpressionFromPayload(GraphOwner, *NodeObj, TEXT("expressionIndex"), TEXT("nodeId"), TEXT("expressionPath"))) {
        Resolved.Add(Expr);
      }
    }
  }
  if (Resolved.Num() < 2) {
    SendAutomationError(Socket, RequestId, TEXT("Fewer than two nodes could be resolved"), TEXT("NODE_NOT_FOUND"));
    return true;
  }

  if (Backend.Equals(TEXT("graph_editor"), ESearchCase::IgnoreCase)) {
    UEdGraph* Graph = nullptr;
    TArray<UEdGraphNode*> GraphNodes;
    for (UMaterialExpression* Expr : Resolved) {
      UEdGraphNode* GraphNode = Expr ? Expr->GraphNode : nullptr;
      if (!GraphNode) {
        SendAutomationError(Socket, RequestId, TEXT("Selected material expression does not have a graph editor node"), TEXT("GRAPH_EDITOR_NODE_NOT_FOUND"));
        return true;
      }
      UEdGraph* NodeGraph = GraphNode->GetGraph();
      if (!NodeGraph) {
        SendAutomationError(Socket, RequestId, TEXT("Selected material expression graph node is not attached to a graph"), TEXT("GRAPH_EDITOR_NODE_NOT_FOUND"));
        return true;
      }
      if (!Graph) {
        Graph = NodeGraph;
      } else if (Graph != NodeGraph) {
        SendAutomationError(Socket, RequestId, TEXT("Selected material expressions are not in the same graph editor"), TEXT("INVALID_ARGUMENT"));
        return true;
      }
      GraphNodes.Add(GraphNode);
    }

    TSharedPtr<SGraphEditor> GraphEditor = SGraphEditor::FindGraphEditorForGraph(Graph);
    if (!GraphEditor.IsValid()) {
      SendAutomationError(Socket, RequestId, TEXT("No open graph editor widget found for the requested material graph"), TEXT("GRAPH_EDITOR_UNAVAILABLE"));
      return true;
    }

    const FGraphPanelSelectionSet PreviousSelection = GraphEditor->GetSelectedNodes();
    GraphEditor->ClearSelectionSet();
    for (UEdGraphNode* GraphNode : GraphNodes) {
      GraphEditor->SetNodeSelection(GraphNode, true);
    }

    if (Operation.Equals(TEXT("align_left"), ESearchCase::IgnoreCase)) {
      GraphEditor->OnAlignLeft();
    } else if (Operation.Equals(TEXT("align_right"), ESearchCase::IgnoreCase)) {
      GraphEditor->OnAlignRight();
    } else if (Operation.Equals(TEXT("align_top"), ESearchCase::IgnoreCase)) {
      GraphEditor->OnAlignTop();
    } else if (Operation.Equals(TEXT("align_bottom"), ESearchCase::IgnoreCase)) {
      GraphEditor->OnAlignBottom();
    } else if (Operation.Equals(TEXT("align_center"), ESearchCase::IgnoreCase)) {
      GraphEditor->OnAlignCenter();
    } else if (Operation.Equals(TEXT("align_middle"), ESearchCase::IgnoreCase)) {
      GraphEditor->OnAlignMiddle();
    } else if (Operation.Equals(TEXT("distribute_horizontal"), ESearchCase::IgnoreCase)) {
      GraphEditor->OnDistributeNodesH();
    } else if (Operation.Equals(TEXT("distribute_vertical"), ESearchCase::IgnoreCase)) {
      GraphEditor->OnDistributeNodesV();
    } else if (Operation.Equals(TEXT("straighten_connections"), ESearchCase::IgnoreCase)) {
      GraphEditor->OnStraightenConnections();
    } else {
      GraphEditor->ClearSelectionSet();
      for (UObject* SelectedObject : PreviousSelection) {
        if (UEdGraphNode* PreviousNode = Cast<UEdGraphNode>(SelectedObject)) {
          GraphEditor->SetNodeSelection(PreviousNode, true);
        }
      }
      SendAutomationError(Socket, RequestId, TEXT("Unsupported alignment operation"), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    GraphEditor->ClearSelectionSet();
    for (UObject* SelectedObject : PreviousSelection) {
      if (UEdGraphNode* PreviousNode = Cast<UEdGraphNode>(SelectedObject)) {
        GraphEditor->SetNodeSelection(PreviousNode, true);
      }
    }

    FString RebuildErr;
    McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Resp, GraphOwner.Asset);
    Resp->SetStringField(TEXT("backend"), TEXT("graph_editor"));
    Resp->SetStringField(TEXT("operation"), Operation);
    Resp->SetNumberField(TEXT("affectedNodeCount"), Resolved.Num());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Material nodes aligned with graph editor"), Resp, FString());
    return true;
  }

  int32 MinX = TNumericLimits<int32>::Max(), MinY = TNumericLimits<int32>::Max();
  int32 MaxX = TNumericLimits<int32>::Min(), MaxY = TNumericLimits<int32>::Min();
  for (UMaterialExpression* Expr : Resolved) {
    const FIntPoint Size = McpEstimateExpressionSize(Expr);
    MinX = FMath::Min(MinX, Expr->MaterialExpressionEditorX);
    MinY = FMath::Min(MinY, Expr->MaterialExpressionEditorY);
    MaxX = FMath::Max(MaxX, Expr->MaterialExpressionEditorX + Size.X);
    MaxY = FMath::Max(MaxY, Expr->MaterialExpressionEditorY + Size.Y);
  }

  Resolved.Sort([](const UMaterialExpression& A, const UMaterialExpression& B) {
    return A.MaterialExpressionEditorX == B.MaterialExpressionEditorX
        ? A.MaterialExpressionEditorY < B.MaterialExpressionEditorY
        : A.MaterialExpressionEditorX < B.MaterialExpressionEditorX;
  });

  for (int32 i = 0; i < Resolved.Num(); ++i) {
    UMaterialExpression* Expr = Resolved[i];
    const FIntPoint Size = McpEstimateExpressionSize(Expr);
    Expr->Modify();
    if (Operation.Equals(TEXT("align_left"), ESearchCase::IgnoreCase)) {
      Expr->MaterialExpressionEditorX = MinX;
    } else if (Operation.Equals(TEXT("align_right"), ESearchCase::IgnoreCase)) {
      Expr->MaterialExpressionEditorX = MaxX - Size.X;
    } else if (Operation.Equals(TEXT("align_top"), ESearchCase::IgnoreCase)) {
      Expr->MaterialExpressionEditorY = MinY;
    } else if (Operation.Equals(TEXT("align_bottom"), ESearchCase::IgnoreCase)) {
      Expr->MaterialExpressionEditorY = MaxY - Size.Y;
    } else if (Operation.Equals(TEXT("align_center"), ESearchCase::IgnoreCase)) {
      Expr->MaterialExpressionEditorX = (MinX + MaxX - Size.X) / 2;
    } else if (Operation.Equals(TEXT("align_middle"), ESearchCase::IgnoreCase)) {
      Expr->MaterialExpressionEditorY = (MinY + MaxY - Size.Y) / 2;
    } else if (Operation.Equals(TEXT("distribute_horizontal"), ESearchCase::IgnoreCase) && Resolved.Num() > 2) {
      Expr->MaterialExpressionEditorX = MinX + ((MaxX - MinX) * i / (Resolved.Num() - 1));
    } else if (Operation.Equals(TEXT("distribute_vertical"), ESearchCase::IgnoreCase) && Resolved.Num() > 2) {
      Expr->MaterialExpressionEditorY = MinY + ((MaxY - MinY) * i / (Resolved.Num() - 1));
    } else if (Operation.Equals(TEXT("straighten_connections"), ESearchCase::IgnoreCase)) {
      Expr->MaterialExpressionEditorY = MinY;
    }
  }

  FString RebuildErr;
  McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Resp, GraphOwner.Asset);
  Resp->SetStringField(TEXT("backend"), TEXT("native"));
  Resp->SetStringField(TEXT("operation"), Operation);
  Resp->SetNumberField(TEXT("affectedNodeCount"), Resolved.Num());
  SendAutomationResponse(Socket, RequestId, true, TEXT("Material nodes aligned"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false, TEXT("align_material_nodes requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
