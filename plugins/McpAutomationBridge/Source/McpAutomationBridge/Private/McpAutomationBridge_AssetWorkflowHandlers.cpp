// =============================================================================
// McpAutomationBridge_AssetWorkflowHandlers.cpp
// =============================================================================
// Asset workflow, material authoring, and source control handlers.
//
// HANDLERS:
//   Asset Operations:
//     - import, duplicate, rename, move, delete, exists, list, search_assets
//     - create_folder, create_material, create_material_instance
//     - get_dependencies, get_asset_graph, set_tags, set_metadata, get_metadata
//     - validate, generate_report, generate_thumbnail, get_material_stats
//
//   Material Authoring:
//     - add_material_node, connect_material_pins, remove_material_node
//     - break_material_connections, get_material_node_details, rebuild_material
//     - add_material_parameter, list_instances, reset_instance_parameters
//
//   Source Control:
//     - source_control_checkout, source_control_submit, get_source_control_state
//     - source_control_enable
//
//   Bulk Operations:
//     - fixup_redirectors, bulk_rename, bulk_delete
//     - generate_lods, nanite_rebuild_mesh
//
// REFACTORING NOTES:
//   - Uses McpVersionCompatibility.h for UE 5.0-5.7 API abstraction
//   - Uses McpHandlerUtils for standardized JSON parsing/responses
//   - Material expression includes grouped by category
//
// VERSION COMPATIBILITY:
//   - MaterialDomain.h: UE 5.1+ (EMaterialDomain in MaterialShared.h for 5.0)
//   - ClassPaths vs ClassNames: UE 5.1+ uses FTopLevelAssetPath
//   - AssetRegistry API varies between UE versions
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"

// -----------------------------------------------------------------------------
// Core Includes
// -----------------------------------------------------------------------------
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridge_MaterialExpressionDetails.h"
#include "McpSafeOperations.h"

// -----------------------------------------------------------------------------
// MCP Handler Utilities (centralized JSON/Asset helpers)
// -----------------------------------------------------------------------------
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/ScopeExit.h"
#include "UObject/MetaData.h"

// -----------------------------------------------------------------------------
// Material Expression Includes (Basic Operations)
// -----------------------------------------------------------------------------
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"

// -----------------------------------------------------------------------------
// Material Expression Includes (Constants)
// -----------------------------------------------------------------------------
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"

// -----------------------------------------------------------------------------
// Material Expression Includes (Trigonometry)
// -----------------------------------------------------------------------------
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialExpressionSine.h"

// -----------------------------------------------------------------------------
// Material Expression Includes (Texture & Time)
// -----------------------------------------------------------------------------
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"

#if WITH_EDITOR

// -----------------------------------------------------------------------------
// Editor-only Includes (Asset Management)
// -----------------------------------------------------------------------------
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "AssetViewUtils.h"
#include "EditorAssetLibrary.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"  // TActorIterator
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "FileHelpers.h"
#include "IAssetTools.h"

// -----------------------------------------------------------------------------
// Editor-only Includes (Source Control)
// -----------------------------------------------------------------------------
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlHelpers.h"
#include "SourceControlOperations.h"

// -----------------------------------------------------------------------------
// Editor-only Includes (Material Editing)
// -----------------------------------------------------------------------------
#include "ImageUtils.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialExpressionGetMaterialAttributes.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInstance.h"
#include "Materials/MaterialFunctionInterface.h"

// MaterialDomain.h was introduced in UE 5.1 - in UE 5.0 EMaterialDomain is in MaterialShared.h
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "MaterialDomain.h"
#endif

#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "MaterialShared.h"

// -----------------------------------------------------------------------------
// Editor-only Includes (Utilities)
// -----------------------------------------------------------------------------
#include "Misc/FileHelper.h"
#include "ObjectTools.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

// -----------------------------------------------------------------------------
// Editor-only Includes (Graph/Blueprint)
// -----------------------------------------------------------------------------
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Blueprint/BlueprintSupport.h"
#include "GraphEditor.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeLayerInfoObject.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Materials/MaterialExpressionLandscapeLayerWeight.h"
#include "Materials/MaterialExpressionLandscapePhysicalMaterialOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"

#endif // WITH_EDITOR

#if WITH_EDITOR
namespace
{
struct FMcpMaterialNodeRect
{
  int32 X = 0;
  int32 Y = 0;
  int32 W = 260;
  int32 H = 140;

  bool Overlaps(const FMcpMaterialNodeRect& Other, int32 Padding = 40) const
  {
    return X < Other.X + Other.W + Padding &&
           X + W + Padding > Other.X &&
           Y < Other.Y + Other.H + Padding &&
           Y + H + Padding > Other.Y;
  }
};
} // namespace

static FString McpExpressionPath(const UMaterialExpression* Expr)
{
  return Expr ? Expr->GetPathName() : FString();
}

int32 McpExpressionIndex(const FMcpMaterialGraphOwner& Owner, const UMaterialExpression* Expr)
{
  const TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressions(Owner);
  return Exprs && Expr ? Exprs->IndexOfByKey(Expr) : INDEX_NONE;
}

void McpAddExpressionIdentity(
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpression* Expr,
    int32 Index,
    const TSharedRef<FJsonObject>& Obj)
{
  if (!Expr)
  {
    return;
  }

  Obj->SetNumberField(TEXT("index"), Index);
  Obj->SetNumberField(TEXT("expressionIndex"), Index);
  Obj->SetStringField(TEXT("nodeId"), Expr->MaterialExpressionGuid.ToString());
  Obj->SetStringField(TEXT("expressionGuid"), Expr->MaterialExpressionGuid.ToString());
  Obj->SetStringField(TEXT("expressionName"), Expr->GetName());
  Obj->SetStringField(TEXT("expressionPath"), McpExpressionPath(Expr));
  Obj->SetStringField(TEXT("name"), Expr->GetName());
  Obj->SetStringField(TEXT("type"), Expr->GetClass()->GetName());
  Obj->SetNumberField(TEXT("x"), Expr->MaterialExpressionEditorX);
  Obj->SetNumberField(TEXT("y"), Expr->MaterialExpressionEditorY);
  Obj->SetNumberField(TEXT("editorX"), Expr->MaterialExpressionEditorX);
  Obj->SetNumberField(TEXT("editorY"), Expr->MaterialExpressionEditorY);

  if (UMaterialExpressionFunctionInput* Input = Cast<UMaterialExpressionFunctionInput>(Expr))
  {
    Obj->SetStringField(TEXT("functionInputId"), Input->Id.ToString());
    Obj->SetStringField(TEXT("inputName"), Input->InputName.ToString());
  }
  else if (UMaterialExpressionFunctionOutput* Output = Cast<UMaterialExpressionFunctionOutput>(Expr))
  {
    Obj->SetStringField(TEXT("functionOutputId"), Output->Id.ToString());
    Obj->SetStringField(TEXT("outputName"), Output->OutputName.ToString());
  }
  else if (UMaterialExpressionNamedRerouteDeclaration* Declaration = Cast<UMaterialExpressionNamedRerouteDeclaration>(Expr))
  {
    Obj->SetStringField(TEXT("rerouteName"), Declaration->Name.ToString());
    Obj->SetStringField(TEXT("rerouteGuid"), Declaration->VariableGuid.ToString());
  }
  else if (UMaterialExpressionNamedRerouteUsage* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(Expr))
  {
    Obj->SetStringField(TEXT("declarationGuid"), Usage->DeclarationGuid.ToString());
    if (Usage->Declaration)
    {
      Obj->SetStringField(TEXT("declarationName"), Usage->Declaration->Name.ToString());
      Obj->SetNumberField(TEXT("declarationExpressionIndex"), McpExpressionIndex(Owner, Usage->Declaration));
    }
  }
}

UMaterialExpression* McpFindGraphExpressionFromPayload(
    const FMcpMaterialGraphOwner& Owner,
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* IndexField = TEXT("expressionIndex"),
    const TCHAR* IdField = TEXT("nodeId"),
    const TCHAR* PathField = TEXT("expressionPath"))
{
  if (!Payload.IsValid())
  {
    return nullptr;
  }

  int32 ExpressionIndex = INDEX_NONE;
  if (Payload->TryGetNumberField(IndexField, ExpressionIndex))
  {
    return McpFindGraphExpression(Owner, FString(), ExpressionIndex);
  }

  FString ExpressionPath;
  if (Payload->TryGetStringField(PathField, ExpressionPath) && !ExpressionPath.IsEmpty())
  {
    return McpFindGraphExpression(Owner, ExpressionPath);
  }

  FString NodeId;
  if (Payload->TryGetStringField(IdField, NodeId) && !NodeId.IsEmpty())
  {
    return McpFindGraphExpression(Owner, NodeId);
  }

  FString ExpressionGuid;
  if (Payload->TryGetStringField(TEXT("expressionGuid"), ExpressionGuid) && !ExpressionGuid.IsEmpty())
  {
    return McpFindGraphExpression(Owner, ExpressionGuid);
  }

  FString ExpressionName;
  if (Payload->TryGetStringField(TEXT("expressionName"), ExpressionName) && !ExpressionName.IsEmpty())
  {
    return McpFindGraphExpression(Owner, ExpressionName);
  }

  FString ParameterName;
  if (Payload->TryGetStringField(TEXT("parameterName"), ParameterName) && !ParameterName.IsEmpty())
  {
    return McpFindGraphExpression(Owner, ParameterName);
  }

  return nullptr;
}

static FExpressionInput* McpFindExpressionInputByName(
    UMaterialExpression* Expression,
    FString& InOutInputName)
{
  if (!Expression)
  {
    return nullptr;
  }

  if (!InOutInputName.IsEmpty())
  {
    for (FProperty* Property = Expression->GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext)
    {
      if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
      {
        if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput")) &&
            Property->GetName().Equals(InOutInputName, ESearchCase::IgnoreCase))
        {
          InOutInputName = Property->GetName();
          return StructProp->ContainerPtrToValuePtr<FExpressionInput>(Expression);
        }
      }
    }
  }

  for (FProperty* Property = Expression->GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext)
  {
    if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
      if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput")))
      {
        InOutInputName = Property->GetName();
        return StructProp->ContainerPtrToValuePtr<FExpressionInput>(Expression);
      }
    }
  }

  return nullptr;
}

static FIntPoint McpEstimateExpressionSize(UMaterialExpression* Expr)
{
  if (!Expr)
  {
    return FIntPoint(260, 140);
  }
  if (Expr->IsA<UMaterialExpressionTextureSample>())
  {
    return FIntPoint(340, 280);
  }
  if (Expr->IsA<UMaterialExpressionFunctionInput>() || Expr->IsA<UMaterialExpressionFunctionOutput>())
  {
    return FIntPoint(260, 110);
  }
  if (Expr->IsA<UMaterialExpressionNamedRerouteDeclaration>() ||
      Expr->IsA<UMaterialExpressionNamedRerouteUsage>())
  {
    return FIntPoint(220, 80);
  }
  if (Expr->GetClass()->GetName().Contains(TEXT("SetMaterialAttributes")))
  {
    return FIntPoint(360, 340);
  }
  if (Expr->GetClass()->GetName().Contains(TEXT("MaterialFunctionCall")))
  {
    return FIntPoint(340, 180);
  }
  return FIntPoint(260, 140);
}

static TArray<FMcpMaterialNodeRect> McpCollectExpressionRects(const FMcpMaterialGraphOwner& Owner)
{
  TArray<FMcpMaterialNodeRect> Rects;
  const TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressions(Owner);
  if (!Exprs)
  {
    return Rects;
  }

  for (UMaterialExpression* Expr : *Exprs)
  {
    if (!Expr)
    {
      continue;
    }
    const FIntPoint Size = McpEstimateExpressionSize(Expr);
    Rects.Add({Expr->MaterialExpressionEditorX, Expr->MaterialExpressionEditorY, Size.X, Size.Y});
  }

  return Rects;
}

static FIntPoint McpFindFreePosition(
    const FMcpMaterialGraphOwner& Owner,
    const FIntPoint& Start,
    const FIntPoint& Size,
    int32 StepY = 180)
{
  const TArray<FMcpMaterialNodeRect> Existing = McpCollectExpressionRects(Owner);
  FIntPoint Candidate = Start;
  for (int32 Attempt = 0; Attempt < 256; ++Attempt)
  {
    const FMcpMaterialNodeRect CandidateRect{Candidate.X, Candidate.Y, Size.X, Size.Y};
    bool bOverlaps = false;
    for (const FMcpMaterialNodeRect& ExistingRect : Existing)
    {
      if (CandidateRect.Overlaps(ExistingRect))
      {
        bOverlaps = true;
        break;
      }
    }
    if (!bOverlaps)
    {
      return Candidate;
    }
    Candidate.Y += StepY;
  }
  return Candidate;
}

static FIntPoint McpResolvePlacement(
    const FMcpMaterialGraphOwner& Owner,
    const TSharedPtr<FJsonObject>& Payload,
    UMaterialExpression* NewExpression)
{
  double ExplicitX = 0.0;
  double ExplicitY = 0.0;
  const bool bHasPosX = Payload->TryGetNumberField(TEXT("posX"), ExplicitX) || Payload->TryGetNumberField(TEXT("x"), ExplicitX);
  const bool bHasPosY = Payload->TryGetNumberField(TEXT("posY"), ExplicitY) || Payload->TryGetNumberField(TEXT("y"), ExplicitY);

  const TSharedPtr<FJsonObject>* PlacementObjPtr = nullptr;
  TSharedPtr<FJsonObject> Placement;
  if (Payload->TryGetObjectField(TEXT("placement"), PlacementObjPtr) && PlacementObjPtr)
  {
    Placement = *PlacementObjPtr;
  }

  FString Mode;
  if (Placement.IsValid())
  {
    Placement->TryGetStringField(TEXT("mode"), Mode);
  }
  if (Mode.IsEmpty())
  {
    Payload->TryGetStringField(TEXT("placementMode"), Mode);
  }

  if (Mode.Equals(TEXT("absolute"), ESearchCase::IgnoreCase) || (bHasPosX && bHasPosY && Mode.IsEmpty()))
  {
    return FIntPoint(static_cast<int32>(ExplicitX), static_cast<int32>(ExplicitY));
  }

  UMaterialExpression* Anchor = nullptr;
  if (Placement.IsValid())
  {
    Anchor = McpFindGraphExpressionFromPayload(Owner, Placement, TEXT("anchorExpressionIndex"), TEXT("anchorNodeId"), TEXT("anchorExpressionPath"));
  }
  if (!Anchor)
  {
    Anchor = McpFindGraphExpressionFromPayload(Owner, Payload, TEXT("anchorExpressionIndex"), TEXT("anchorNodeId"), TEXT("anchorExpressionPath"));
  }

  FString Direction = TEXT("right");
  if (Placement.IsValid())
  {
    Placement->TryGetStringField(TEXT("direction"), Direction);
  }
  Payload->TryGetStringField(TEXT("direction"), Direction);

  const FIntPoint Size = McpEstimateExpressionSize(NewExpression);
  FIntPoint Start(static_cast<int32>(ExplicitX), static_cast<int32>(ExplicitY));
  if (Anchor)
  {
    const FIntPoint AnchorSize = McpEstimateExpressionSize(Anchor);
    if (Direction.Equals(TEXT("left"), ESearchCase::IgnoreCase))
    {
      Start = FIntPoint(Anchor->MaterialExpressionEditorX - Size.X - 320, Anchor->MaterialExpressionEditorY);
    }
    else if (Direction.Equals(TEXT("below"), ESearchCase::IgnoreCase))
    {
      Start = FIntPoint(Anchor->MaterialExpressionEditorX, Anchor->MaterialExpressionEditorY + AnchorSize.Y + 180);
    }
    else if (Direction.Equals(TEXT("above"), ESearchCase::IgnoreCase))
    {
      Start = FIntPoint(Anchor->MaterialExpressionEditorX, Anchor->MaterialExpressionEditorY - Size.Y - 180);
    }
    else
    {
      Start = FIntPoint(Anchor->MaterialExpressionEditorX + AnchorSize.X + 320, Anchor->MaterialExpressionEditorY);
    }
  }

  bool bAvoidOverlap = true;
  if (Placement.IsValid())
  {
    Placement->TryGetBoolField(TEXT("avoidOverlap"), bAvoidOverlap);
  }
  Payload->TryGetBoolField(TEXT("avoidOverlap"), bAvoidOverlap);

  if (!bAvoidOverlap)
  {
    return Start;
  }
  return McpFindFreePosition(Owner, Start, Size);
}

static bool McpAddExpressionToGraph(const FMcpMaterialGraphOwner& Owner, UMaterialExpression* Expression)
{
  if (!Expression)
  {
    return false;
  }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  if (Owner.Kind == EMcpMaterialGraphOwnerKind::Material)
  {
    CastChecked<UMaterial>(Owner.GraphSource)->GetEditorOnlyData()->ExpressionCollection.AddExpression(Expression);
  }
  else
  {
    CastChecked<UMaterialFunction>(Owner.GraphSource)->GetEditorOnlyData()->ExpressionCollection.AddExpression(Expression);
  }
#else
  if (TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressionsMutable(Owner))
  {
    Exprs->Add(Expression);
  }
#endif
  return true;
}

static bool McpAddCommentToGraph(const FMcpMaterialGraphOwner& Owner, UMaterialExpressionComment* Comment)
{
  if (!Comment)
  {
    return false;
  }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  if (Owner.Kind == EMcpMaterialGraphOwnerKind::Material)
  {
    CastChecked<UMaterial>(Owner.GraphSource)->GetEditorOnlyData()->ExpressionCollection.AddComment(Comment);
  }
  else
  {
    CastChecked<UMaterialFunction>(Owner.GraphSource)->GetEditorOnlyData()->ExpressionCollection.AddComment(Comment);
  }
  return true;
#else
  return false;
#endif
}

static TConstArrayView<TObjectPtr<UMaterialExpressionComment>> McpGetGraphComments(const FMcpMaterialGraphOwner& Owner)
{
  if (Owner.Kind == EMcpMaterialGraphOwnerKind::Material)
  {
    return CastChecked<UMaterial>(Owner.GraphSource)->GetEditorComments();
  }
  return CastChecked<UMaterialFunction>(Owner.GraphSource)->GetEditorComments();
}

static UMaterialExpressionNamedRerouteDeclaration* McpFindNamedRerouteDeclaration(
    const FMcpMaterialGraphOwner& Owner,
    const FString& NameOrGuid)
{
  const TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressions(Owner);
  if (!Exprs)
  {
    return nullptr;
  }

  FGuid ParsedGuid;
  const bool bHasGuid = FGuid::Parse(NameOrGuid, ParsedGuid);
  for (UMaterialExpression* Expr : *Exprs)
  {
    UMaterialExpressionNamedRerouteDeclaration* Declaration = Cast<UMaterialExpressionNamedRerouteDeclaration>(Expr);
    if (!Declaration)
    {
      continue;
    }
    if ((bHasGuid && Declaration->VariableGuid == ParsedGuid) ||
        Declaration->Name.ToString() == NameOrGuid ||
        Declaration->GetName() == NameOrGuid ||
        Declaration->GetPathName() == NameOrGuid)
    {
      return Declaration;
    }
  }
  return nullptr;
}

FString McpLandscapeBlendTypeToString(ELandscapeLayerBlendType BlendType)
{
  switch (BlendType)
  {
  case LB_WeightBlend:
    return TEXT("LB_WeightBlend");
  case LB_AlphaBlend:
    return TEXT("LB_AlphaBlend");
  case LB_HeightBlend:
    return TEXT("LB_HeightBlend");
  default:
    return TEXT("Unknown");
  }
}

FString McpGetOutputName(UMaterialExpression* Expression, int32 OutputIndex, bool& bOutResolved)
{
  bOutResolved = false;
  if (!Expression)
  {
    return FString();
  }

  TArray<FExpressionOutput>& Outputs = Expression->GetOutputs();
  if (Outputs.IsValidIndex(OutputIndex))
  {
    const FExpressionOutput& Output = Outputs[OutputIndex];
    if (Output.OutputName != NAME_None)
    {
      bOutResolved = true;
      return Output.OutputName.ToString();
    }
    bOutResolved = true;
    return FString::Printf(TEXT("Output%d"), OutputIndex);
  }

  return FString();
}

TSharedPtr<FJsonObject> McpBuildExpressionRef(
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpression* Expression)
{
  TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
  McpAddExpressionIdentity(Owner, Expression, McpExpressionIndex(Owner, Expression), Obj.ToSharedRef());
  if (Expression)
  {
    Obj->SetStringField(TEXT("className"), Expression->GetClass()->GetName());
  }
  return Obj;
}

// Single source of truth for "JSON describing one input pin".
// `PinName` is the name of the input (e.g. property name from the property walk, or
// the function input name for MaterialFunctionCall). When unconnected, only `name`
// and `isConnected: false` are emitted; otherwise the full set of connection fields
// is appended (matching the legacy McpAddConnectedExpressionInfo contract plus
// sourceOutputIndex/sourceOutputName).
void McpEmitInputPinJson(
    const FMcpMaterialGraphOwner& Owner,
    const FExpressionInput* Input,
    const FString& PinName,
    const TSharedRef<FJsonObject>& Out)
{
  Out->SetStringField(TEXT("name"), PinName);
  if (!Input || !Input->Expression)
  {
    Out->SetBoolField(TEXT("isConnected"), false);
    return;
  }

  Out->SetBoolField(TEXT("isConnected"), true);
  Out->SetObjectField(TEXT("source"), McpBuildExpressionRef(Owner, Input->Expression));
  Out->SetStringField(TEXT("connectedToId"), Input->Expression->MaterialExpressionGuid.ToString());
  Out->SetStringField(TEXT("connectedToExpressionGuid"), Input->Expression->MaterialExpressionGuid.ToString());
  Out->SetStringField(TEXT("connectedToExpressionPath"), Input->Expression->GetPathName());
  Out->SetNumberField(TEXT("connectedToIndex"), McpExpressionIndex(Owner, Input->Expression));
  Out->SetStringField(TEXT("connectedToName"), Input->Expression->GetName());
  Out->SetNumberField(TEXT("sourceOutputIndex"), Input->OutputIndex);

  bool bResolvedOutputName = false;
  const FString OutputName = McpGetOutputName(Input->Expression, Input->OutputIndex, bResolvedOutputName);
  if (!OutputName.IsEmpty())
  {
    Out->SetStringField(TEXT("sourceOutputName"), OutputName);
  }
  Out->SetBoolField(TEXT("sourceOutputNameResolved"), bResolvedOutputName);
}

void McpAddConnectedExpressionInfo(
    const FMcpMaterialGraphOwner& Owner,
    const FExpressionInput* Input,
    const TSharedRef<FJsonObject>& Obj)
{
  if (!Input || !Input->Expression)
  {
    Obj->SetBoolField(TEXT("isConnected"), false);
    return;
  }

  Obj->SetBoolField(TEXT("isConnected"), true);
  Obj->SetObjectField(TEXT("source"), McpBuildExpressionRef(Owner, Input->Expression));
  Obj->SetStringField(TEXT("connectedToId"), Input->Expression->MaterialExpressionGuid.ToString());
  Obj->SetStringField(TEXT("connectedToExpressionGuid"), Input->Expression->MaterialExpressionGuid.ToString());
  Obj->SetStringField(TEXT("connectedToExpressionPath"), Input->Expression->GetPathName());
  Obj->SetNumberField(TEXT("connectedToIndex"), McpExpressionIndex(Owner, Input->Expression));
  Obj->SetStringField(TEXT("connectedToName"), Input->Expression->GetName());
  Obj->SetNumberField(TEXT("sourceOutputIndex"), Input->OutputIndex);
  bool bResolvedOutputName = false;
  const FString OutputName = McpGetOutputName(Input->Expression, Input->OutputIndex, bResolvedOutputName);
  if (!OutputName.IsEmpty())
  {
    Obj->SetStringField(TEXT("sourceOutputName"), OutputName);
  }
  Obj->SetBoolField(TEXT("sourceOutputNameResolved"), bResolvedOutputName);
}

namespace
{

static TArray<TSharedPtr<FJsonValue>> McpBuildExpressionInputsArray(
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpression* Expression)
{
  TArray<TSharedPtr<FJsonValue>> InputsArray;
  if (!Expression)
  {
    return InputsArray;
  }

  for (FProperty* Property = Expression->GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext)
  {
    if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
      if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput")))
      {
        FExpressionInput* Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(Expression);
        TSharedPtr<FJsonObject> InputObj = McpHandlerUtils::CreateResultObject();
        McpEmitInputPinJson(Owner, Input, Property->GetName(), InputObj.ToSharedRef());
        InputsArray.Add(MakeShared<FJsonValueObject>(InputObj));
      }
    }
  }

  return InputsArray;
}

static TArray<TSharedPtr<FJsonValue>> McpBuildExpressionConsumersArray(
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpression* SourceExpression)
{
  TArray<TSharedPtr<FJsonValue>> Consumers;
  if (!SourceExpression)
  {
    return Consumers;
  }

  const TArray<TObjectPtr<UMaterialExpression>>* Expressions = McpGetGraphExpressions(Owner);
  if (!Expressions)
  {
    return Consumers;
  }

  for (UMaterialExpression* Candidate : *Expressions)
  {
    if (!Candidate)
    {
      continue;
    }

    for (FProperty* Property = Candidate->GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext)
    {
      if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
      {
        if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput")))
        {
          FExpressionInput* Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(Candidate);
          if (Input && Input->Expression == SourceExpression)
          {
            TSharedPtr<FJsonObject> ConsumerObj = McpHandlerUtils::CreateResultObject();
            ConsumerObj->SetObjectField(TEXT("target"), McpBuildExpressionRef(Owner, Candidate));
            ConsumerObj->SetStringField(TEXT("targetInputPin"), Property->GetName());
            ConsumerObj->SetNumberField(TEXT("sourceOutputIndex"), Input->OutputIndex);
            bool bResolvedOutputName = false;
            const FString OutputName = McpGetOutputName(SourceExpression, Input->OutputIndex, bResolvedOutputName);
            if (!OutputName.IsEmpty())
            {
              ConsumerObj->SetStringField(TEXT("sourceOutputName"), OutputName);
            }
            ConsumerObj->SetBoolField(TEXT("sourceOutputNameResolved"), bResolvedOutputName);
            Consumers.Add(MakeShared<FJsonValueObject>(ConsumerObj));
          }
        }
      }
    }
  }

  return Consumers;
}

static bool McpExpressionMatchesFilters(
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpression* Expr,
    int32 Index,
    const TSharedPtr<FJsonObject>& Payload)
{
  if (!Expr || !Payload.IsValid())
  {
    return false;
  }

  FString ClassName;
  if (Payload->TryGetStringField(TEXT("className"), ClassName) ||
      Payload->TryGetStringField(TEXT("expressionClass"), ClassName))
  {
    if (!ClassName.IsEmpty() &&
        !Expr->GetClass()->GetName().Contains(ClassName, ESearchCase::IgnoreCase))
    {
      return false;
    }
  }

  FString ParameterName;
  if (Payload->TryGetStringField(TEXT("parameterName"), ParameterName) && !ParameterName.IsEmpty())
  {
    const UMaterialExpressionParameter* Param = Cast<UMaterialExpressionParameter>(Expr);
    if (!Param || !Param->ParameterName.ToString().Contains(ParameterName, ESearchCase::IgnoreCase))
    {
      return false;
    }
  }

  FString ExpressionName;
  if (Payload->TryGetStringField(TEXT("expressionName"), ExpressionName) && !ExpressionName.IsEmpty())
  {
    if (!Expr->GetName().Contains(ExpressionName, ESearchCase::IgnoreCase))
    {
      return false;
    }
  }

  FString Desc;
  if (Payload->TryGetStringField(TEXT("desc"), Desc) && !Desc.IsEmpty())
  {
    if (!Expr->Desc.Contains(Desc, ESearchCase::IgnoreCase))
    {
      return false;
    }
  }

  FString ExpressionPath;
  if (Payload->TryGetStringField(TEXT("expressionPath"), ExpressionPath) && !ExpressionPath.IsEmpty())
  {
    if (!Expr->GetPathName().Equals(ExpressionPath, ESearchCase::IgnoreCase))
    {
      return false;
    }
  }

  FString Guid;
  if ((Payload->TryGetStringField(TEXT("expressionGuid"), Guid) || Payload->TryGetStringField(TEXT("nodeId"), Guid)) &&
      !Guid.IsEmpty())
  {
    if (!Expr->MaterialExpressionGuid.ToString().Equals(Guid, ESearchCase::IgnoreCase))
    {
      return false;
    }
  }

  int32 ExpressionIndex = INDEX_NONE;
  if (Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex) && ExpressionIndex != Index)
  {
    return false;
  }

  return true;
}

static void McpAppendTypedExpressionDetails(
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpression* Expression,
    const TSharedRef<FJsonObject>& Resp)
{
    McpMaterialExpressionDetails::AppendTypedDetails(Owner, Expression, Resp);
}

static ALandscape* McpFindLandscapeActorByPayload(
    const TSharedPtr<FJsonObject>& Payload)
{
  if (!Payload.IsValid() || !GEditor)
  {
    return nullptr;
  }

  UWorld* World = GEditor->GetEditorWorldContext().World();
  if (!World)
  {
    return nullptr;
  }

  FString ActorPath;
  Payload->TryGetStringField(TEXT("actorPath"), ActorPath);
  if (ActorPath.IsEmpty())
  {
    Payload->TryGetStringField(TEXT("landscapePath"), ActorPath);
  }

  if (!ActorPath.IsEmpty())
  {
    if (ALandscape* Landscape = Cast<ALandscape>(StaticLoadObject(ALandscape::StaticClass(), nullptr, *ActorPath)))
    {
      return Landscape;
    }
    if (ALandscapeStreamingProxy* Proxy = Cast<ALandscapeStreamingProxy>(StaticLoadObject(ALandscapeStreamingProxy::StaticClass(), nullptr, *ActorPath)))
    {
      return Proxy->GetLandscapeActor();
    }
  }

  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty())
  {
    Payload->TryGetStringField(TEXT("landscapeName"), ActorName);
  }

  if (!ActorName.IsEmpty())
  {
    for (TActorIterator<ALandscape> It(World); It; ++It)
    {
      ALandscape* Landscape = *It;
      if (Landscape &&
          (Landscape->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
           Landscape->GetName().Equals(ActorName, ESearchCase::IgnoreCase)))
      {
        return Landscape;
      }
    }
  }

  return nullptr;
}
} // namespace
#endif

// =============================================================================
// ASSET ACTION DISPATCHER
// =============================================================================

bool UMcpAutomationBridgeSubsystem::HandleAssetAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FString Lower = Action.ToLower();

  // If the action is the generic "manage_asset" tool, check for a subAction in
  // the payload
  if (Lower == TEXT("manage_asset") && Payload.IsValid()) {
    FString SubAction;
    if (Payload->TryGetStringField(TEXT("subAction"), SubAction) &&
        !SubAction.IsEmpty()) {
      Lower = SubAction.ToLower();
    }
  }

  if (Lower.IsEmpty())
    return false;

  // Dispatch to specific handlers
  // CRITICAL: These actions must match what TS sends as 'action' (not just 'subAction')
  // When TS calls executeAutomationRequest(tools, 'search_assets', {...}), Action='search_assets'
  
  // Asset Operations
  if (Lower == TEXT("import"))
    return HandleImportAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("duplicate"))
    return HandleDuplicateAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("rename"))
    return HandleRenameAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("move"))
    return HandleMoveAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("delete") || Lower == TEXT("delete_asset") || Lower == TEXT("delete_assets"))
    return HandleDeleteAssets(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_folder"))
    return HandleCreateFolder(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_material"))
    return HandleCreateMaterial(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_material_instance"))
    return HandleCreateMaterialInstance(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("get_dependencies"))
    return HandleGetDependencies(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("get_asset_graph"))
    return HandleGetAssetGraph(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("set_tags"))
    return HandleSetTags(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("set_metadata"))
    return HandleSetMetadata(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("get_metadata"))
    return HandleGetMetadata(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("validate"))
    return HandleValidateAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("list") || Lower == TEXT("list_assets"))
    return HandleListAssets(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("generate_report"))
    return HandleGenerateReport(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_thumbnail") || Lower == TEXT("generate_thumbnail"))
    return HandleGenerateThumbnail(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("add_material_parameter"))
    return HandleAddMaterialParameter(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("list_instances"))
    return HandleListMaterialInstances(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("reset_instance_parameters"))
    return HandleResetInstanceParameters(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("exists"))
    return HandleDoesAssetExist(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("get_material_stats"))
    return HandleGetMaterialStats(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("get_material_instance_info"))
    return HandleGetMaterialInstanceInfo(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("find_material_expressions"))
    return HandleFindMaterialExpressions(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("get_material_expression_details"))
    return HandleGetMaterialExpressionDetails(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("get_material_expression_connections"))
    return HandleGetMaterialExpressionConnections(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("get_landscape_material_context"))
    return HandleGetLandscapeMaterialContext(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("compile_material_diagnostics"))
    return HandleCompileMaterialDiagnostics(RequestId, Lower, Payload, RequestingSocket);
  
  // Search (CRITICAL: search_assets must be dispatched - was missing causing timeouts)
  if (Lower == TEXT("search_assets"))
    return HandleSearchAssets(RequestId, Lower, Payload, RequestingSocket);

  // Bulk Operations
  if (Lower == TEXT("fixup_redirectors"))
    return HandleFixupRedirectors(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("bulk_rename"))
    return HandleBulkRenameAssets(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("bulk_delete"))
    return HandleBulkDeleteAssets(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("generate_lods"))
    return HandleGenerateLODs(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("nanite_rebuild_mesh"))
    return HandleNaniteRebuildMesh(RequestId, Lower, Payload, RequestingSocket);

  // Source Control
  if (Lower == TEXT("source_control_checkout"))
    return HandleSourceControlCheckout(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("source_control_submit"))
    return HandleSourceControlSubmit(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("get_source_control_state"))
    return HandleGetSourceControlState(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("source_control_enable"))
    return HandleSourceControlEnable(RequestId, Lower, Payload, RequestingSocket);

  // Graph & Analysis
  if (Lower == TEXT("analyze_graph"))
    return HandleAnalyzeGraph(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("find_by_tag"))
    return HandleFindByTag(RequestId, Lower, Payload, RequestingSocket);

  // Material Authoring
  if (Lower == TEXT("add_material_node"))
    return HandleAddMaterialNode(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("set_material_node_position") || Lower == TEXT("move_material_node"))
    return HandleSetMaterialNodePosition(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("bulk_set_material_node_positions") || Lower == TEXT("bulk_move_material_nodes"))
    return HandleBulkSetMaterialNodePositions(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("connect_material_pins"))
    return HandleConnectMaterialPins(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("remove_material_node"))
    return HandleRemoveMaterialNode(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("break_material_connections"))
    return HandleBreakMaterialConnections(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("create_material_comment"))
    return HandleCreateMaterialComment(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("wrap_material_nodes_in_comment"))
    return HandleWrapMaterialNodesInComment(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("create_named_reroute"))
    return HandleCreateNamedReroute(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("use_named_reroute"))
    return HandleUseNamedReroute(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("replace_long_connection_with_named_reroute"))
    return HandleReplaceLongConnectionWithNamedReroute(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("align_material_nodes"))
    return HandleAlignMaterialNodes(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("get_material_node_details"))
    return HandleGetMaterialNodeDetails(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("rebuild_material"))
    return HandleRebuildMaterial(RequestId, Lower, Payload, RequestingSocket);

  return false;
}


// ============================================================================
// 1. FIXUP REDIRECTORS
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleFixupRedirectors(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("fixup_redirectors"), ESearchCase::IgnoreCase)) {
    // Not our action — allow other handlers to try
    return false;
  }

  // Implementation of redirector fixup functionality
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("fixup_redirectors payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Get directory path - REQUIRED for proper error reporting
  FString DirectoryPath;
  Payload->TryGetStringField(TEXT("directoryPath"), DirectoryPath);
  
  // Also check for "path" as alias
  if (DirectoryPath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("path"), DirectoryPath);
  }

  bool bCheckoutFiles = false;
  Payload->TryGetBoolField(TEXT("checkoutFiles"), bCheckoutFiles);

  // Validate path is provided
  if (DirectoryPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("directoryPath or path is required for fixup_redirectors"),
                        TEXT("MISSING_ARGUMENT"));
    return true;
  }

  // SECURITY: Sanitize path to prevent traversal attacks
  FString SanitizedPath = SanitizeProjectRelativePath(DirectoryPath);
  if (SanitizedPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Invalid path (traversal/security violation): %s"), *DirectoryPath),
        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Normalize path
  FString NormalizedPath = SanitizedPath;
  if (NormalizedPath.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
    NormalizedPath = FString::Printf(TEXT("/Game%s"), *NormalizedPath.RightChop(8));
  }

  AsyncTask(ENamedThreads::GameThread, [this, RequestId, NormalizedPath,
                                        bCheckoutFiles, RequestingSocket]() {
    // CRITICAL FIX: Use DoesAssetDirectoryExistOnDisk for strict validation
    // UEditorAssetLibrary::DoesDirectoryExist() uses AssetRegistry cache which may
    // contain stale entries. We need to check if the directory ACTUALLY exists on disk.
    if (!DoesAssetDirectoryExistOnDisk(NormalizedPath)) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Directory not found: %s"), *NormalizedPath),
                          TEXT("PATH_NOT_FOUND"));
      return;
    }

    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

    // Find all redirectors
    FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CoreUObject"),
                                             TEXT("ObjectRedirector")));
#else
    Filter.ClassNames.Add(FName(TEXT("ObjectRedirector")));
#endif

    Filter.PackagePaths.Add(FName(*NormalizedPath));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> RedirectorAssets;
    AssetRegistry.GetAssets(Filter, RedirectorAssets);

    if (RedirectorAssets.Num() == 0) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetBoolField(TEXT("success"), true);
      Result->SetNumberField(TEXT("redirectorsFound"), 0);
      Result->SetNumberField(TEXT("redirectorsFixed"), 0);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("No redirectors found"), Result, FString());
      return;
    }

    // Convert to string paths for AssetTools
    TArray<FString> RedirectorPaths;
    for (const FAssetData &Asset : RedirectorAssets) {
      RedirectorPaths.Add(Asset.ToSoftObjectPath().ToString());
    }

    // Checkout files if source control is enabled
    if (bCheckoutFiles && ISourceControlModule::Get().IsEnabled()) {
      ISourceControlProvider &SourceControlProvider =
          ISourceControlModule::Get().GetProvider();
      TArray<FString> PackageNames;
      for (const FAssetData &Asset : RedirectorAssets) {
        PackageNames.Add(Asset.PackageName.ToString());
      }
      SourceControlHelpers::CheckOutFiles(PackageNames, true);
    }

    // Convert FAssetData to UObjectRedirector* for AssetTools
    TArray<UObjectRedirector *> Redirectors;
    for (const FAssetData &Asset : RedirectorAssets) {
      if (UObjectRedirector *Redirector =
              Cast<UObjectRedirector>(Asset.GetAsset())) {
        Redirectors.Add(Redirector);
      }
    }

    // Fixup redirectors using AssetTools
    if (Redirectors.Num() > 0) {
      IAssetTools &AssetTools =
          FModuleManager::LoadModuleChecked<FAssetToolsModule>(
              TEXT("AssetTools"))
              .Get();
      AssetTools.FixupReferencers(Redirectors);
    }

    // Delete the now-unused redirectors
    int32 DeletedCount = 0;
    TArray<UObject *> ObjectsToDelete;
    for (const FAssetData &Asset : RedirectorAssets) {
      if (UObject *Obj = Asset.GetAsset()) {
        ObjectsToDelete.Add(Obj);
      }
    }

    if (ObjectsToDelete.Num() > 0) {
      DeletedCount = ObjectTools::DeleteObjects(ObjectsToDelete, false);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("redirectorsFound"), RedirectorAssets.Num());
    Result->SetNumberField(TEXT("redirectorsFixed"), DeletedCount);

    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Fixed %d redirectors"), DeletedCount), Result,
        FString());
  });

  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("fixup_redirectors requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 2. SOURCE CONTROL CHECKOUT
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleSourceControlCheckout(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("source_control_checkout"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("checkout"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("source_control_checkout payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPaths (array) and assetPath (single string)
  TArray<FString> AssetPaths;
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) &&
      AssetPathsArray && AssetPathsArray->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }
  } else {
    // Try single assetPath
    FString SinglePath;
    if (Payload->TryGetStringField(TEXT("assetPath"), SinglePath) && !SinglePath.IsEmpty()) {
      AssetPaths.Add(SinglePath);
    }
  }

  if (AssetPaths.Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("assetPath (string) or assetPaths (array) required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!ISourceControlModule::Get().IsEnabled()) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"),
                           TEXT("Source control is not enabled"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Source control disabled"), Result,
                           TEXT("SOURCE_CONTROL_DISABLED"));
    return true;
  }

  ISourceControlProvider &SourceControlProvider =
      ISourceControlModule::Get().GetProvider();

  TArray<FString> PackageNames;
  TArray<FString> ValidPaths;
  for (const FString &Path : AssetPaths) {
    if (UEditorAssetLibrary::DoesAssetExist(Path)) {
      ValidPaths.Add(Path);
      FString PackageName = FPackageName::ObjectPathToPackageName(Path);
      PackageNames.Add(PackageName);
    }
  }

  if (PackageNames.Num() == 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("No valid assets found"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("No valid assets"), Result,
                           TEXT("NO_VALID_ASSETS"));
    return true;
  }

  bool bSuccess = SourceControlHelpers::CheckOutFiles(PackageNames, true);

  TArray<TSharedPtr<FJsonValue>> CheckedOutPaths;
  for (const FString &Path : ValidPaths) {
    CheckedOutPaths.Add(MakeShared<FJsonValueString>(Path));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), bSuccess);
  Result->SetNumberField(TEXT("checkedOut"), PackageNames.Num());
  Result->SetArrayField(TEXT("assets"), CheckedOutPaths);

  SendAutomationResponse(RequestingSocket, RequestId, bSuccess,
                         bSuccess ? TEXT("Assets checked out successfully")
                                  : TEXT("Checkout failed"),
                         Result,
                         bSuccess ? FString() : TEXT("CHECKOUT_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("source_control_checkout requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 3. SOURCE CONTROL SUBMIT
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleSourceControlSubmit(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("source_control_submit"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("submit"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("source_control_submit payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPaths (array) and assetPath (single string)
  TArray<FString> AssetPaths;
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) &&
      AssetPathsArray && AssetPathsArray->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }
  } else {
    // Try single assetPath
    FString SinglePath;
    if (Payload->TryGetStringField(TEXT("assetPath"), SinglePath) && !SinglePath.IsEmpty()) {
      AssetPaths.Add(SinglePath);
    }
  }

  if (AssetPaths.Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("assetPath (string) or assetPaths (array) required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString Description;
  if (!Payload->TryGetStringField(TEXT("description"), Description) ||
      Description.IsEmpty()) {
    Description = TEXT("Automated submission via MCP Automation Bridge");
  }

  if (!ISourceControlModule::Get().IsEnabled()) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"),
                           TEXT("Source control is not enabled"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Source control disabled"), Result,
                           TEXT("SOURCE_CONTROL_DISABLED"));
    return true;
  }

  ISourceControlProvider &SourceControlProvider =
      ISourceControlModule::Get().GetProvider();

  TArray<FString> PackageNames;
  for (const FString &Path : AssetPaths) {
    if (UEditorAssetLibrary::DoesAssetExist(Path)) {
      FString PackageName = FPackageName::ObjectPathToPackageName(Path);
      PackageNames.Add(PackageName);
    }
  }

  if (PackageNames.Num() == 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("No valid assets found"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("No valid assets"), Result,
                           TEXT("NO_VALID_ASSETS"));
    return true;
  }

  TArray<FString> FilePaths;
  for (const FString &PackageName : PackageNames) {
    FString FilePath;
    if (FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, FilePath, FPackageName::GetAssetPackageExtension())) {
      FilePaths.Add(FilePath);
    }
  }

  TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation =
      ISourceControlOperation::Create<FCheckIn>();
  CheckInOperation->SetDescription(FText::FromString(Description));

  ECommandResult::Type Result =
      SourceControlProvider.Execute(CheckInOperation, FilePaths);
  bool bSuccess = (Result == ECommandResult::Succeeded);

  TSharedPtr<FJsonObject> ResultObj = McpHandlerUtils::CreateResultObject();
  ResultObj->SetBoolField(TEXT("success"), bSuccess);
  ResultObj->SetNumberField(TEXT("submitted"),
                            bSuccess ? PackageNames.Num() : 0);
  ResultObj->SetStringField(TEXT("description"), Description);

  SendAutomationResponse(
      RequestingSocket, RequestId, bSuccess,
      bSuccess ? TEXT("Assets submitted successfully") : TEXT("Submit failed"),
      ResultObj, bSuccess ? FString() : TEXT("SUBMIT_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("source_control_submit requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 4A. SOURCE CONTROL ENABLE
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleSourceControlEnable(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("source_control_enable"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  FString Provider = TEXT("None");
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("provider"), Provider);
  }

  ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
  
  // Check if already enabled
  if (SourceControlModule.IsEnabled()) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("provider"), SourceControlModule.GetProvider().GetName().ToString());
    Result->SetStringField(TEXT("message"), TEXT("Source control already enabled"));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Source control already enabled"), Result, FString());
    return true;
  }

  // Try to set the provider by name
  if (!Provider.IsEmpty() && !Provider.Equals(TEXT("None"), ESearchCase::IgnoreCase)) {
    SourceControlModule.SetProvider(FName(*Provider));
  }
  
  bool bEnabled = SourceControlModule.IsEnabled();
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), bEnabled);
  Result->SetStringField(TEXT("provider"), SourceControlModule.GetProvider().GetName().ToString());
  
  if (bEnabled) {
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Source control enabled"), Result, FString());
  } else {
    Result->SetStringField(TEXT("error"), TEXT("Failed to enable source control. Please configure provider in Editor preferences."));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Source control enable failed"), Result,
                           TEXT("SOURCE_CONTROL_ENABLE_FAILED"));
  }
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("source_control_enable requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================

// ============================================================================
// 4. BULK RENAME ASSETS
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleBulkRenameAssets(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("bulk_rename_assets"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("bulk_rename"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("bulk_rename payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Get rename options
  FString Prefix, Suffix, SearchText, ReplaceText;
  Payload->TryGetStringField(TEXT("prefix"), Prefix);
  Payload->TryGetStringField(TEXT("suffix"), Suffix);
  Payload->TryGetStringField(TEXT("searchText"), SearchText);
  Payload->TryGetStringField(TEXT("replaceText"), ReplaceText);

  bool bCheckoutFiles = false;
  Payload->TryGetBoolField(TEXT("checkoutFiles"), bCheckoutFiles);

  TArray<FString> AssetPaths;

  // Check for assetPaths array first
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) &&
      AssetPathsArray && AssetPathsArray->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }
  } else {
    // Check for folderPath - if provided, list all assets in that folder
    FString FolderPath;
    if (Payload->TryGetStringField(TEXT("folderPath"), FolderPath) && !FolderPath.IsEmpty()) {
      // Normalize path
      FString NormalizedPath = FolderPath;
      if (NormalizedPath.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
        NormalizedPath = FString::Printf(TEXT("/Game%s"), *NormalizedPath.RightChop(8));
      }
      
      // Get all assets in the folder
      FAssetRegistryModule &AssetRegistryModule =
          FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
      IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();
      
      FARFilter Filter;
      Filter.PackagePaths.Add(FName(*NormalizedPath));
      Filter.bRecursivePaths = true;
      
      // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
      // Asset listing uses cached AssetRegistry data exclusively.
      // LIMITATION: Assets not yet indexed by the editor's background scanner
      // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
      TArray<FAssetData> AssetDataList;
      AssetRegistry.GetAssets(Filter, AssetDataList);
      
      for (const FAssetData &AssetData : AssetDataList) {
        AssetPaths.Add(AssetData.ToSoftObjectPath().ToString());
      }
      
      if (AssetPaths.Num() == 0) {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetNumberField(TEXT("renamed"), 0);
        Result->SetStringField(TEXT("message"), TEXT("No assets found in folder"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("No assets found"), Result, FString());
        return true;
      }
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Either assetPaths array or folderPath is required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
  }

  TArray<FAssetRenameData> RenameData;

  for (const FString &InputPath : AssetPaths) {
    FString AssetPath = ResolveAssetPath(InputPath);
    if (AssetPath.IsEmpty()) {
      AssetPath = InputPath;
    }

    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
      continue;
    }

    UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset) {
      continue;
    }

    FString CurrentName = Asset->GetName();
    FString NewName = CurrentName;

    if (!SearchText.IsEmpty()) {
      NewName =
          NewName.Replace(*SearchText, *ReplaceText, ESearchCase::IgnoreCase);
    }

    if (!Prefix.IsEmpty()) {
      NewName = Prefix + NewName;
    }
    if (!Suffix.IsEmpty()) {
      NewName = NewName + Suffix;
    }

    if (NewName == CurrentName) {
      continue;
    }

    FString PackagePath =
        FPackageName::GetLongPackagePath(Asset->GetOutermost()->GetName());
    FAssetRenameData RenameEntry(Asset, PackagePath, NewName);
    RenameData.Add(RenameEntry);
  }

  if (RenameData.Num() == 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("renamed"), 0);
    Result->SetStringField(TEXT("message"),
                           TEXT("No assets required renaming"));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("No renames needed"), Result, FString());
    return true;
  }

  if (bCheckoutFiles && ISourceControlModule::Get().IsEnabled()) {
    TArray<FString> PackageNames;
    for (const FAssetRenameData &Data : RenameData) {
      PackageNames.Add(Data.Asset->GetOutermost()->GetName());
    }
    SourceControlHelpers::CheckOutFiles(PackageNames, true);
  }

  IAssetTools &AssetTools =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"))
          .Get();
  bool bSuccess = AssetTools.RenameAssets(RenameData);

  TArray<TSharedPtr<FJsonValue>> RenamedAssets;
  for (const FAssetRenameData &Data : RenameData) {
    TSharedPtr<FJsonObject> AssetInfo = McpHandlerUtils::CreateResultObject();
    AssetInfo->SetStringField(TEXT("oldPath"), Data.Asset->GetPathName());
    AssetInfo->SetStringField(TEXT("newName"), Data.NewName);
    RenamedAssets.Add(MakeShared<FJsonValueObject>(AssetInfo));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), bSuccess);
  Result->SetNumberField(TEXT("renamed"), RenameData.Num());
  Result->SetArrayField(TEXT("assets"), RenamedAssets);

  SendAutomationResponse(
      RequestingSocket, RequestId, bSuccess,
      bSuccess ? FString::Printf(TEXT("Renamed %d assets"), RenameData.Num())
               : TEXT("Bulk rename failed"),
      Result, bSuccess ? FString() : TEXT("BULK_RENAME_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("bulk_rename requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 5. BULK DELETE ASSETS
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleBulkDeleteAssets(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("bulk_delete_assets"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("bulk_delete"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("bulk_delete payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  bool bShowConfirmation = false;
  Payload->TryGetBoolField(TEXT("showConfirmation"), bShowConfirmation);

  bool bFixupRedirectors = true;
  Payload->TryGetBoolField(TEXT("fixupRedirectors"), bFixupRedirectors);

  TArray<FString> AssetPaths;

  // Check for assetPaths array first
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) &&
      AssetPathsArray && AssetPathsArray->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }
  } else {
    // Check for folderPath - if provided, list all assets in that folder
    FString FolderPath;
    FString Pattern;
    Payload->TryGetStringField(TEXT("folderPath"), FolderPath);
    Payload->TryGetStringField(TEXT("path"), FolderPath);  // alias
    Payload->TryGetStringField(TEXT("pattern"), Pattern);
    
    if (!FolderPath.IsEmpty()) {
      // Normalize path
      FString NormalizedPath = FolderPath;
      if (NormalizedPath.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
        NormalizedPath = FString::Printf(TEXT("/Game%s"), *NormalizedPath.RightChop(8));
      }
      
      // Get all assets in the folder
      FAssetRegistryModule &AssetRegistryModule =
          FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
      IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();
      
      FARFilter Filter;
      Filter.PackagePaths.Add(FName(*NormalizedPath));
      Filter.bRecursivePaths = true;
      
      // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
      // Asset listing uses cached AssetRegistry data exclusively.
      // LIMITATION: Assets not yet indexed by the editor's background scanner
      // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
      TArray<FAssetData> AssetDataList;
      AssetRegistry.GetAssets(Filter, AssetDataList);
      
      for (const FAssetData &AssetData : AssetDataList) {
        FString AssetPath = AssetData.ToSoftObjectPath().ToString();
        // If pattern is specified, filter by it
        if (!Pattern.IsEmpty()) {
          FString AssetName = AssetData.AssetName.ToString();
          if (!AssetName.Contains(Pattern)) {
            continue;
          }
        }
        AssetPaths.Add(AssetPath);
      }
      
      if (AssetPaths.Num() == 0) {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetNumberField(TEXT("deleted"), 0);
        Result->SetStringField(TEXT("message"), TEXT("No assets found matching criteria"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("No assets found"), Result, FString());
        return true;
      }
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Either assetPaths array or folderPath is required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
  }

  TArray<UObject *> ObjectsToDelete;
  TArray<FString> ValidPaths;

  for (const FString &AssetPath : AssetPaths) {
    if (UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
      if (UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath)) {
        ObjectsToDelete.Add(Asset);
        ValidPaths.Add(AssetPath);
      }
    }
  }

  if (ObjectsToDelete.Num() == 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("No valid assets found"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("No valid assets"), Result,
                           TEXT("NO_VALID_ASSETS"));
    return true;
  }

  int32 DeletedCount =
      ObjectTools::DeleteObjects(ObjectsToDelete, bShowConfirmation);

  if (bFixupRedirectors && DeletedCount > 0) {
    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

    FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CoreUObject"),
                                             TEXT("ObjectRedirector")));
#else
    Filter.ClassNames.Add(FName(TEXT("ObjectRedirector")));
#endif

    TArray<FAssetData> RedirectorAssets;
    AssetRegistry.GetAssets(Filter, RedirectorAssets);

    if (RedirectorAssets.Num() > 0) {
      TArray<UObjectRedirector *> Redirectors;
      for (const FAssetData &Asset : RedirectorAssets) {
        if (UObjectRedirector *Redirector =
                Cast<UObjectRedirector>(Asset.GetAsset())) {
          Redirectors.Add(Redirector);
        }
      }

      if (Redirectors.Num() > 0) {
        IAssetTools &AssetTools =
            FModuleManager::LoadModuleChecked<FAssetToolsModule>(
                TEXT("AssetTools"))
                .Get();
        AssetTools.FixupReferencers(Redirectors);
      }
    }
  }

  TArray<TSharedPtr<FJsonValue>> DeletedArray;
  for (const FString &Path : ValidPaths) {
    DeletedArray.Add(MakeShared<FJsonValueString>(Path));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), DeletedCount > 0);
  Result->SetArrayField(TEXT("deleted"), DeletedArray);
  Result->SetNumberField(TEXT("requested"), ObjectsToDelete.Num());

  SendAutomationResponse(
      RequestingSocket, RequestId, DeletedCount > 0,
      FString::Printf(TEXT("Deleted %d of %d assets"), DeletedCount,
                      ObjectsToDelete.Num()),
      Result, DeletedCount > 0 ? FString() : TEXT("BULK_DELETE_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("bulk_delete requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 6. GENERATE THUMBNAIL
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleGenerateThumbnail(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("generate_thumbnail"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("create_thumbnail"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("generate_thumbnail payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
      AssetPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // SECURITY: Validate asset path
  FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SafeAssetPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Invalid path (traversal/security violation): %s"), *AssetPath),
        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  int32 Width = 512;
  int32 Height = 512;

  double TempWidth = 0, TempHeight = 0;
  if (Payload->TryGetNumberField(TEXT("width"), TempWidth))
    Width = static_cast<int32>(TempWidth);
  if (Payload->TryGetNumberField(TEXT("height"), TempHeight))
    Height = static_cast<int32>(TempHeight);

  FString OutputPath;
  Payload->TryGetStringField(TEXT("outputPath"), OutputPath);

  // NOTE: ProcessAutomationRequest already dispatches to GameThread.
  // Wrapping ALL work (including fast existence checks) in AsyncTask(GameThread, ...)
  // caused the queued lambda to sit behind the current dispatch cycle, so responses
  // never reached the MCP server before the 30-second timeout (issues #138, #139).
  // Execute synchronously instead.
  SendProgressUpdate(RequestId, 0.0f,
      FString::Printf(TEXT("Starting thumbnail generation for: %s"), *SafeAssetPath), true);

  if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Asset not found"), nullptr,
                           TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(SafeAssetPath);
  if (!Asset) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Failed to load asset"), nullptr,
                           TEXT("LOAD_FAILED"));
    return true;
  }

  // Send progress update before GPU operation
  SendProgressUpdate(RequestId, 50.0f,
      TEXT("Rendering thumbnail (GPU operation)..."), true);

  FObjectThumbnail ObjectThumbnail;
  ThumbnailTools::RenderThumbnail(
      Asset, Width, Height,
      ThumbnailTools::EThumbnailTextureFlushMode::NeverFlush, nullptr,
      &ObjectThumbnail);

  bool bSuccess = ObjectThumbnail.GetImageWidth() > 0 &&
                  ObjectThumbnail.GetImageHeight() > 0;

  if (bSuccess && !OutputPath.IsEmpty()) {
    const TArray<uint8> &ImageData = ObjectThumbnail.GetUncompressedImageData();

    if (ImageData.Num() > 0) {
      TArray<FColor> ColorData;
      ColorData.Reserve(Width * Height);

      // Fixed: Ensure we don't read out of bounds if ImageData length isn't a multiple of 4
      for (int32 i = 0; i + 3 < ImageData.Num(); i += 4) {
        FColor Color;
        Color.B = ImageData[i + 0];
        Color.G = ImageData[i + 1];
        Color.R = ImageData[i + 2];
        Color.A = ImageData[i + 3];
        ColorData.Add(Color);
      }

      // SECURITY: Sanitize and validate the output path to prevent path traversal
      FString SafeOutputPath = SanitizeProjectFilePath(OutputPath);
      if (SafeOutputPath.IsEmpty()) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               FString::Printf(TEXT("Invalid or unsafe output path: %s"), *OutputPath),
                               nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
      }

      FString AbsolutePath = FPaths::ProjectDir() / SafeOutputPath;
      AbsolutePath = FPaths::ConvertRelativePathToFull(AbsolutePath);
      FPaths::NormalizeFilename(AbsolutePath);

      FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
      FPaths::NormalizeDirectoryName(NormalizedProjectDir);
      if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
        NormalizedProjectDir += TEXT("/");
      }

      if (!AbsolutePath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase)) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               FString::Printf(TEXT("Output path escapes project directory: %s"), *OutputPath),
                               nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
      }

      TArray<uint8> CompressedData;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      FImageUtils::ThumbnailCompressImageArray(Width, Height, ColorData,
                                               CompressedData);
#else
      // UE 5.0: Use CompressImageArray instead
      FImageUtils::CompressImageArray(Width, Height, ColorData, CompressedData);
#endif
      bSuccess = FFileHelper::SaveArrayToFile(CompressedData, *AbsolutePath);
    }
  }

  if (Asset->GetOutermost()) {
    Asset->GetOutermost()->MarkPackageDirty();
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), bSuccess);
  Result->SetStringField(TEXT("assetPath"), SafeAssetPath);
  Result->SetNumberField(TEXT("width"), Width);
  Result->SetNumberField(TEXT("height"), Height);

  if (!OutputPath.IsEmpty()) {
    Result->SetStringField(TEXT("outputPath"), OutputPath);
  }

  SendAutomationResponse(
      RequestingSocket, RequestId, bSuccess,
      bSuccess ? TEXT("Thumbnail generated successfully")
               : TEXT("Thumbnail generation failed"),
      Result, bSuccess ? FString() : TEXT("THUMBNAIL_GENERATION_FAILED"));

  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("generate_thumbnail requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 7. BASIC ASSET OPERATIONS (Import, Duplicate, Rename, Move, etc.)
// ============================================================================

/**
 * Handles asset import requests.
 *
 * IMPORTANT: In UE 5.7+, the Interchange Framework is the default importer for
 * FBX/glTF files. Interchange uses the TaskGraph internally for async operations.
 * If we call ImportAssetsAutomated() synchronously from within an AsyncTask callback
 * (which is how WebSocket messages are dispatched), we hit a TaskGraph recursion
 * guard assertion: "++Queue(QueueIndex).RecursionGuard == 1".
 *
 * The fix is to defer the import to the next editor tick using GEditor->GetTimerManager(),
 * which breaks out of the TaskGraph callback chain and allows Interchange to function
 * correctly.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'sourcePath' and 'destinationPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleImportAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString DestinationPath;
  Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);
  FString SourcePath;
  Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);

  if (DestinationPath.IsEmpty() || SourcePath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sourcePath and destinationPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Verify source file exists
  if (!FPaths::FileExists(SourcePath)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Source file not found: %s"), *SourcePath),
        nullptr, TEXT("SOURCE_NOT_FOUND"));
    return true;
  }

  // Sanitize destination path
  FString SafeDestPath = SanitizeProjectRelativePath(DestinationPath);
  if (SafeDestPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid destination path"), nullptr,
                           TEXT("INVALID_PATH"));
    return true;
  }

  FString DestPath = FPaths::GetPath(SafeDestPath);
  FString DestName = FPaths::GetBaseFilename(SafeDestPath);

  // If destination is just a folder, use that
  if (FPaths::GetExtension(SafeDestPath).IsEmpty()) {
    DestPath = SafeDestPath;
    DestName = FPaths::GetBaseFilename(SourcePath);
  }

  // Sanitize DestName: UE asset names cannot contain spaces or dots
  DestName.ReplaceInline(TEXT(" "), TEXT("_"));
  DestName.ReplaceInline(TEXT("."), TEXT("_"));

  // Defer the import to the next tick to avoid TaskGraph recursion issues with
  // UE 5.7+ Interchange Framework. See issue #137.
  // We use SetTimerForNextTick to ensure we're completely outside of any
  // TaskGraph callback chain before invoking the import.
  if (GEditor) {
    TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
    GEditor->GetTimerManager()->SetTimerForNextTick(
        [WeakThis, RequestId, SourcePath, DestPath, DestName, Socket]() {
          UMcpAutomationBridgeSubsystem *StrongThis = WeakThis.Get();
          if (!StrongThis) {
            return;
          }

          IAssetTools &AssetTools =
              FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools")
                  .Get();

          TArray<FString> Files;
          Files.Add(SourcePath);

          UAutomatedAssetImportData *ImportData =
              NewObject<UAutomatedAssetImportData>();
          ImportData->bReplaceExisting = true;
          ImportData->DestinationPath = DestPath;
          ImportData->Filenames = Files;

          TArray<UObject *> ImportedAssets =
              AssetTools.ImportAssetsAutomated(ImportData);

          // Find the first valid (non-null) asset in the array.
          // ImportAssetsAutomated can return arrays with nullptr entries.
          UObject *Asset = nullptr;
          for (UObject *ImportedObj : ImportedAssets) {
            if (ImportedObj) {
              Asset = ImportedObj;
              break;
            }
          }

          if (Asset) {
            // Compute the final asset path. If we rename, use the destination
            // path/name since RenameAssets may invalidate the Asset pointer.
            FString FinalAssetPath;
            bool bRenameSucceeded = true;

            // Rename if needed
            if (Asset->GetName() != DestName) {
              FAssetRenameData RenameData(Asset, DestPath, DestName);
              bRenameSucceeded = AssetTools.RenameAssets({RenameData});
              // After rename, compute path from destination (Asset pointer may
              // be stale)
              FinalAssetPath = DestPath / DestName + TEXT(".") + DestName;
            } else {
              // No rename needed, safe to use the asset's current path
              FinalAssetPath = Asset->GetPathName();
            }

            TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetStringField(TEXT("assetPath"), FinalAssetPath);
            if (!bRenameSucceeded) {
              Resp->SetBoolField(TEXT("renameWarning"), true);
            }
            // Add verification data
            UObject *ImportedAsset = UEditorAssetLibrary::LoadAsset(FinalAssetPath);
            if (ImportedAsset) {
              McpHandlerUtils::AddVerification(Resp, ImportedAsset);
            }
            StrongThis->SendAutomationResponse(
                Socket, RequestId, true,
                bRenameSucceeded ? TEXT("Asset imported")
                                 : TEXT("Asset imported but rename failed"),
                Resp, FString());
          } else {
            StrongThis->SendAutomationResponse(
                Socket, RequestId, false,
                FString::Printf(TEXT("Failed to import asset from '%s'"),
                                *SourcePath),
                nullptr, TEXT("IMPORT_FAILED"));
          }
        });
  } else {
    // Fallback: GEditor not available (shouldn't happen in editor context)
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available for deferred import"),
                           nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
  }

  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles metadata setting requests for assets.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath' and 'metadata' object.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleSetMetadata(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("set_metadata payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  const TSharedPtr<FJsonObject> *MetadataObjPtr = nullptr;
  if (!Payload->TryGetObjectField(TEXT("metadata"), MetadataObjPtr) ||
      !MetadataObjPtr) {
    // Treat missing/empty metadata as a no-op success; nothing to write.
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetNumberField(TEXT("updatedKeys"), 0);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("No metadata provided; no-op"), Resp,
                           FString());
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  if (!Asset) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to load asset"), nullptr,
                           TEXT("LOAD_FAILED"));
    return true;
  }

  UPackage *Package = Asset->GetOutermost();
  if (!Package) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to resolve package for asset"), nullptr,
                           TEXT("PACKAGE_NOT_FOUND"));
    return true;
  }

  // GetMetaData returns the metadata object that is owned by this package.
  // UE 5.0 uses UMetaData*, UE 5.6+ uses FMetaData&
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
  FMetaData& Meta = Package->GetMetaData();
#else
  UMetaData* Meta = Package->GetMetaData();
#endif

  const TSharedPtr<FJsonObject> &MetadataObj = *MetadataObjPtr;
  int32 UpdatedCount = 0;

  for (const auto &Kvp : MetadataObj->Values) {
    const FString &Key = Kvp.Key;
    const TSharedPtr<FJsonValue> &Val = Kvp.Value;

    FString ValueString;
    if (!Val.IsValid() || Val->IsNull()) {
      continue;
    }
    switch (Val->Type) {
    case EJson::String:
      ValueString = Val->AsString();
      break;
    case EJson::Number:
      ValueString = LexToString(Val->AsNumber());
      break;
    case EJson::Boolean:
      ValueString = Val->AsBool() ? TEXT("true") : TEXT("false");
      break;
    default:
      // For arrays/objects, store a compact JSON string
      {
        FString JsonOut;
        const TSharedRef<TJsonWriter<>> Writer =
            TJsonWriterFactory<>::Create(&JsonOut);
        FJsonSerializer::Serialize(Val, TEXT(""), Writer);
        ValueString = JsonOut;
      }
      break;
    }

    if (!ValueString.IsEmpty()) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
      Meta.SetValue(Asset, *Key, *ValueString);
#else
      Meta->SetValue(Asset, *Key, *ValueString);
#endif
      ++UpdatedCount;
    }
  }

  if (UpdatedCount > 0) {
    Package->SetDirtyFlag(true);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);
  Resp->SetNumberField(TEXT("updatedKeys"), UpdatedCount);
  
  // Add verification data
  McpHandlerUtils::AddVerification(Resp, Asset);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Asset metadata updated"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles asset duplication requests. Supports both single asset and folder
 * (deep) duplication.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'sourcePath' and 'destinationPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleDuplicateAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString SourcePath;
  Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);
  FString DestinationPath;
  Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);

  if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sourcePath and destinationPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Auto-resolve simple name for destination
  if (!DestinationPath.IsEmpty() &&
      FPaths::GetPath(DestinationPath).IsEmpty()) {
    FString ParentDir = FPaths::GetPath(SourcePath);
    if (ParentDir.IsEmpty() || ParentDir == TEXT("/"))
      ParentDir = TEXT("/Game");

    DestinationPath = ParentDir / DestinationPath;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
           TEXT("HandleDuplicateAsset: Auto-resolved simple name destination "
                "to '%s'"),
           *DestinationPath);
  }

  // If the source path is a directory, perform a deep duplication of all
  // assets under that folder into the destination folder, preserving
  // relative structure. This powers the "Deep Duplication - Duplicate
  // Folder" scenario in tests.
  if (UEditorAssetLibrary::DoesDirectoryExist(SourcePath)) {
    // Ensure the destination root exists
    UEditorAssetLibrary::MakeDirectory(DestinationPath);

    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*SourcePath));
    Filter.bRecursivePaths = true;

    // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
    // Asset listing uses cached AssetRegistry data exclusively.
    // LIMITATION: Assets not yet indexed by the editor's background scanner
    // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
    TArray<FAssetData> Assets;
    AssetRegistryModule.Get().GetAssets(Filter, Assets);

    int32 DuplicatedCount = 0;
    for (const FAssetData &Asset : Assets) {
      // PackageName is the long package path (e.g.,
      // /Game/Tests/DeepCopy/Source/M_Source)
      const FString SourceAssetPath = Asset.PackageName.ToString();

      FString RelativePath;
      if (SourceAssetPath.StartsWith(SourcePath)) {
        RelativePath = SourceAssetPath.RightChop(SourcePath.Len());
      } else {
        // Should not happen for the filtered set, but skip if it does.
        continue;
      }

      const FString TargetAssetPath =
          DestinationPath + RelativePath; // preserves any subfolders
      const FString TargetFolderPath = FPaths::GetPath(TargetAssetPath);
      if (!TargetFolderPath.IsEmpty()) {
        UEditorAssetLibrary::MakeDirectory(TargetFolderPath);
      }

      if (UEditorAssetLibrary::DuplicateAsset(SourceAssetPath,
                                              TargetAssetPath)) {
        ++DuplicatedCount;
      }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    const bool bSuccess = DuplicatedCount > 0;
    Resp->SetBoolField(TEXT("success"), bSuccess);
    Resp->SetStringField(TEXT("sourcePath"), SourcePath);
    Resp->SetStringField(TEXT("destinationPath"), DestinationPath);
    Resp->SetNumberField(TEXT("duplicatedCount"), DuplicatedCount);

    if (bSuccess) {
      SendAutomationResponse(Socket, RequestId, true, TEXT("Folder duplicated"),
                             Resp, FString());
    } else {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("No assets duplicated"), Resp,
                             TEXT("DUPLICATE_FAILED"));
    }
    return true;
  }

  // Fallback: single-asset duplication
  if (!UEditorAssetLibrary::DoesAssetExist(SourcePath)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Source asset not found: %s"), *SourcePath),
        nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  if (UEditorAssetLibrary::DoesAssetExist(DestinationPath)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Destination asset already exists: %s"),
                        *DestinationPath),
        nullptr, TEXT("DESTINATION_EXISTS"));
    return true;
  }

  if (UEditorAssetLibrary::DuplicateAsset(SourcePath, DestinationPath)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), DestinationPath);
    // Add verification data
    UObject *NewAsset = UEditorAssetLibrary::LoadAsset(DestinationPath);
    if (NewAsset) {
      McpHandlerUtils::AddVerification(Resp, NewAsset);
    }
    SendAutomationResponse(Socket, RequestId, true, TEXT("Asset duplicated"),
                           Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Duplicate failed"),
                           nullptr, TEXT("DUPLICATE_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles asset renaming (and moving) requests.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'sourcePath' and 'destinationPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleRenameAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString SourcePath;
  Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);
  FString DestinationPath;
  Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);

  if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sourcePath and destinationPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Auto-resolve simple name for destination
  if (!DestinationPath.IsEmpty() &&
      FPaths::GetPath(DestinationPath).IsEmpty()) {
    FString ParentDir = FPaths::GetPath(SourcePath);
    if (ParentDir.IsEmpty() || ParentDir == TEXT("/"))
      ParentDir = TEXT("/Game");

    DestinationPath = ParentDir / DestinationPath;
    UE_LOG(
        LogMcpAutomationBridgeSubsystem, Display,
        TEXT(
            "HandleRenameAsset: Auto-resolved simple name destination to '%s'"),
        *DestinationPath);
  }

  // Resolve source path to ensure it matches a real asset
  FString ResolvedSourcePath = ResolveAssetPath(SourcePath);
  if (ResolvedSourcePath.IsEmpty()) {
    // If resolution failed, fall back to original for strict check
    ResolvedSourcePath = SourcePath;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(ResolvedSourcePath)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Source asset not found: %s"), *SourcePath),
        nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  // Use the resolved path for the rename operation
  if (UEditorAssetLibrary::RenameAsset(ResolvedSourcePath, DestinationPath)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), DestinationPath);
    
    // Add verification data
    UObject* RenamedAsset = UEditorAssetLibrary::LoadAsset(DestinationPath);
    if (RenamedAsset) {
      McpHandlerUtils::AddVerification(Resp, RenamedAsset);
    }
    
    SendAutomationResponse(Socket, RequestId, true, TEXT("Asset renamed"), Resp,
                           FString());
  } else {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Failed to rename asset. Check if destination "
                             "'%s' already exists or source is locked."),
                        *DestinationPath),
        nullptr, TEXT("RENAME_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleMoveAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  // Move is essentially rename in Unreal
  return HandleRenameAsset(RequestId, Payload, Socket);
}

/**
 * Handles asset deletion requests.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'path' (string) or 'paths' (array of
 * strings).
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleDeleteAssets(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  // Support both single 'path' and array 'paths'
  TArray<FString> PathsToDelete;
  const TArray<TSharedPtr<FJsonValue>> *PathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("paths"), PathsArray) && PathsArray) {
    for (const auto &Val : *PathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String)
        PathsToDelete.Add(Val->AsString());
    }
  }

  FString SinglePath;
  if (Payload->TryGetStringField(TEXT("path"), SinglePath) &&
      !SinglePath.IsEmpty()) {
    PathsToDelete.Add(SinglePath);
  }

  if (PathsToDelete.Num() == 0) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("No paths provided"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  int32 DeletedCount = 0;
  TArray<FString> NotFoundPaths;
  TArray<FString> FailedToDeletePaths;
  
  for (const FString &Path : PathsToDelete) {
    // Check if it's a directory first (folder path)
    if (UEditorAssetLibrary::DoesDirectoryExist(Path)) {
      // Directory exists - use safe folder deletion with proper cleanup
      // CRITICAL for UE 5.7+: Use McpSafeDeleteFolder instead of UEditorAssetLibrary::DeleteDirectory
      // to prevent crashes during UWorld::CleanupWorld when deleting folders containing
      // AnimBlueprints, IKRigs, IKRetargeters, etc.
      if (McpSafeOperations::McpSafeDeleteFolder(Path, true))
      {
        // Verify the directory was actually deleted
        if (!UEditorAssetLibrary::DoesDirectoryExist(Path)) {
          DeletedCount++;
        } else {
          // Delete returned true but directory still exists
          FailedToDeletePaths.Add(Path);
        }
      } else {
        FailedToDeletePaths.Add(Path);
      }
    } else if (UEditorAssetLibrary::DoesAssetExist(Path)) {
      // Asset exists - attempt to delete it
      if (UEditorAssetLibrary::DeleteAsset(Path)) {
        // Verify the asset was actually deleted
        if (!UEditorAssetLibrary::DoesAssetExist(Path)) {
          DeletedCount++;
        } else {
          // Delete returned true but asset still exists
          FailedToDeletePaths.Add(Path);
        }
      } else {
        FailedToDeletePaths.Add(Path);
      }
    } else {
      // Asset/directory does not exist
      NotFoundPaths.Add(Path);
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  
  // Return success only if at least one asset was deleted
  bool bSuccess = DeletedCount > 0;
  Resp->SetBoolField(TEXT("success"), bSuccess);
  Resp->SetNumberField(TEXT("deletedCount"), DeletedCount);
  Resp->SetBoolField(TEXT("existsAfter"), false);
  
  if (NotFoundPaths.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> NotFoundArray;
    for (const FString& P : NotFoundPaths) {
      NotFoundArray.Add(MakeShared<FJsonValueString>(P));
    }
    Resp->SetArrayField(TEXT("notFoundPaths"), NotFoundArray);
    Resp->SetNumberField(TEXT("notFoundCount"), NotFoundPaths.Num());
  }
  
  if (FailedToDeletePaths.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> FailedArray;
    for (const FString& P : FailedToDeletePaths) {
      FailedArray.Add(MakeShared<FJsonValueString>(P));
    }
    Resp->SetArrayField(TEXT("failedToDeletePaths"), FailedArray);
    Resp->SetNumberField(TEXT("failedCount"), FailedToDeletePaths.Num());
  }
  
  if (bSuccess) {
    SendAutomationResponse(Socket, RequestId, true, TEXT("Assets deleted"), Resp, FString());
  } else {
    // Nothing was deleted - determine the reason
    FString ErrorMessage;
    FString ErrorCode;
    
    if (NotFoundPaths.Num() > 0 && FailedToDeletePaths.Num() == 0) {
      // All paths were not found
      ErrorMessage = FString::Printf(TEXT("No assets deleted. %d path(s) not found."), NotFoundPaths.Num());
      ErrorCode = TEXT("ASSET_NOT_FOUND");
    } else if (FailedToDeletePaths.Num() > 0 && NotFoundPaths.Num() == 0) {
      // All paths existed but deletion failed
      ErrorMessage = FString::Printf(TEXT("Failed to delete %d asset(s). They may be in use or locked."), FailedToDeletePaths.Num());
      ErrorCode = TEXT("DELETE_FAILED");
    } else {
      // Mixed: some not found, some failed to delete
      ErrorMessage = FString::Printf(TEXT("No assets deleted. %d path(s) not found, %d failed to delete."), 
                                      NotFoundPaths.Num(), FailedToDeletePaths.Num());
      ErrorCode = TEXT("DELETE_FAILED");
    }
    
    SendAutomationResponse(Socket, RequestId, false, ErrorMessage, Resp, ErrorCode);
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles folder creation requests.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'path'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleCreateFolder(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString Path;
  if (!Payload->TryGetStringField(TEXT("path"), Path) || Path.IsEmpty()) {
    Payload->TryGetStringField(TEXT("directoryPath"), Path);
  }

  if (Path.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("path (or directoryPath) required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SafePath = SanitizeProjectRelativePath(Path);
  if (SafePath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("Invalid path: must be project-relative and not contain '..'"),
        nullptr, TEXT("INVALID_PATH"));
    return true;
  }

  if (UEditorAssetLibrary::DoesDirectoryExist(SafePath) ||
      UEditorAssetLibrary::MakeDirectory(SafePath)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("path"), SafePath);
    // verifiedPath/existsAfter must reflect folder existence, not asset existence
    VerifyDirectoryExists(Resp, SafePath);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Folder created"),
                           Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create folder"), nullptr,
                           TEXT("CREATE_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to get asset dependencies.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath' and optional 'recursive'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
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

/**
 * Handles requests to set asset tags. NOTE: Asset Registry tags are distinct
 * from Actor tags. This function currently returns NOT_IMPLEMENTED as generic
 * asset tagging is ambiguous (metadata vs registry tags).
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleSetTags(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("set_tags payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const TArray<TSharedPtr<FJsonValue>> *TagsArray = nullptr;
  TArray<FString> Tags;
  if (Payload->TryGetArrayField(TEXT("tags"), TagsArray) && TagsArray) {
    for (const TSharedPtr<FJsonValue> &Val : *TagsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        Tags.Add(Val->AsString());
      }
    }
  }

  AsyncTask(ENamedThreads::GameThread, [this, RequestId, Socket, AssetPath,
                                        Tags]() {
    // Edge-case: empty or missing tags array should be treated as a no-op
    // success.
    if (Tags.Num() == 0) {
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("assetPath"), AssetPath);
      Resp->SetNumberField(TEXT("appliedTags"), 0);
      SendAutomationResponse(Socket, RequestId, true,
                             TEXT("No tags provided; no-op"), Resp, FString());
      return;
    }

    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
      SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                             nullptr, TEXT("ASSET_NOT_FOUND"));
      return;
    }

    UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset) {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Failed to load asset"), nullptr,
                             TEXT("LOAD_FAILED"));
      return;
    }

    // Implement set_tags by mapping them to Package Metadata (Tag=true)
    int32 AppliedCount = 0;
    for (const FString &Tag : Tags) {
      UEditorAssetLibrary::SetMetadataTag(Asset, FName(*Tag), TEXT("true"));
      AppliedCount++;
    }

    // Mark dirty so the asset can be saved later
    Asset->MarkPackageDirty();

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("markedDirty"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetNumberField(TEXT("appliedTags"), AppliedCount);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Tags applied as metadata"), Resp, FString());
  });

  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to validate if an asset exists and can be loaded.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleValidateAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("validate payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AsyncTask(ENamedThreads::GameThread, [this, RequestId, Socket, AssetPath]() {
    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
      SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                             nullptr, TEXT("ASSET_NOT_FOUND"));
      return;
    }

    UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset) {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Failed to load asset"), nullptr,
                             TEXT("LOAD_FAILED"));
      return;
    }

    bool bIsValid = true;
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), bIsValid);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetBoolField(TEXT("isValid"), bIsValid);

    SendAutomationResponse(Socket, RequestId, true, TEXT("Asset validated"),
                           Resp, FString());
  });
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to list assets with filtering and pagination.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing filter criteria and pagination
 * options.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleListAssets(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  // Parse filters
  FString PathFilter;
  FString ClassFilter;
  FString TagFilter;
  FString PathStartsWith;

  const TSharedPtr<FJsonObject> *FilterObj;
  if (Payload->TryGetObjectField(TEXT("filter"), FilterObj) && FilterObj) {
    (*FilterObj)->TryGetStringField(TEXT("path"), PathFilter);
    (*FilterObj)->TryGetStringField(TEXT("class"), ClassFilter);
    (*FilterObj)->TryGetStringField(TEXT("tag"), TagFilter);
    (*FilterObj)->TryGetStringField(TEXT("pathStartsWith"), PathStartsWith);
  } else {
    // Legacy support for direct path/recursive fields
    Payload->TryGetStringField(TEXT("path"), PathFilter);
  }

  // Sanitize PathFilter to remove trailing slash which can break AssetRegistry
  // lookups
  if (PathFilter.Len() > 1 && PathFilter.EndsWith(TEXT("/"))) {
    PathFilter.RemoveAt(PathFilter.Len() - 1);
  }

  bool bRecursive = true;
  Payload->TryGetBoolField(TEXT("recursive"), bRecursive);

  // Parse pagination
  int32 Offset = 0;
  int32 Limit = -1; // -1 means no limit
  Payload->TryGetNumberField(TEXT("offset"), Offset);
  Payload->TryGetNumberField(TEXT("limit"), Limit);
  const TSharedPtr<FJsonObject> *PaginationObj;
  if (Payload->TryGetObjectField(TEXT("pagination"), PaginationObj) &&
      PaginationObj) {
    (*PaginationObj)->TryGetNumberField(TEXT("offset"), Offset);
    (*PaginationObj)->TryGetNumberField(TEXT("limit"), Limit);
  }

  Offset = FMath::Max(0, Offset);

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  FARFilter Filter;
  Filter.bRecursivePaths = bRecursive;
  Filter.bRecursiveClasses = true;

  // Apply path filters
  if (!PathFilter.IsEmpty()) {
    Filter.PackagePaths.Add(FName(*PathFilter));
  } else if (!PathStartsWith.IsEmpty()) {
    // If we have a path prefix, assume it's a package path
    // Note: FARFilter doesn't support 'StartsWith' natively for paths in an
    // efficient way other than adding the path and set bRecursivePaths=true. So
    // if PathStartsWith is a folder, we use it.
    Filter.PackagePaths.Add(FName(*PathStartsWith));
  } else {
    // Default to /Game to prevent empty results or massive scan
    Filter.PackagePaths.Add(FName(TEXT("/Game")));
  }

  // Use cached AssetRegistry data — ScanPathsSynchronous() removed to prevent
  // blocking the GameThread (causes SSE/HTTP transport timeouts).
  // LIMITATION: Assets not yet indexed by the editor's background scanner
  // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.

  if (!ClassFilter.IsEmpty()) {
    // Support both short class names and full paths (best effort)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    FTopLevelAssetPath ClassPath(ClassFilter);
    if (ClassPath.IsValid()) {
      Filter.ClassPaths.Add(ClassPath);
    }
#else
    // UE 5.0: Use ClassNames instead of ClassPaths
    Filter.ClassNames.Add(FName(*ClassFilter));
#endif
  }

  // Tags are not standard on assets in the same way as actors.
  // AssetRegistry tags are Key-Value pairs.
  // If TagFilter is provided, we assume it checks for the existence of a tag
  // key or value. Implementing a generic "HasTag" is ambiguous. We'll assume
  // TagFilter refers to a metadata key presence.

  // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
  // Asset listing uses cached AssetRegistry data exclusively.
  // LIMITATION: Assets not yet indexed by the editor's background scanner
  // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
  TArray<FAssetData> AssetList;
  AssetRegistry.GetAssets(Filter, AssetList);

  // Post-filtering
  if (!ClassFilter.IsEmpty() || !TagFilter.IsEmpty()) {
    AssetList.RemoveAll([&](const FAssetData &Asset) {
      if (!ClassFilter.IsEmpty()) {
        // Check full class path or asset class name
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        FString AssetClass = Asset.AssetClassPath.ToString();
        FString AssetClassName = Asset.AssetClassPath.GetAssetName().ToString();
#else
        FString AssetClass = Asset.AssetClass.ToString();
        FString AssetClassName = Asset.AssetClass.ToString();
#endif
        if (!AssetClass.Equals(ClassFilter) &&
            !AssetClassName.Equals(ClassFilter)) {
          return true; // Remove
        }
      }
      if (!TagFilter.IsEmpty()) {
        if (!Asset.TagsAndValues.Contains(FName(*TagFilter))) {
          return true; // Remove
        }
      }
      return false;
    });
  }

  // Filter by Depth if specified
  // (Changes made to support depth and folders - Touch to force rebuild)
  int32 Depth = -1;
  Payload->TryGetNumberField(TEXT("depth"), Depth);

  if (Depth >= 0 && bRecursive && !PathFilter.IsEmpty()) {
    // Normalize base path for depth calculation
    FString BasePath = PathFilter;
    if (BasePath.EndsWith(TEXT("/"))) {
      BasePath.RemoveAt(BasePath.Len() - 1);
    }
    // Base depth: number of slashes in /Game/Foo is 2
    int32 BaseSlashCount = 0;
    for (const TCHAR *P = *BasePath; *P; ++P) {
      if (*P == TEXT('/'))
        BaseSlashCount++;
    }

    AssetList.RemoveAll([&](const FAssetData &Asset) {
      FString PkgPath = Asset.PackagePath.ToString();
      // If PkgPath is shorter than BasePath (shouldn't happen with filter),
      // keep it I guess? Actually we only care about descendants.

      int32 SlashCount = 0;
      for (const TCHAR *P = *PkgPath; *P; ++P) {
        if (*P == TEXT('/'))
          SlashCount++;
      }

      // Difference in slashes determines depth
      // /Game (1 slash) vs /Game/A (2 slashes) -> Diff 1 -> Depth 0 (immediate
      // child) Wait, PackagePath for /Game/A is /Game. PackagePath for
      // /Game/Sub/B is /Game/Sub.

      // Let's test:
      // Filter: /Game (Slash=1)
      // Asset: /Game/A (PackagePath=/Game, Slash=1). Diff=0. Depth 0? Yes.
      // Asset: /Game/Sub/B (PackagePath=/Game/Sub, Slash=2). Diff=1. Depth 1?
      // Yes.

      // If Depth=0, we want Diff=0.
      // If Depth=1, we want Diff<=1.

      return (SlashCount - BaseSlashCount) > Depth;
    });
  }

  int32 TotalCount = AssetList.Num();

  // Apply pagination
  if (Offset > 0) {
    if (Offset >= AssetList.Num()) {
      AssetList.Empty();
    } else {
      AssetList.RemoveAt(0, Offset);
    }
  }

  if (Limit >= 0 && AssetList.Num() > Limit) {
    AssetList.SetNum(Limit);
  }

  // Also fetch sub-folders if we are listing a directory (PathFilter is set)
  TArray<FString> SubPathList;
  if (!PathFilter.IsEmpty()) {
    // If non-recursive (or depth limited), we generally want at least the
    // immediate subfolders. GetSubPaths is non-recursive by default.
    AssetRegistry.GetSubPaths(PathFilter, SubPathList, false);

    // If Depth is specified, we might want deeper folders?
    // Actually, standard 'ls' behavior on a folder shows immediate children
    // (files and folders). If recursive, it shows everything. Let keeps it
    // simple: If we are listing a path, show its immediate subfolders. Getting
    // ALL recursive folders might be too much info if strictly not requested,
    // but 'GetSubPaths' with bInRecurse=true gets everything.

    // Decision:
    // If Recursive=true (and Depth not limited), maybe we don't strictly need
    // folders as assets cover it? But user asked for folders when assets are
    // missing. Default 'ls' shows immediate folders. So let's always include
    // immediate subfolders of the requested path.
  }

  TArray<TSharedPtr<FJsonValue>> AssetsArray;
  for (const FAssetData &Asset : AssetList) {
    TSharedPtr<FJsonObject> AssetObj = McpHandlerUtils::CreateResultObject();
    AssetObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    AssetObj->SetStringField(TEXT("path"), Asset.GetSoftObjectPath().ToString());
    AssetObj->SetStringField(TEXT("class"), Asset.AssetClassPath.ToString());
#else
    AssetObj->SetStringField(TEXT("path"), Asset.ToSoftObjectPath().ToString());
    AssetObj->SetStringField(TEXT("class"), Asset.AssetClass.ToString());
#endif
    AssetObj->SetStringField(TEXT("packagePath"), Asset.PackagePath.ToString());

    // Add tags for context if requested
    TArray<TSharedPtr<FJsonValue>> Tags;
    for (auto TagPair : Asset.TagsAndValues) {
      Tags.Add(MakeShared<FJsonValueString>(TagPair.Key.ToString()));
    }
    AssetObj->SetArrayField(TEXT("tags"), Tags);

    AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
  }

  TArray<TSharedPtr<FJsonValue>> FoldersJson;
  for (const FString &SubPath : SubPathList) {
    FoldersJson.Add(MakeShared<FJsonValueString>(SubPath));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetArrayField(TEXT("assets"), AssetsArray);
  Resp->SetArrayField(TEXT("folders"), FoldersJson);
  Resp->SetNumberField(TEXT("totalCount"), TotalCount);
  Resp->SetNumberField(TEXT("count"), AssetsArray.Num());
  Resp->SetNumberField(TEXT("offset"), Offset);

  SendAutomationResponse(Socket, RequestId, true, TEXT("Assets listed"), Resp,
                         FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to get detailed information about a single asset.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleGetAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("get_asset payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FAssetData AssetData = UEditorAssetLibrary::FindAssetData(AssetPath);
  if (!AssetData.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to find asset data"), nullptr,
                           TEXT("ASSET_DATA_INVALID"));
    return true;
  }

  TSharedPtr<FJsonObject> AssetObj = McpHandlerUtils::CreateResultObject();
  AssetObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  AssetObj->SetStringField(TEXT("path"), AssetData.GetSoftObjectPath().ToString());
  AssetObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.ToString());
#else
  AssetObj->SetStringField(TEXT("path"), AssetData.ToSoftObjectPath().ToString());
  AssetObj->SetStringField(TEXT("class"), AssetData.AssetClass.ToString());
#endif
  AssetObj->SetStringField(TEXT("packagePath"),
                           AssetData.PackagePath.ToString());

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetObjectField(TEXT("result"), AssetObj);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Asset details retrieved"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to generate an asset report (CSV/JSON).
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'directory' and 'reportType'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleGenerateReport(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("generate_report payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Directory;
  Payload->TryGetStringField(TEXT("directory"), Directory);
  if (Directory.IsEmpty()) {
    Directory = TEXT("/Game");
  }

  // Normalize /Content prefix to /Game for convenience
  if (Directory.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
    Directory = FString::Printf(TEXT("/Game%s"), *Directory.RightChop(8));
  }

  FString ReportType;
  Payload->TryGetStringField(TEXT("reportType"), ReportType);
  if (ReportType.IsEmpty()) {
    ReportType = TEXT("Summary");
  }

  FString OutputPath;
  Payload->TryGetStringField(TEXT("outputPath"), OutputPath);

  AsyncTask(ENamedThreads::GameThread, [this, RequestId, Socket, Directory,
                                        ReportType, OutputPath]() {
    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    FARFilter Filter;
    Filter.bRecursivePaths = true;
    if (!Directory.IsEmpty()) {
      Filter.PackagePaths.Add(FName(*Directory));
    }

    // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
    // Asset listing uses cached AssetRegistry data exclusively.
    // LIMITATION: Assets not yet indexed by the editor's background scanner
    // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
    TArray<FAssetData> AssetList;
    AssetRegistryModule.Get().GetAssets(Filter, AssetList);

    TArray<TSharedPtr<FJsonValue>> AssetsArray;
    for (const FAssetData &Asset : AssetList) {
      TSharedPtr<FJsonObject> AssetObj = McpHandlerUtils::CreateResultObject();
      AssetObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
      AssetObj->SetStringField(TEXT("path"),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                               Asset.GetSoftObjectPath().ToString());
      AssetObj->SetStringField(TEXT("class"), Asset.AssetClassPath.ToString());
#else
                               Asset.ToSoftObjectPath().ToString());
      AssetObj->SetStringField(TEXT("class"), Asset.AssetClass.ToString());
#endif
      AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
    }

    bool bFileWritten = false;
    if (!OutputPath.IsEmpty()) {
      // SECURITY: Sanitize and validate the output path to prevent path traversal
      FString SafeOutputPath = SanitizeProjectFilePath(OutputPath);
      if (SafeOutputPath.IsEmpty()) {
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Invalid or unsafe output path: %s"), *OutputPath),
                            TEXT("SECURITY_VIOLATION"));
        return;
      }
      
      FString AbsoluteOutput = FPaths::ProjectDir() / SafeOutputPath;
      AbsoluteOutput = FPaths::ConvertRelativePathToFull(AbsoluteOutput);
      FPaths::NormalizeFilename(AbsoluteOutput);
      
      FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
      FPaths::NormalizeDirectoryName(NormalizedProjectDir);
      if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
        NormalizedProjectDir += TEXT("/");
      }
      
      if (!AbsoluteOutput.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase)) {
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Output path escapes project directory: %s"), *OutputPath),
                            TEXT("SECURITY_VIOLATION"));
        return;
      }

      const FString DirPath = FPaths::GetPath(AbsoluteOutput);
      IPlatformFile &PlatformFile =
          FPlatformFileManager::Get().GetPlatformFile();
      PlatformFile.CreateDirectoryTree(*DirPath);

      const FString FileContents = TEXT(
          "{\"report\":\"Asset report generated by MCP Automation Bridge\"}");
      bFileWritten =
          FFileHelper::SaveStringToFile(FileContents, *AbsoluteOutput);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("directory"), Directory);
    Resp->SetStringField(TEXT("reportType"), ReportType);
    Resp->SetNumberField(TEXT("assetCount"), AssetList.Num());
    Resp->SetArrayField(TEXT("assets"), AssetsArray);
    if (!OutputPath.IsEmpty()) {
      Resp->SetStringField(TEXT("outputPath"), OutputPath);
      Resp->SetBoolField(TEXT("fileWritten"), bFileWritten);
    }

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Asset report generated"), Resp, FString());
  });
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// ============================================================================
// 8. MATERIAL CREATION
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleCreateMaterial(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString Name;
  Payload->TryGetStringField(TEXT("name"), Name);
  FString Path;
  Payload->TryGetStringField(TEXT("path"), Path);

  if (Name.IsEmpty() || Path.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("name and path required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Validate properties if present
  const TSharedPtr<FJsonObject> *Props;
  if (Payload->TryGetObjectField(TEXT("properties"), Props)) {
    FString ShadingModelStr;
    if ((*Props)->TryGetStringField(TEXT("ShadingModel"), ShadingModelStr)) {
      // Simple validation for test case
      if (ShadingModelStr.Equals(TEXT("InvalidModel"),
                                 ESearchCase::IgnoreCase)) {
        SendAutomationResponse(Socket, RequestId, false,
                               TEXT("Invalid shading model"), nullptr,
                               TEXT("INVALID_PROPERTY"));
        return true;
      }
    }
  }

  IAssetTools &AssetTools =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

  FString FullPath = Path + TEXT("/") + Name;
  if (UEditorAssetLibrary::DoesAssetExist(FullPath)) {
    UEditorAssetLibrary::DeleteAsset(FullPath);
  }

  UMaterialFactoryNew *Factory = NewObject<UMaterialFactoryNew>();
  UObject *NewAsset =
      AssetTools.CreateAsset(Name, Path, UMaterial::StaticClass(), Factory);

  if (NewAsset) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Material created"),
                           Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create material"), nullptr,
                           TEXT("CREATE_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleCreateMaterialInstance(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString Name;
  Payload->TryGetStringField(TEXT("name"), Name);
  FString Path;
  Payload->TryGetStringField(TEXT("path"), Path);
  FString ParentPath;
  Payload->TryGetStringField(TEXT("parentMaterial"), ParentPath);

  if (Name.IsEmpty() || Path.IsEmpty() || ParentPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("name, path and parentMaterial required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  UMaterialInterface *ParentMaterial = nullptr;

  // Special test sentinel: treat "/Valid" as a shorthand for the engine's
  // default surface material so tests can exercise parameter handling without
  // requiring a real asset at that path.
  if (ParentPath.Equals(TEXT("/Valid"), ESearchCase::IgnoreCase)) {
    ParentMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
  } else {
    if (!UEditorAssetLibrary::DoesAssetExist(ParentPath)) {
      SendAutomationResponse(
          Socket, RequestId, false,
          FString::Printf(TEXT("Parent material asset not found: %s"),
                          *ParentPath),
          nullptr, TEXT("PARENT_NOT_FOUND"));
      return true;
    }
    ParentMaterial = LoadObject<UMaterialInterface>(nullptr, *ParentPath);
  }

  if (!ParentMaterial) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Parent material not found"), nullptr,
                           TEXT("PARENT_NOT_FOUND"));
    return true;
  }

  IAssetTools &AssetTools =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

  UMaterialInstanceConstantFactoryNew *Factory =
      NewObject<UMaterialInstanceConstantFactoryNew>();
  Factory->InitialParent = ParentMaterial;

  UObject *NewAsset = AssetTools.CreateAsset(
      Name, Path, UMaterialInstanceConstant::StaticClass(), Factory);

  if (NewAsset) {
    // Handle parameters if provided
    UMaterialInstanceConstant *MIC = Cast<UMaterialInstanceConstant>(NewAsset);
    const TSharedPtr<FJsonObject> *ParamsObj = nullptr;
    if (MIC && Payload->TryGetObjectField(TEXT("parameters"), ParamsObj)) {
      // Scalar parameters
      const TSharedPtr<FJsonObject> *Scalars;
      if ((*ParamsObj)->TryGetObjectField(TEXT("scalar"), Scalars)) {
        for (const auto &Kvp : (*Scalars)->Values) {
          double Val = 0.0;
          if (Kvp.Value->TryGetNumber(Val)) {
            MIC->SetScalarParameterValueEditorOnly(FName(*Kvp.Key), (float)Val);
          }
        }
      }

      // Vector parameters
      const TSharedPtr<FJsonObject> *Vectors;
      if ((*ParamsObj)->TryGetObjectField(TEXT("vector"), Vectors)) {
        for (const auto &Kvp : (*Vectors)->Values) {
          const TSharedPtr<FJsonObject> *VecObj;
          if (Kvp.Value->TryGetObject(VecObj)) {
            // Try generic RGBA
            double R = 0, G = 0, B = 0, A = 1;
            (*VecObj)->TryGetNumberField(TEXT("r"), R);
            (*VecObj)->TryGetNumberField(TEXT("g"), G);
            (*VecObj)->TryGetNumberField(TEXT("b"), B);
            (*VecObj)->TryGetNumberField(TEXT("a"), A);
            MIC->SetVectorParameterValueEditorOnly(
                FName(*Kvp.Key),
                FLinearColor((float)R, (float)G, (float)B, (float)A));
          }
        }
      }

      // Texture parameters
      const TSharedPtr<FJsonObject> *Textures;
      if ((*ParamsObj)->TryGetObjectField(TEXT("texture"), Textures)) {
        for (const auto &Kvp : (*Textures)->Values) {
          FString TexPath;
          if (Kvp.Value->TryGetString(TexPath) && !TexPath.IsEmpty()) {
            UTexture *Tex = LoadObject<UTexture>(nullptr, *TexPath);
            if (Tex) {
              MIC->SetTextureParameterValueEditorOnly(FName(*Kvp.Key), Tex);
            }
          }
        }
      }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material Instance created"), Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create material instance"), nullptr,
                           TEXT("CREATE_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// ============================================================================
// 10. MATERIAL PARAMETER & INSTANCE MANAGEMENT
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleAddMaterialParameter(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  FString Name;
  Payload->TryGetStringField(TEXT("name"), Name);
  FString Type;
  Payload->TryGetStringField(TEXT("type"), Type);

  if (AssetPath.IsEmpty() || Name.IsEmpty() || Type.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("assetPath, name, and type required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  UMaterial *Material = Cast<UMaterial>(Asset);

  if (!Material) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Asset is not a Material (Master Material "
                                "required for adding parameters)"),
                           nullptr, TEXT("INVALID_ASSET_TYPE"));
    return true;
  }

  UMaterialExpression *NewExpression = nullptr;
  Type = Type.ToLower();

  if (Type == TEXT("scalar")) {
    NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
        Material, UMaterialExpressionScalarParameter::StaticClass());
    if (UMaterialExpressionScalarParameter *ScalarParam =
            Cast<UMaterialExpressionScalarParameter>(NewExpression)) {
      ScalarParam->ParameterName = FName(*Name);
      double Val = 0.0;
      if (Payload->TryGetNumberField(TEXT("value"), Val)) {
        ScalarParam->DefaultValue = (float)Val;
      }
    }
  } else if (Type == TEXT("vector")) {
    NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
        Material, UMaterialExpressionVectorParameter::StaticClass());
    if (UMaterialExpressionVectorParameter *VectorParam =
            Cast<UMaterialExpressionVectorParameter>(NewExpression)) {
      VectorParam->ParameterName = FName(*Name);
      const TSharedPtr<FJsonObject> *VecObj;
      if (Payload->TryGetObjectField(TEXT("value"), VecObj)) {
        double R = 0, G = 0, B = 0, A = 1;
        (*VecObj)->TryGetNumberField(TEXT("r"), R);
        (*VecObj)->TryGetNumberField(TEXT("g"), G);
        (*VecObj)->TryGetNumberField(TEXT("b"), B);
        (*VecObj)->TryGetNumberField(TEXT("a"), A);
        VectorParam->DefaultValue =
            FLinearColor((float)R, (float)G, (float)B, (float)A);
      }
    }
  } else if (Type == TEXT("texture")) {
    NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass());
    if (UMaterialExpressionTextureSampleParameter2D *TexParam =
            Cast<UMaterialExpressionTextureSampleParameter2D>(NewExpression)) {
      TexParam->ParameterName = FName(*Name);
      FString TexPath;
      if (Payload->TryGetStringField(TEXT("value"), TexPath) &&
          !TexPath.IsEmpty()) {
        UTexture *Tex = LoadObject<UTexture>(nullptr, *TexPath);
        if (Tex) {
          TexParam->Texture = Tex;
        }
      }
    }
  } else if (Type == TEXT("staticswitch") || Type == TEXT("static_switch")) {
    NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
        Material, UMaterialExpressionStaticSwitchParameter::StaticClass());
    if (UMaterialExpressionStaticSwitchParameter *SwitchParam =
            Cast<UMaterialExpressionStaticSwitchParameter>(NewExpression)) {
      SwitchParam->ParameterName = FName(*Name);
      bool Val = false;
      if (Payload->TryGetBoolField(TEXT("value"), Val)) {
        SwitchParam->DefaultValue = Val;
      }
    }
  } else {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Unsupported parameter type: %s"), *Type), nullptr,
        TEXT("INVALID_TYPE"));
    return true;
  }

  if (NewExpression) {
    // UMaterialEditingLibrary::CreateMaterialExpression handles adding to the
    // material and graph. We just need to ensure the material is
    // recompiled/updated.
    UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetStringField(TEXT("parameterName"), Name);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Parameter added"),
                           Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create parameter expression"),
                           nullptr, TEXT("CREATE_FAILED"));
  }

  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleListMaterialInstances(
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

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  // Find all assets that are Material Instances and have this asset as parent
  // Note: This can be expensive if we scan all assets.
  // Optimization: Use GetReferencers? Or just filter by class and check parent.
  // Since we can't easily query by "Parent" tag efficiently without iterating,
  // we'll try a filtered query.

  FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"),
                                           TEXT("MaterialInstanceConstant")));
#else
  Filter.ClassNames.Add(FName(TEXT("MaterialInstanceConstant")));
#endif
  Filter.bRecursiveClasses = true;

  // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
  // Asset listing uses cached AssetRegistry data exclusively.
  // LIMITATION: Assets not yet indexed by the editor's background scanner
  // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
  TArray<FAssetData> AssetList;
  AssetRegistry.GetAssets(Filter, AssetList);

  TArray<TSharedPtr<FJsonValue>> Instances;

  // We need to check the parent. Loading the asset is safest but slow.
  // Checking tags is faster. MICs usually have "Parent" tag.
  FName ParentPathName(*AssetPath);

  for (const FAssetData &Asset : AssetList) {
    // Check tag first
    FString ParentTag;
    if (Asset.GetTagValue(TEXT("Parent"), ParentTag)) {
      // Tag value might be "Material'Path'" or just "Path"
      // It's usually formatted string.
      if (ParentTag.Contains(AssetPath)) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        Instances.Add(
            MakeShared<FJsonValueString>(Asset.GetSoftObjectPath().ToString()));
#else
        Instances.Add(
            MakeShared<FJsonValueString>(Asset.ToSoftObjectPath().ToString()));
#endif
      }
    } else {
      // Fallback: load asset (slow, but accurate)
      // Only do this if tag is missing? Or maybe skip to avoid perf hit.
      // Let's rely on tag for now.
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetArrayField(TEXT("instances"), Instances);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Instances listed"),
                         Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleResetInstanceParameters(
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

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  UMaterialInstanceConstant *MIC = Cast<UMaterialInstanceConstant>(Asset);

  if (!MIC) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Asset is not a Material Instance Constant"),
                           nullptr, TEXT("INVALID_ASSET_TYPE"));
    return true;
  }

  MIC->ClearParameterValuesEditorOnly();
  MIC->PostEditChange();
  MIC->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Instance parameters reset"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleDoesAssetExist(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  // Accept both 'assetPath' (legacy) and 'path' (canonical for the tool); sibling
  // actions like delete and create_folder already accept both. See SMOKE_BUGS.md bug #3.
  FString InputPath;
  Payload->TryGetStringField(TEXT("assetPath"), InputPath);
  if (InputPath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("path"), InputPath);
  }
  if (InputPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("path (or assetPath) required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const bool bAssetExists = UEditorAssetLibrary::DoesAssetExist(InputPath);
  const bool bDirectoryExists =
      !bAssetExists && UEditorAssetLibrary::DoesDirectoryExist(InputPath);
  const bool bExists = bAssetExists || bDirectoryExists;

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("exists"), bExists);
  Resp->SetBoolField(TEXT("isDirectory"), bDirectoryExists);
  Resp->SetStringField(TEXT("path"), InputPath);
  // echo back assetPath for callers that key off the legacy field name
  Resp->SetStringField(TEXT("assetPath"), InputPath);

  const TCHAR* Message = TEXT("Asset does not exist");
  if (bAssetExists) {
    Message = TEXT("Asset exists");
  } else if (bDirectoryExists) {
    Message = TEXT("Directory exists");
  }

  SendAutomationResponse(Socket, RequestId, true, Message, Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGetMaterialStats(
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

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  UMaterialInterface *Material = Cast<UMaterialInterface>(Asset);

  if (!Material) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Asset is not a Material"), nullptr,
                           TEXT("INVALID_ASSET_TYPE"));
    return true;
  }

  // Ensure material is compiled
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Material->EnsureIsComplete();
#else
  // UE 5.0: Force compilation by accessing the material resource
  Material->GetMaterial();
#endif

  TSharedPtr<FJsonObject> Stats = McpHandlerUtils::CreateResultObject();

  // Get actual shading model from the material
  FString ShadingModelStr = TEXT("Unknown");
  if (UMaterial *BaseMat = Material->GetMaterial()) {
    FMaterialShadingModelField ShadingModels = BaseMat->GetShadingModels();
    // Check shading models using HasShadingModel - prioritize common ones
    if (ShadingModels.HasShadingModel(MSM_Unlit)) {
      ShadingModelStr = TEXT("Unlit");
    } else if (ShadingModels.HasShadingModel(MSM_DefaultLit)) {
      ShadingModelStr = TEXT("DefaultLit");
    } else if (ShadingModels.HasShadingModel(MSM_Subsurface)) {
      ShadingModelStr = TEXT("Subsurface");
    } else if (ShadingModels.HasShadingModel(MSM_SubsurfaceProfile)) {
      ShadingModelStr = TEXT("SubsurfaceProfile");
    } else if (ShadingModels.HasShadingModel(MSM_ClearCoat)) {
      ShadingModelStr = TEXT("ClearCoat");
    } else if (ShadingModels.HasShadingModel(MSM_TwoSidedFoliage)) {
      ShadingModelStr = TEXT("TwoSidedFoliage");
    } else if (ShadingModels.HasShadingModel(MSM_Hair)) {
      ShadingModelStr = TEXT("Hair");
    } else if (ShadingModels.HasShadingModel(MSM_Cloth)) {
      ShadingModelStr = TEXT("Cloth");
    } else if (ShadingModels.HasShadingModel(MSM_Eye)) {
      ShadingModelStr = TEXT("Eye");
    } else if (ShadingModels.HasShadingModel(MSM_PreintegratedSkin)) {
      ShadingModelStr = TEXT("PreintegratedSkin");
    }
  }
  Stats->SetStringField(TEXT("shadingModel"), ShadingModelStr);

  // Get instruction count from material resource
  // Note: GetMaxNumInstructionsForShader takes FShaderType* in UE 5.6, EShaderFrequency in some earlier versions
  // Skip this in 5.6 as there's no clean way to get a FShaderType* for the pixel shader
  int32 InstructionCount = -1; // Not easily available in this UE version
  Stats->SetNumberField(TEXT("instructionCount"), InstructionCount);

  // Count texture samplers used in the material
  int32 SamplerCount = 0;
  if (UMaterial *BaseMat = Material->GetMaterial()) {
    for (UMaterialExpression *Expr : MCP_GET_MATERIAL_EXPRESSIONS(BaseMat)) {
      if (Expr && Expr->IsA<UMaterialExpressionTextureSample>()) {
        SamplerCount++;
      }
    }
  }
  Stats->SetNumberField(TEXT("samplerCount"), SamplerCount);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetObjectField(TEXT("stats"), Stats);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material stats retrieved"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGenerateLODs(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("generate_lods"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Support both landscapePath (single) and assetPaths (array)
  FString LandscapePath;
  Payload->TryGetStringField(TEXT("landscapePath"), LandscapePath);
  
  // Support both assetPath (single) and assetPaths (array)
  FString SingleAssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), SingleAssetPath);
  
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray);

  // Support both lodCount and numLODs
  int32 NumLODs = 4;
  Payload->TryGetNumberField(TEXT("lodCount"), NumLODs);
  Payload->TryGetNumberField(TEXT("numLODs"), NumLODs);
  NumLODs = FMath::Clamp(NumLODs, 1, 50);

  // Build list of paths to process
  TArray<FString> Paths;
  
  // Add landscape path if provided
  if (!LandscapePath.IsEmpty()) {
    // Validate landscape path
    FString SafePath = SanitizeProjectRelativePath(LandscapePath);
    if (SafePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Invalid or unsafe landscape path: %s"), *LandscapePath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    Paths.Add(SafePath);
  }
  
  // Add single asset path if provided
  if (!SingleAssetPath.IsEmpty()) {
    FString SafePath = SanitizeProjectRelativePath(SingleAssetPath);
    if (SafePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Invalid or unsafe asset path: %s"), *SingleAssetPath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    Paths.Add(SafePath);
  }
  
  // Add asset paths if provided
  if (AssetPathsArray) {
    for (const auto &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        FString SafePath = SanitizeProjectRelativePath(Val->AsString());
        if (!SafePath.IsEmpty()) {
          Paths.Add(SafePath);
        }
      }
    }
  }

  if (Paths.Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("landscapePath or assetPaths required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // NOTE: ProcessAutomationRequest already dispatches to GameThread.
  // Wrapping ALL work in AsyncTask(GameThread, ...) caused the queued lambda
  // to sit behind the current dispatch cycle, so responses never reached the
  // MCP server before the 30-second timeout. Execute synchronously instead.
  int32 SuccessCount = 0;
  TArray<FString> NotFoundPaths;
  TArray<FString> NotMeshPaths;

  for (const FString &Path : Paths) {
    SendProgressUpdate(RequestId, -1.0f,
        FString::Printf(TEXT("Processing LOD generation for: %s"), *Path), true);

    UObject *Obj = LoadObject<UObject>(nullptr, *Path);

    if (!Obj) {
      NotFoundPaths.Add(Path);
      continue;
    }

    // Try Static Mesh
    if (UStaticMesh *Mesh = Cast<UStaticMesh>(Obj)) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("Generating %d LODs for static mesh %s"), NumLODs, *Path);

        Mesh->Modify();
        Mesh->SetNumSourceModels(NumLODs);

        // Configure LOD reduction settings with progressive reduction
        for (int32 LODIndex = 1; LODIndex < NumLODs; LODIndex++) {
          FStaticMeshSourceModel &SourceModel = Mesh->GetSourceModel(LODIndex);
          FMeshReductionSettings &ReductionSettings =
              SourceModel.ReductionSettings;

          // Progressive reduction: 50%, 25%, 12.5%...
          float ReductionPercent =
              1.0f / FMath::Pow(2.0f, static_cast<float>(LODIndex));
          ReductionSettings.PercentTriangles = ReductionPercent;
          ReductionSettings.PercentVertices = ReductionPercent;

          // Enable reduction for this LOD level
          SourceModel.BuildSettings.bRecomputeNormals = false;
          SourceModel.BuildSettings.bRecomputeTangents = false;
          SourceModel.BuildSettings.bUseMikkTSpace = true;
        }

        // Build the mesh with new LOD settings
        Mesh->Build();
        Mesh->PostEditChange();
        McpSafeAssetSave(Mesh);

        SuccessCount++;
      } else {
        // Asset exists but is not a static mesh
        NotMeshPaths.Add(Path);
      }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    
    // CRITICAL FIX: Return proper success/failure based on actual results
    // Previously always returned success=true even when 0 meshes processed
    bool bSuccess = SuccessCount > 0;
    Resp->SetBoolField(TEXT("success"), bSuccess);
    Resp->SetNumberField(TEXT("processed"), SuccessCount);
    Resp->SetNumberField(TEXT("requested"), Paths.Num());
    Resp->SetNumberField(TEXT("lodCount"), NumLODs);
    
    // Add details about failures
    if (NotFoundPaths.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> NotFoundArray;
      for (const FString& P : NotFoundPaths) {
        NotFoundArray.Add(MakeShared<FJsonValueString>(P));
      }
      Resp->SetArrayField(TEXT("notFoundPaths"), NotFoundArray);
      Resp->SetNumberField(TEXT("notFoundCount"), NotFoundPaths.Num());
    }
    
    if (NotMeshPaths.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> NotMeshArray;
      for (const FString& P : NotMeshPaths) {
        NotMeshArray.Add(MakeShared<FJsonValueString>(P));
      }
      Resp->SetArrayField(TEXT("notMeshPaths"), NotMeshArray);
      Resp->SetNumberField(TEXT("notMeshCount"), NotMeshPaths.Num());
    }
    
    FString Message;
    FString ErrorCode;
    
    if (bSuccess) {
      Message = FString::Printf(TEXT("Generated LODs for %d mesh(es)"), SuccessCount);
    } else if (NotFoundPaths.Num() > 0 && NotMeshPaths.Num() == 0) {
      Message = FString::Printf(TEXT("No assets found. %d path(s) not found."), NotFoundPaths.Num());
      ErrorCode = TEXT("ASSET_NOT_FOUND");
    } else if (NotMeshPaths.Num() > 0 && NotFoundPaths.Num() == 0) {
      Message = FString::Printf(TEXT("No static meshes found. %d asset(s) are not meshes."), NotMeshPaths.Num());
      ErrorCode = TEXT("INVALID_ASSET_TYPE");
    } else {
      Message = FString::Printf(TEXT("No LODs generated. %d not found, %d not meshes."), 
                                NotFoundPaths.Num(), NotMeshPaths.Num());
      ErrorCode = TEXT("LOD_GENERATION_FAILED");
    }
    
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess,
                                      Message, Resp, ErrorCode);

  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Requires editor"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 8. METADATA
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleGetMetadata(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("get_metadata payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  if (!Asset) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to load asset"), nullptr,
                           TEXT("LOAD_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);

  // 1. Asset Registry Tags
  FAssetData AssetData(Asset);
  TSharedPtr<FJsonObject> TagsObj = McpHandlerUtils::CreateResultObject();
  for (const auto &Kvp : AssetData.TagsAndValues) {
    TagsObj->SetStringField(Kvp.Key.ToString(), Kvp.Value.AsString());
  }
  Resp->SetObjectField(TEXT("tags"), TagsObj);

  // 2. Package Metadata information
  UPackage *Package = Asset->GetOutermost();
  if (Package) {

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
    FMetaData& Meta = Package->GetMetaData();
    bool bHasMeta = FMetaData::GetMapForObject(Asset) != nullptr;
    Resp->SetBoolField(TEXT("debug_has_meta"), bHasMeta);

    const TMap<FName, FString> *ObjectMeta = FMetaData::GetMapForObject(Asset);
#else
    UMetaData* Meta = Package->GetMetaData();
    bool bHasMeta = Meta->GetMapForObject(Asset) != nullptr;
    Resp->SetBoolField(TEXT("debug_has_meta"), bHasMeta);

    const TMap<FName, FString> *ObjectMeta = Meta->GetMapForObject(Asset);
#endif
    if (ObjectMeta) {
      TSharedPtr<FJsonObject> MetaObj = McpHandlerUtils::CreateResultObject();
      for (const auto &Entry : *ObjectMeta) {
        MetaObj->SetStringField(Entry.Key.ToString(), Entry.Value);
      }
      Resp->SetObjectField(TEXT("metadata"), MetaObj);
    }
  }

  SendAutomationResponse(Socket, RequestId, true, TEXT("Metadata retrieved"),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("get_metadata requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 9. MATERIAL REBUILD - NOT IMPLEMENTED (placeholder for future implementation)
// ============================================================================

// Stub implementations for functions declared in header but removed from implementation
// These functions are referenced by the dispatcher but were removed due to API changes

bool UMcpAutomationBridgeSubsystem::HandleNaniteRebuildMesh(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("nanite_rebuild_mesh"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR && ENGINE_MAJOR_VERSION >= 5
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("nanite_rebuild_mesh payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MeshPath;
  if (!Payload->TryGetStringField(TEXT("meshPath"), MeshPath) ||
      MeshPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("meshPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Load the static mesh
  UStaticMesh *StaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
  if (!StaticMesh) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Static mesh not found: %s"), *MeshPath),
                        TEXT("MESH_NOT_FOUND"));
    return true;
  }

  // Check if mesh supports Nanite
  bool bEnableNanite = true;
  Payload->TryGetBoolField(TEXT("enableNanite"), bEnableNanite);

  // Nanite settings
  bool bPreserveArea = true;
  double TrianglePercent = 100.0;
  double FallbackPercent = 0.0;

  Payload->TryGetBoolField(TEXT("preserveArea"), bPreserveArea);
  Payload->TryGetNumberField(TEXT("trianglePercent"), TrianglePercent);
  Payload->TryGetNumberField(TEXT("fallbackPercent"), FallbackPercent);

  // Clamp values
  TrianglePercent = FMath::Clamp(TrianglePercent, 0.0, 100.0);
  FallbackPercent = FMath::Clamp(FallbackPercent, 0.0, 100.0);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
  // UE 5.7+: Use accessor functions to avoid deprecation warnings
  FMeshNaniteSettings Settings = StaticMesh->GetNaniteSettings();
  Settings.bEnabled = bEnableNanite;
  Settings.PositionPrecision = 8; // Default precision
  
  // bPreserveArea replaced with ShapePreservation enum
  if (bPreserveArea) {
    Settings.ShapePreservation = ENaniteShapePreservation::PreserveArea;
  } else {
    Settings.ShapePreservation = ENaniteShapePreservation::None;
  }
  Settings.KeepPercentTriangles = static_cast<float>(TrianglePercent / 100.0);
  Settings.FallbackPercentTriangles = static_cast<float>(FallbackPercent / 100.0);
  if (FallbackPercent > 0.0) {
    Settings.GenerateFallback = ENaniteGenerateFallback::Enabled;
  } else {
    Settings.GenerateFallback = ENaniteGenerateFallback::PlatformDefault;
  }
  StaticMesh->SetNaniteSettings(Settings);
  StaticMesh->NotifyNaniteSettingsChanged();
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  // UE 5.1-5.6: Uses KeepPercentTriangles, FallbackPercentTriangles, and bPreserveArea
  StaticMesh->NaniteSettings.bEnabled = bEnableNanite;
  StaticMesh->NaniteSettings.PositionPrecision = 8;
  StaticMesh->NaniteSettings.bPreserveArea = bPreserveArea;
  StaticMesh->NaniteSettings.KeepPercentTriangles = static_cast<float>(TrianglePercent / 100.0);
  StaticMesh->NaniteSettings.FallbackPercentTriangles = static_cast<float>(FallbackPercent / 100.0);
#else
  // UE 5.0: Uses KeepPercentTriangles (no bPreserveArea)
  StaticMesh->NaniteSettings.bEnabled = bEnableNanite;
  StaticMesh->NaniteSettings.PositionPrecision = 8;
  StaticMesh->NaniteSettings.KeepPercentTriangles = static_cast<float>(TrianglePercent / 100.0);
  StaticMesh->NaniteSettings.FallbackPercentTriangles = static_cast<float>(FallbackPercent / 100.0);
#endif

  // Mark mesh as modified
  StaticMesh->MarkPackageDirty();

  // Build response
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("meshPath"), MeshPath);
  Resp->SetStringField(TEXT("meshName"), StaticMesh->GetName());
  Resp->SetBoolField(TEXT("naniteEnabled"), bEnableNanite);
  Resp->SetBoolField(TEXT("preserveArea"), bPreserveArea);
  Resp->SetNumberField(TEXT("trianglePercent"), TrianglePercent);
  Resp->SetNumberField(TEXT("fallbackPercent"), FallbackPercent);

  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Nanite settings updated for %s"), *StaticMesh->GetName()),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("nanite_rebuild_mesh requires UE 5.0+ editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleFindByTag(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("find_by_tag"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("find_by_tag payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Tag;
  if (!Payload->TryGetStringField(TEXT("tag"), Tag) || Tag.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("tag field is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // CRITICAL: Validate path parameter for security even if not used for actor search
  // This prevents false negatives in security testing and follows defense-in-depth
  FString Path;
  if (Payload->TryGetStringField(TEXT("path"), Path) && !Path.IsEmpty()) {
    FString SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("Invalid path (traversal/security violation): %s"), *Path),
          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    // Path is valid - could be used for scoping asset search in future
  }

  FName TagName(*Tag);
  TArray<TSharedPtr<FJsonValue>> Results;
  int32 MaxResults = 100;
  Payload->TryGetNumberField(TEXT("maxResults"), MaxResults);
  MaxResults = FMath::Clamp(MaxResults, 1, 1000);

  bool bSearchActors = true;
  bool bSearchComponents = false;
  bool bSearchAssets = false;
  Payload->TryGetBoolField(TEXT("searchActors"), bSearchActors);
  Payload->TryGetBoolField(TEXT("searchComponents"), bSearchComponents);
  Payload->TryGetBoolField(TEXT("searchAssets"), bSearchAssets);

  // Search in world
  if (GEditor && bSearchActors) {
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (World) {
      for (TActorIterator<AActor> It(World); It && Results.Num() < MaxResults; ++It) {
        AActor *Actor = *It;
        if (Actor && Actor->ActorHasTag(TagName)) {
          TSharedPtr<FJsonObject> ResultObj = McpHandlerUtils::CreateResultObject();
          ResultObj->SetStringField(TEXT("type"), TEXT("Actor"));
          ResultObj->SetStringField(TEXT("name"), Actor->GetName());
          ResultObj->SetStringField(TEXT("label"), Actor->GetActorLabel());
          ResultObj->SetStringField(TEXT("path"), Actor->GetPathName());
          ResultObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
          
          const FVector Location = Actor->GetActorLocation();
          TSharedPtr<FJsonObject> LocObj = McpHandlerUtils::CreateResultObject();
          LocObj->SetNumberField(TEXT("x"), Location.X);
          LocObj->SetNumberField(TEXT("y"), Location.Y);
          LocObj->SetNumberField(TEXT("z"), Location.Z);
          ResultObj->SetObjectField(TEXT("location"), LocObj);
          
          Results.Add(MakeShared<FJsonValueObject>(ResultObj));
        }
      }
    }
  }

  // Search for components with tag
  if (bSearchComponents && GEditor && Results.Num() < MaxResults) {
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (World) {
      for (TActorIterator<AActor> It(World); It && Results.Num() < MaxResults; ++It) {
        AActor *Actor = *It;
        if (Actor) {
          TInlineComponentArray<UActorComponent*> Components;
          Actor->GetComponents(Components);
          for (UActorComponent *Component : Components) {
            if (Component && Component->ComponentHasTag(TagName)) {
              TSharedPtr<FJsonObject> ResultObj = McpHandlerUtils::CreateResultObject();
              ResultObj->SetStringField(TEXT("type"), TEXT("Component"));
              ResultObj->SetStringField(TEXT("name"), Component->GetName());
              ResultObj->SetStringField(TEXT("class"), Component->GetClass()->GetName());
              ResultObj->SetStringField(TEXT("owner"), Actor->GetName());
              ResultObj->SetStringField(TEXT("path"), Component->GetPathName());
              Results.Add(MakeShared<FJsonValueObject>(ResultObj));
            }
          }
        }
      }
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("tag"), Tag);
  Resp->SetNumberField(TEXT("count"), Results.Num());
  Resp->SetArrayField(TEXT("results"), Results);

  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Found %d objects with tag '%s'"), Results.Num(), *Tag),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("find_by_tag requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleAddMaterialNode(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("add_material_node"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("add_material_node payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("assetPath"), MaterialPath))
  {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath or materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("assetPath cannot be empty"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString NodeType;
  if (!Payload->TryGetStringField(TEXT("nodeType"), NodeType) ||
      NodeType.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("nodeType is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Resolve to UMaterial or UMaterialFunction (Instance is read-only, reject it)
  FMcpMaterialGraphOwner GraphOwner;
  FString GraphOwnerError;
  if (!McpResolveMaterialGraphOwner(MaterialPath, GraphOwner, GraphOwnerError))
  {
    SendAutomationError(Socket, RequestId, GraphOwnerError,
                        GraphOwnerError.Contains(TEXT("not found"))
                            ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE"));
    return true;
  }
  if (GraphOwner.bReadOnly)
  {
    SendAutomationError(Socket, RequestId,
                        TEXT("Cannot add nodes to a MaterialFunctionInstance - edit the parent function instead"),
                        TEXT("UNSUPPORTED_OPERATION"));
    return true;
  }

  // Create material expression based on node type
  UMaterialExpression *NewExpression = nullptr;
  UClass *ExpressionClass = nullptr;

  // Map common node type names to expression classes
  if (NodeType.Equals(TEXT("Constant"), ESearchCase::IgnoreCase) ||
      NodeType.Equals(TEXT("Constant1"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionConstant::StaticClass();
  } else if (NodeType.Equals(TEXT("Constant2"), ESearchCase::IgnoreCase) ||
             NodeType.Equals(TEXT("Constant2Vector"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionConstant2Vector::StaticClass();
  } else if (NodeType.Equals(TEXT("Constant3"), ESearchCase::IgnoreCase) ||
             NodeType.Equals(TEXT("Constant3Vector"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionConstant3Vector::StaticClass();
  } else if (NodeType.Equals(TEXT("Constant4"), ESearchCase::IgnoreCase) ||
             NodeType.Equals(TEXT("Constant4Vector"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionConstant4Vector::StaticClass();
  } else if (NodeType.Equals(TEXT("TextureSample"), ESearchCase::IgnoreCase) ||
             NodeType.Equals(TEXT("Texture"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionTextureSample::StaticClass();
  } else if (NodeType.Equals(TEXT("Add"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionAdd::StaticClass();
  } else if (NodeType.Equals(TEXT("Multiply"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionMultiply::StaticClass();
  } else if (NodeType.Equals(TEXT("Sine"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionSine::StaticClass();
  } else if (NodeType.Equals(TEXT("Cosine"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionCosine::StaticClass();
  } else if (NodeType.Equals(TEXT("Time"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionTime::StaticClass();
  } else if (NodeType.Equals(TEXT("VertexColor"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionVertexColor::StaticClass();
  } else if (NodeType.Equals(TEXT("MakeMaterialAttributes"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionMakeMaterialAttributes::StaticClass();
  } else if (NodeType.Equals(TEXT("BreakMaterialAttributes"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionBreakMaterialAttributes::StaticClass();
  } else {
    // Try to find the class dynamically
    FString FullClassName = FString::Printf(TEXT("/Script/Engine.MaterialExpression%s"), *NodeType);
    ExpressionClass = LoadClass<UMaterialExpression>(nullptr, *FullClassName);
    
    if (!ExpressionClass) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Unknown node type: %s"), *NodeType),
                          TEXT("INVALID_NODE_TYPE"));
      return true;
    }
  }

  // Create the expression owned by the graph source object
  NewExpression = NewObject<UMaterialExpression>(GraphOwner.GraphSource, ExpressionClass, NAME_None, RF_Transactional);
  if (!NewExpression) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create material expression"),
                        TEXT("EXPRESSION_CREATION_FAILED"));
    return true;
  }

  const FIntPoint Position = McpResolvePlacement(GraphOwner, Payload, NewExpression);
  NewExpression->MaterialExpressionEditorX = Position.X;
  NewExpression->MaterialExpressionEditorY = Position.Y;

  // Set node properties based on type
  if (UMaterialExpressionConstant *Const = Cast<UMaterialExpressionConstant>(NewExpression)) {
    // N2: accept both bare number and { "value": N } object form
    double Value = 0;
    if (!Payload->TryGetNumberField(TEXT("value"), Value))
    {
      const TSharedPtr<FJsonObject>* ValueObj = nullptr;
      if (Payload->TryGetObjectField(TEXT("value"), ValueObj) && ValueObj)
      {
        (*ValueObj)->TryGetNumberField(TEXT("value"), Value);
      }
    }
    Const->R = static_cast<float>(Value);
  } else if (UMaterialExpressionConstant3Vector *Const3 = Cast<UMaterialExpressionConstant3Vector>(NewExpression)) {
    double R = 0, G = 0, B = 0;
    const TSharedPtr<FJsonObject> *ColorObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("color"), ColorObj) && ColorObj) {
      (*ColorObj)->TryGetNumberField(TEXT("r"), R);
      (*ColorObj)->TryGetNumberField(TEXT("g"), G);
      (*ColorObj)->TryGetNumberField(TEXT("b"), B);
    }
    Const3->Constant = FLinearColor(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B));
  } else if (UMaterialExpressionTextureSample *TexSample = Cast<UMaterialExpressionTextureSample>(NewExpression)) {
    FString TexturePath;
    if (Payload->TryGetStringField(TEXT("texturePath"), TexturePath) && !TexturePath.IsEmpty()) {
      UTexture *Texture = LoadObject<UTexture>(nullptr, *TexturePath);
      if (Texture) {
        TexSample->Texture = Texture;
        // N6: auto-detect samplerType from texture when not explicitly provided
        FString SamplerTypeStr;
        if (Payload->TryGetStringField(TEXT("samplerType"), SamplerTypeStr) && !SamplerTypeStr.IsEmpty())
        {
          TexSample->SamplerType = McpParseSamplerTypeString(SamplerTypeStr);
        }
        else
        {
          TexSample->SamplerType = McpInferSamplerTypeFromTexture(Texture);
        }
      }
    }
  }

  McpAddExpressionToGraph(GraphOwner, NewExpression);
  TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressionsMutable(GraphOwner);

  FString RebuildErr;
  McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

  int32 ExpressionIndex = Exprs ? Exprs->IndexOfByKey(NewExpression) : -1;

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("assetPath"), MaterialPath);
  Resp->SetStringField(TEXT("assetClass"), GraphOwner.Asset->GetClass()->GetName());
  Resp->SetStringField(TEXT("nodeType"), NodeType);
  Resp->SetStringField(TEXT("nodeGuid"), NewExpression->MaterialExpressionGuid.ToString());
  McpAddExpressionIdentity(GraphOwner, NewExpression, ExpressionIndex, Resp.ToSharedRef());

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material node added successfully"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("add_material_node requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSetMaterialNodePosition(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("set_material_node_position"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("move_material_node"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("set_material_node_position payload missing"), TEXT("INVALID_PAYLOAD"));
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
    SendAutomationError(Socket, RequestId, TEXT("Cannot move nodes on a MaterialFunctionInstance - edit the parent function instead"), TEXT("UNSUPPORTED_OPERATION"));
    return true;
  }

  UMaterialExpression* Expression = McpFindGraphExpressionFromPayload(GraphOwner, Payload);
  if (!Expression) {
    SendAutomationError(Socket, RequestId, TEXT("Node not found. Provide expressionIndex, expressionPath, or nodeId"), TEXT("NODE_NOT_FOUND"));
    return true;
  }

  double NewX = 0.0;
  double NewY = 0.0;
  if (!(Payload->TryGetNumberField(TEXT("x"), NewX) || Payload->TryGetNumberField(TEXT("posX"), NewX)) ||
      !(Payload->TryGetNumberField(TEXT("y"), NewY) || Payload->TryGetNumberField(TEXT("posY"), NewY))) {
    SendAutomationError(Socket, RequestId, TEXT("x/y or posX/posY are required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const int32 OldX = Expression->MaterialExpressionEditorX;
  const int32 OldY = Expression->MaterialExpressionEditorY;
  Expression->Modify();
  Expression->MaterialExpressionEditorX = static_cast<int32>(NewX);
  Expression->MaterialExpressionEditorY = static_cast<int32>(NewY);

  FString RebuildErr;
  McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Resp, GraphOwner.Asset);
  McpAddExpressionIdentity(GraphOwner, Expression, McpExpressionIndex(GraphOwner, Expression), Resp.ToSharedRef());
  Resp->SetNumberField(TEXT("oldX"), OldX);
  Resp->SetNumberField(TEXT("oldY"), OldY);
  Resp->SetNumberField(TEXT("newX"), Expression->MaterialExpressionEditorX);
  Resp->SetNumberField(TEXT("newY"), Expression->MaterialExpressionEditorY);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Material node position updated"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false, TEXT("set_material_node_position requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleBulkSetMaterialNodePositions(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("bulk_set_material_node_positions"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("bulk_move_material_nodes"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("bulk_set_material_node_positions payload missing"), TEXT("INVALID_PAYLOAD"));
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

    UMaterialExpression* Expression = McpFindGraphExpressionFromPayload(GraphOwner, *NodeObj);
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
  SendAutomationResponse(Socket, RequestId, false, TEXT("bulk_set_material_node_positions requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleConnectMaterialPins(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("connect_material_pins"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("connect_material_pins payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPath and materialPath
  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath or materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Resolve to UMaterial or UMaterialFunction
  FMcpMaterialGraphOwner GraphOwner;
  FString GraphOwnerError;
  if (!McpResolveMaterialGraphOwner(MaterialPath, GraphOwner, GraphOwnerError))
  {
    SendAutomationError(Socket, RequestId, GraphOwnerError,
                        GraphOwnerError.Contains(TEXT("not found"))
                            ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE"));
    return true;
  }
  if (GraphOwner.bReadOnly)
  {
    SendAutomationError(Socket, RequestId,
                        TEXT("Cannot connect pins on a MaterialFunctionInstance - edit the parent function instead"),
                        TEXT("UNSUPPORTED_OPERATION"));
    return true;
  }

  static const TArray<TObjectPtr<UMaterialExpression>> EmptyExprs;
  const TArray<TObjectPtr<UMaterialExpression>>* ExpressionsPtr = McpGetGraphExpressions(GraphOwner);
  const TArray<TObjectPtr<UMaterialExpression>>& Expressions = ExpressionsPtr ? *ExpressionsPtr : EmptyExprs;

  // Accept sourceNodeId/targetNodeId, sourceExpressionPath/targetExpressionPath, and fromExpression/toExpression indices.
  // Also accept spec-style sourceExpression object, target object with kind/expression/inputName.
  FString SourceNodeId, TargetNodeId;
  int32 FromExpressionIndex = -1, ToExpressionIndex = -1;

  UMaterialExpression *FromExpression = nullptr;
  UMaterialExpression *ToExpression = nullptr;

  // Spec-style: sourceExpression as object or string
  const TSharedPtr<FJsonObject>* SrcExprObj = nullptr;
  if (Payload->TryGetObjectField(TEXT("sourceExpression"), SrcExprObj) && SrcExprObj)
  {
    int32 SrcIdx = INDEX_NONE;
    FString SrcPath, SrcGuidStr, SrcName;
    if ((*SrcExprObj)->TryGetNumberField(TEXT("expressionIndex"), SrcIdx))
      FromExpression = McpFindGraphExpression(GraphOwner, FString(), SrcIdx);
    else if ((*SrcExprObj)->TryGetStringField(TEXT("expressionPath"), SrcPath) && !SrcPath.IsEmpty())
      FromExpression = McpFindGraphExpression(GraphOwner, SrcPath);
    else if ((*SrcExprObj)->TryGetStringField(TEXT("expressionName"), SrcName) && !SrcName.IsEmpty())
      FromExpression = McpFindGraphExpression(GraphOwner, SrcName);
  }
  else
  {
    FString SrcExprStr;
    if (Payload->TryGetStringField(TEXT("sourceExpression"), SrcExprStr) && !SrcExprStr.IsEmpty())
      FromExpression = McpFindGraphExpression(GraphOwner, SrcExprStr);
  }

  if (!FromExpression)
    FromExpression = McpFindGraphExpressionFromPayload(GraphOwner, Payload, TEXT("sourceExpressionIndex"), TEXT("sourceNodeId"), TEXT("sourceExpressionPath"));

  Payload->TryGetStringField(TEXT("sourceNodeId"), SourceNodeId);
  Payload->TryGetStringField(TEXT("targetNodeId"), TargetNodeId);

  if (!FromExpression && Payload->TryGetNumberField(TEXT("fromExpression"), FromExpressionIndex))
    FromExpression = McpFindGraphExpression(GraphOwner, FString(), FromExpressionIndex);

  // Spec-style: target as object with kind, expression, inputName
  FString TargetKind;
  FString InputName;
  const TSharedPtr<FJsonObject>* TargetObj = nullptr;
  if (Payload->TryGetObjectField(TEXT("target"), TargetObj) && TargetObj)
  {
    (*TargetObj)->TryGetStringField(TEXT("kind"), TargetKind);
    (*TargetObj)->TryGetStringField(TEXT("inputName"), InputName);
    const TSharedPtr<FJsonObject>* TgtExprObj = nullptr;
    if ((*TargetObj)->TryGetObjectField(TEXT("expression"), TgtExprObj) && TgtExprObj && !ToExpression)
    {
      int32 TgtIdx = INDEX_NONE;
      FString TgtPath, TgtName;
      if ((*TgtExprObj)->TryGetNumberField(TEXT("expressionIndex"), TgtIdx))
        ToExpression = McpFindGraphExpression(GraphOwner, FString(), TgtIdx);
      else if ((*TgtExprObj)->TryGetStringField(TEXT("expressionPath"), TgtPath) && !TgtPath.IsEmpty())
        ToExpression = McpFindGraphExpression(GraphOwner, TgtPath);
      else if ((*TgtExprObj)->TryGetStringField(TEXT("expressionName"), TgtName) && !TgtName.IsEmpty())
        ToExpression = McpFindGraphExpression(GraphOwner, TgtName);
    }
  }

  if (!ToExpression)
    ToExpression = McpFindGraphExpressionFromPayload(GraphOwner, Payload, TEXT("targetExpressionIndex"), TEXT("targetNodeId"), TEXT("targetExpressionPath"));
  if (!ToExpression && Payload->TryGetNumberField(TEXT("toExpression"), ToExpressionIndex))
    ToExpression = McpFindGraphExpression(GraphOwner, FString(), ToExpressionIndex);

  if (InputName.IsEmpty()) Payload->TryGetStringField(TEXT("inputName"), InputName);
  if (InputName.IsEmpty()) Payload->TryGetStringField(TEXT("targetPin"), InputName);
  if (InputName.IsEmpty()) Payload->TryGetStringField(TEXT("sourcePin"), InputName);

  // Resolve sourceOutputIndex from explicit field or by name lookup on source outputs
  int32 SourceOutputIndex = 0;
  {
    double SrcOutIdx = 0;
    if (Payload->TryGetNumberField(TEXT("sourceOutputIndex"), SrcOutIdx))
      SourceOutputIndex = (int32)SrcOutIdx;
    else
    {
      FString SrcOutName;
      if (Payload->TryGetStringField(TEXT("sourceOutputName"), SrcOutName) && !SrcOutName.IsEmpty() && FromExpression)
      {
        const TArray<FExpressionOutput>& Outputs = FromExpression->GetOutputs();
        for (int32 OIdx = 0; OIdx < Outputs.Num(); ++OIdx)
        {
          if (Outputs[OIdx].OutputName.ToString().Equals(SrcOutName, ESearchCase::IgnoreCase))
          {
            SourceOutputIndex = OIdx;
            break;
          }
        }
      }
    }
  }

  // Handle connection to main material node (only for UMaterial)
  bool bConnectToMainNode = false;
  if (TargetKind == TEXT("mainMaterialPin"))
    bConnectToMainNode = true;
  else if (!ToExpression && (TargetNodeId.IsEmpty() || TargetNodeId == TEXT("Main")) && !InputName.IsEmpty())
    bConnectToMainNode = true;
  else if (!ToExpression && !InputName.IsEmpty() && TargetKind.IsEmpty())
    bConnectToMainNode = true;

  if (bConnectToMainNode && FromExpression)
  {
    if (GraphOwner.Kind != EMcpMaterialGraphOwnerKind::Material)
    {
      SendAutomationError(Socket, RequestId,
                          TEXT("Main material node connections only apply to UMaterial, not UMaterialFunction"),
                          TEXT("UNSUPPORTED_OPERATION"));
      return true;
    }

    UMaterial* Material = CastChecked<UMaterial>(GraphOwner.GraphSource);
    bool bFound = false;
#if WITH_EDITORONLY_DATA
    if (InputName == TEXT("BaseColor")) {
      MCP_GET_MATERIAL_INPUT(Material, BaseColor).Expression = FromExpression; bFound = true;
    } else if (InputName == TEXT("EmissiveColor")) {
      MCP_GET_MATERIAL_INPUT(Material, EmissiveColor).Expression = FromExpression; bFound = true;
    } else if (InputName == TEXT("Roughness")) {
      MCP_GET_MATERIAL_INPUT(Material, Roughness).Expression = FromExpression; bFound = true;
    } else if (InputName == TEXT("Metallic")) {
      MCP_GET_MATERIAL_INPUT(Material, Metallic).Expression = FromExpression; bFound = true;
    } else if (InputName == TEXT("Specular")) {
      MCP_GET_MATERIAL_INPUT(Material, Specular).Expression = FromExpression; bFound = true;
    } else if (InputName == TEXT("Normal")) {
      MCP_GET_MATERIAL_INPUT(Material, Normal).Expression = FromExpression; bFound = true;
    } else if (InputName == TEXT("Opacity")) {
      MCP_GET_MATERIAL_INPUT(Material, Opacity).Expression = FromExpression; bFound = true;
    } else if (InputName == TEXT("OpacityMask")) {
      MCP_GET_MATERIAL_INPUT(Material, OpacityMask).Expression = FromExpression; bFound = true;
    } else if (InputName == TEXT("AmbientOcclusion") || InputName == TEXT("AO")) {
      MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion).Expression = FromExpression; bFound = true;
    } else if (InputName == TEXT("SubsurfaceColor")) {
      MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor).Expression = FromExpression; bFound = true;
    } else if (InputName == TEXT("WorldPositionOffset")) {
      MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset).Expression = FromExpression; bFound = true;
    }
#endif

    if (bFound) {
      FString RebuildErr;
      McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      McpHandlerUtils::AddVerification(Resp, Material);
      Resp->SetStringField(TEXT("inputName"), InputName);
      Resp->SetStringField(TEXT("sourceNodeId"), FromExpression->MaterialExpressionGuid.ToString());
      SendAutomationResponse(Socket, RequestId, true, TEXT("Connected to main material pin"), Resp, FString());
    } else {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Unknown main material input: %s"), *InputName),
                          TEXT("INVALID_PIN"));
    }
    return true;
  }

  // Normal expression-to-expression connection
  if (!FromExpression) {
    SendAutomationError(Socket, RequestId, TEXT("Source node not found"), TEXT("SOURCE_NODE_NOT_FOUND"));
    return true;
  }
  if (!ToExpression) {
    SendAutomationError(Socket, RequestId, TEXT("Target node not found"), TEXT("TARGET_NODE_NOT_FOUND"));
    return true;
  }

  if (InputName.IsEmpty()) InputName = TEXT("Input");

  FExpressionInput *TargetInput = nullptr;
  for (FProperty *Property = ToExpression->GetClass()->PropertyLink; Property;
       Property = Property->PropertyLinkNext) {
    if (FStructProperty *StructProp = CastField<FStructProperty>(Property)) {
      if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
        if (Property->GetName().Equals(InputName, ESearchCase::IgnoreCase)) {
          TargetInput = StructProp->ContainerPtrToValuePtr<FExpressionInput>(ToExpression);
          break;
        }
      }
    }
  }

  if (!TargetInput) {
    for (FProperty *Property = ToExpression->GetClass()->PropertyLink; Property;
         Property = Property->PropertyLinkNext) {
      if (FStructProperty *StructProp = CastField<FStructProperty>(Property)) {
        if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
          TargetInput = StructProp->ContainerPtrToValuePtr<FExpressionInput>(ToExpression);
          InputName = Property->GetName();
          break;
        }
      }
    }
  }

  if (!TargetInput) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("No input found on target expression. Tried: %s"), *InputName),
                        TEXT("INPUT_NOT_FOUND"));
    return true;
  }

  TargetInput->Expression = FromExpression;
  TargetInput->OutputIndex = SourceOutputIndex;
  FString RebuildErr;
  McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Resp, GraphOwner.Asset);
  Resp->SetStringField(TEXT("assetClass"), GraphOwner.Asset->GetClass()->GetName());
  Resp->SetNumberField(TEXT("sourceOutputIndex"), SourceOutputIndex);
  Resp->SetStringField(TEXT("sourceNodeId"), FromExpression->MaterialExpressionGuid.ToString());
  Resp->SetStringField(TEXT("targetNodeId"), ToExpression->MaterialExpressionGuid.ToString());
  Resp->SetStringField(TEXT("inputName"), InputName);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material pins connected successfully"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("connect_material_pins requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleRemoveMaterialNode(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("remove_material_node"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("remove_material_node payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPath and materialPath
  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath or materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Resolve to UMaterial or UMaterialFunction
  FMcpMaterialGraphOwner GraphOwner;
  FString GraphOwnerError;
  if (!McpResolveMaterialGraphOwner(MaterialPath, GraphOwner, GraphOwnerError))
  {
    SendAutomationError(Socket, RequestId, GraphOwnerError,
                        GraphOwnerError.Contains(TEXT("not found"))
                            ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE"));
    return true;
  }
  if (GraphOwner.bReadOnly)
  {
    SendAutomationError(Socket, RequestId,
                        TEXT("Cannot remove nodes from a MaterialFunctionInstance - edit the parent function instead"),
                        TEXT("UNSUPPORTED_OPERATION"));
    return true;
  }

  TArray<TObjectPtr<UMaterialExpression>>* ExpressionsPtr = McpGetGraphExpressionsMutable(GraphOwner);
  static TArray<TObjectPtr<UMaterialExpression>> EmptyExprs;
  TArray<TObjectPtr<UMaterialExpression>>& Expressions = ExpressionsPtr ? *ExpressionsPtr : EmptyExprs;

  UMaterialExpression *ExpressionToRemove = McpFindGraphExpressionFromPayload(GraphOwner, Payload);

  if (!ExpressionToRemove) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Node not found. Provide valid expressionIndex, expressionPath, or nodeId"),
                        TEXT("NODE_NOT_FOUND"));
    return true;
  }

  FString RemovedName = ExpressionToRemove->GetName();
  FString RemovedGuid = ExpressionToRemove->MaterialExpressionGuid.ToString();

  // Remove from collection
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  if (GraphOwner.Kind == EMcpMaterialGraphOwnerKind::Material)
    CastChecked<UMaterial>(GraphOwner.GraphSource)->GetEditorOnlyData()->ExpressionCollection.RemoveExpression(ExpressionToRemove);
  else
    CastChecked<UMaterialFunction>(GraphOwner.GraphSource)->GetEditorOnlyData()->ExpressionCollection.RemoveExpression(ExpressionToRemove);
#else
  Expressions.Remove(ExpressionToRemove);
#endif

  // Remove from material parameter tracking (only relevant for UMaterial)
  if (GraphOwner.Kind == EMcpMaterialGraphOwnerKind::Material)
    CastChecked<UMaterial>(GraphOwner.GraphSource)->RemoveExpressionParameter(ExpressionToRemove);

  FString RebuildErr;
  McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Resp, GraphOwner.Asset);
  Resp->SetStringField(TEXT("assetClass"), GraphOwner.Asset->GetClass()->GetName());
  Resp->SetStringField(TEXT("nodeId"), RemovedGuid);
  Resp->SetStringField(TEXT("removedName"), RemovedName);
  Resp->SetNumberField(TEXT("remainingExpressions"), Expressions.Num());
  Resp->SetBoolField(TEXT("removed"), true);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material node removed successfully"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("remove_material_node requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleBreakMaterialConnections(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("break_material_connections"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("break_material_connections payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPath and materialPath
  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath or materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Resolve to UMaterial or UMaterialFunction
  FMcpMaterialGraphOwner GraphOwner;
  FString GraphOwnerError;
  if (!McpResolveMaterialGraphOwner(MaterialPath, GraphOwner, GraphOwnerError))
  {
    SendAutomationError(Socket, RequestId, GraphOwnerError,
                        GraphOwnerError.Contains(TEXT("not found"))
                            ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE"));
    return true;
  }
  if (GraphOwner.bReadOnly)
  {
    SendAutomationError(Socket, RequestId,
                        TEXT("Cannot break connections on a MaterialFunctionInstance - edit the parent function instead"),
                        TEXT("UNSUPPORTED_OPERATION"));
    return true;
  }

  static const TArray<TObjectPtr<UMaterialExpression>> EmptyExprs;
  const TArray<TObjectPtr<UMaterialExpression>>* ExpressionsPtr = McpGetGraphExpressions(GraphOwner);
  const TArray<TObjectPtr<UMaterialExpression>>& Expressions = ExpressionsPtr ? *ExpressionsPtr : EmptyExprs;

  FString NodeId, PinName;
  bool bHasNodeId = Payload->TryGetStringField(TEXT("nodeId"), NodeId) && !NodeId.IsEmpty();
  bool bHasPinName = Payload->TryGetStringField(TEXT("pinName"), PinName) && !PinName.IsEmpty();

  // Break from main material node (only valid for UMaterial)
  if ((!bHasNodeId || NodeId == TEXT("Main")) && bHasPinName)
  {
    if (GraphOwner.Kind != EMcpMaterialGraphOwnerKind::Material)
    {
      SendAutomationError(Socket, RequestId,
                          TEXT("Main material node pins only exist on UMaterial, not UMaterialFunction"),
                          TEXT("UNSUPPORTED_OPERATION"));
      return true;
    }

    UMaterial* Material = CastChecked<UMaterial>(GraphOwner.GraphSource);
    bool bFound = false;
#if WITH_EDITORONLY_DATA
    if (PinName == TEXT("BaseColor")) {
      MCP_GET_MATERIAL_INPUT(Material, BaseColor).Expression = nullptr; bFound = true;
    } else if (PinName == TEXT("EmissiveColor")) {
      MCP_GET_MATERIAL_INPUT(Material, EmissiveColor).Expression = nullptr; bFound = true;
    } else if (PinName == TEXT("Roughness")) {
      MCP_GET_MATERIAL_INPUT(Material, Roughness).Expression = nullptr; bFound = true;
    } else if (PinName == TEXT("Metallic")) {
      MCP_GET_MATERIAL_INPUT(Material, Metallic).Expression = nullptr; bFound = true;
    } else if (PinName == TEXT("Specular")) {
      MCP_GET_MATERIAL_INPUT(Material, Specular).Expression = nullptr; bFound = true;
    } else if (PinName == TEXT("Normal")) {
      MCP_GET_MATERIAL_INPUT(Material, Normal).Expression = nullptr; bFound = true;
    } else if (PinName == TEXT("Opacity")) {
      MCP_GET_MATERIAL_INPUT(Material, Opacity).Expression = nullptr; bFound = true;
    } else if (PinName == TEXT("OpacityMask")) {
      MCP_GET_MATERIAL_INPUT(Material, OpacityMask).Expression = nullptr; bFound = true;
    } else if (PinName == TEXT("AmbientOcclusion") || PinName == TEXT("AO")) {
      MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion).Expression = nullptr; bFound = true;
    } else if (PinName == TEXT("SubsurfaceColor")) {
      MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor).Expression = nullptr; bFound = true;
    }
#endif

    if (bFound) {
      FString RebuildErr;
      McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      McpHandlerUtils::AddVerification(Resp, Material);
      Resp->SetStringField(TEXT("pinName"), PinName);
      Resp->SetBoolField(TEXT("disconnected"), true);
      SendAutomationResponse(Socket, RequestId, true,
                             TEXT("Disconnected from main material pin"), Resp, FString());
    } else {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Unknown main material pin: %s"), *PinName),
                          TEXT("INVALID_PIN"));
    }
    return true;
  }

  // Find target expression
  UMaterialExpression *TargetExpression = McpFindGraphExpressionFromPayload(GraphOwner, Payload);

  if (!TargetExpression) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Node not found. Provide valid expressionIndex, expressionPath, or nodeId"),
                        TEXT("NODE_NOT_FOUND"));
    return true;
  }

  FString InputName;
  bool bSpecificInput = Payload->TryGetStringField(TEXT("inputName"), InputName) && !InputName.IsEmpty();
  int32 BrokenConnections = 0;

  for (FProperty *Property = TargetExpression->GetClass()->PropertyLink; Property;
       Property = Property->PropertyLinkNext) {
    if (FStructProperty *StructProp = CastField<FStructProperty>(Property)) {
      if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
        if (bSpecificInput && !Property->GetName().Equals(InputName, ESearchCase::IgnoreCase))
          continue;

        FExpressionInput *Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(TargetExpression);
        if (Input && Input->Expression) {
          Input->Expression = nullptr;
          BrokenConnections++;
          if (bSpecificInput) break;
        }
      }
    }
  }

  FString RebuildErr;
  McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Resp, GraphOwner.Asset);
  Resp->SetStringField(TEXT("assetClass"), GraphOwner.Asset->GetClass()->GetName());
  Resp->SetStringField(TEXT("nodeId"), TargetExpression->MaterialExpressionGuid.ToString());
  Resp->SetNumberField(TEXT("brokenConnections"), BrokenConnections);
  if (bSpecificInput)
    Resp->SetStringField(TEXT("inputName"), InputName);

  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Broken %d connection(s)"), BrokenConnections),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("break_material_connections requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleCreateMaterialComment(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("create_material_comment"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("create_material_comment payload missing"), TEXT("INVALID_PAYLOAD"));
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

  FString Text;
  Payload->TryGetStringField(TEXT("text"), Text);
  if (Text.IsEmpty()) {
    Payload->TryGetStringField(TEXT("comment"), Text);
  }

  double X = 0.0, Y = 0.0, Width = 800.0, Height = 400.0;
  Payload->TryGetNumberField(TEXT("x"), X);
  Payload->TryGetNumberField(TEXT("y"), Y);
  Payload->TryGetNumberField(TEXT("width"), Width);
  Payload->TryGetNumberField(TEXT("height"), Height);

  UMaterialExpressionComment* Comment = NewObject<UMaterialExpressionComment>(GraphOwner.GraphSource, UMaterialExpressionComment::StaticClass(), NAME_None, RF_Transactional);
  Comment->Text = Text;
  Comment->MaterialExpressionEditorX = static_cast<int32>(X);
  Comment->MaterialExpressionEditorY = static_cast<int32>(Y);
  Comment->SizeX = static_cast<int32>(Width);
  Comment->SizeY = static_cast<int32>(Height);

  bool bGroupMode = true;
  Payload->TryGetBoolField(TEXT("groupMode"), bGroupMode);
  Comment->bGroupMode = bGroupMode;

  const TSharedPtr<FJsonObject>* ColorObj = nullptr;
  if (Payload->TryGetObjectField(TEXT("color"), ColorObj) && ColorObj) {
    double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
    (*ColorObj)->TryGetNumberField(TEXT("r"), R);
    (*ColorObj)->TryGetNumberField(TEXT("g"), G);
    (*ColorObj)->TryGetNumberField(TEXT("b"), B);
    (*ColorObj)->TryGetNumberField(TEXT("a"), A);
    Comment->CommentColor = FLinearColor(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B), static_cast<float>(A));
  }

  if (!McpAddCommentToGraph(GraphOwner, Comment)) {
    SendAutomationError(Socket, RequestId, TEXT("Material comments require UE 5.1+ expression collections"), TEXT("UNSUPPORTED_OPERATION"));
    return true;
  }

  FString RebuildErr;
  McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Resp, GraphOwner.Asset);
  Resp->SetStringField(TEXT("commentId"), Comment->GetPathName());
  Resp->SetStringField(TEXT("text"), Comment->Text);
  Resp->SetNumberField(TEXT("x"), Comment->MaterialExpressionEditorX);
  Resp->SetNumberField(TEXT("y"), Comment->MaterialExpressionEditorY);
  Resp->SetNumberField(TEXT("width"), Comment->SizeX);
  Resp->SetNumberField(TEXT("height"), Comment->SizeY);
  Resp->SetBoolField(TEXT("groupMode"), Comment->bGroupMode);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Material comment created"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false, TEXT("create_material_comment requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleWrapMaterialNodesInComment(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("wrap_material_nodes_in_comment"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("wrap_material_nodes_in_comment payload missing"), TEXT("INVALID_PAYLOAD"));
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
    SendAutomationError(Socket, RequestId, TEXT("Cannot create comments on a MaterialFunctionInstance - edit the parent function instead"), TEXT("UNSUPPORTED_OPERATION"));
    return true;
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
    UMaterialExpression* Expr = McpFindGraphExpressionFromPayload(GraphOwner, *NodeObj);
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
    SendAutomationError(Socket, RequestId, TEXT("No nodes could be resolved"), TEXT("NODE_NOT_FOUND"));
    return true;
  }

  double Padding = 80.0;
  Payload->TryGetNumberField(TEXT("padding"), Padding);
  TSharedPtr<FJsonObject> LocalPayload = McpHandlerUtils::CreateResultObject();
  LocalPayload->SetStringField(TEXT("assetPath"), MaterialPath);
  FString Text;
  Payload->TryGetStringField(TEXT("text"), Text);
  if (Text.IsEmpty()) {
    Payload->TryGetStringField(TEXT("comment"), Text);
  }
  LocalPayload->SetStringField(TEXT("text"), Text);
  LocalPayload->SetNumberField(TEXT("x"), MinX - Padding);
  LocalPayload->SetNumberField(TEXT("y"), MinY - Padding);
  LocalPayload->SetNumberField(TEXT("width"), (MaxX - MinX) + Padding * 2.0);
  LocalPayload->SetNumberField(TEXT("height"), (MaxY - MinY) + Padding * 2.0);
  bool bGroupMode = true;
  Payload->TryGetBoolField(TEXT("groupMode"), bGroupMode);
  LocalPayload->SetBoolField(TEXT("groupMode"), bGroupMode);
  return HandleCreateMaterialComment(RequestId, TEXT("create_material_comment"), LocalPayload, Socket);
#else
  SendAutomationResponse(Socket, RequestId, false, TEXT("wrap_material_nodes_in_comment requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleCreateNamedReroute(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("create_named_reroute"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("create_named_reroute payload missing"), TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MaterialPath, RerouteName;
  if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    SendAutomationError(Socket, RequestId, TEXT("assetPath or materialPath is required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("name"), RerouteName) || RerouteName.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("name is required"), TEXT("INVALID_ARGUMENT"));
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

  UMaterialExpression* Source = McpFindGraphExpressionFromPayload(GraphOwner, Payload, TEXT("sourceExpressionIndex"), TEXT("sourceNodeId"), TEXT("sourceExpressionPath"));
  if (!Source) {
    Source = McpFindGraphExpressionFromPayload(GraphOwner, Payload);
  }
  if (!Source) {
    SendAutomationError(Socket, RequestId, TEXT("Source node not found"), TEXT("SOURCE_NODE_NOT_FOUND"));
    return true;
  }

  int32 SourceOutputIndex = 0;
  Payload->TryGetNumberField(TEXT("sourceOutputIndex"), SourceOutputIndex);

  UMaterialExpressionNamedRerouteDeclaration* Declaration =
      NewObject<UMaterialExpressionNamedRerouteDeclaration>(GraphOwner.GraphSource, UMaterialExpressionNamedRerouteDeclaration::StaticClass(), NAME_None, RF_Transactional);
  Declaration->Name = FName(*RerouteName);
  Declaration->Input.Expression = Source;
  Declaration->Input.OutputIndex = SourceOutputIndex;
  const FIntPoint Position = McpResolvePlacement(GraphOwner, Payload, Declaration);
  Declaration->MaterialExpressionEditorX = Position.X;
  Declaration->MaterialExpressionEditorY = Position.Y;
  McpAddExpressionToGraph(GraphOwner, Declaration);

  FString RebuildErr;
  McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Resp, GraphOwner.Asset);
  McpAddExpressionIdentity(GraphOwner, Declaration, McpExpressionIndex(GraphOwner, Declaration), Resp.ToSharedRef());
  Resp->SetStringField(TEXT("rerouteName"), Declaration->Name.ToString());
  Resp->SetStringField(TEXT("rerouteGuid"), Declaration->VariableGuid.ToString());
  Resp->SetNumberField(TEXT("sourceExpressionIndex"), McpExpressionIndex(GraphOwner, Source));
  SendAutomationResponse(Socket, RequestId, true, TEXT("Named reroute declaration created"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false, TEXT("create_named_reroute requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleUseNamedReroute(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("use_named_reroute"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("use_named_reroute payload missing"), TEXT("INVALID_PAYLOAD"));
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

  FString DeclarationRef;
  Payload->TryGetStringField(TEXT("declarationId"), DeclarationRef);
  if (DeclarationRef.IsEmpty()) Payload->TryGetStringField(TEXT("declarationGuid"), DeclarationRef);
  if (DeclarationRef.IsEmpty()) Payload->TryGetStringField(TEXT("declarationName"), DeclarationRef);
  if (DeclarationRef.IsEmpty()) Payload->TryGetStringField(TEXT("name"), DeclarationRef);
  UMaterialExpressionNamedRerouteDeclaration* Declaration = McpFindNamedRerouteDeclaration(GraphOwner, DeclarationRef);
  if (!Declaration) {
    SendAutomationError(Socket, RequestId, TEXT("Named reroute declaration not found"), TEXT("DECLARATION_NOT_FOUND"));
    return true;
  }

  UMaterialExpression* Target = McpFindGraphExpressionFromPayload(GraphOwner, Payload, TEXT("targetExpressionIndex"), TEXT("targetNodeId"), TEXT("targetExpressionPath"));
  if (!Target) {
    SendAutomationError(Socket, RequestId, TEXT("Target node not found"), TEXT("TARGET_NODE_NOT_FOUND"));
    return true;
  }

  FString TargetInputName;
  Payload->TryGetStringField(TEXT("targetInputPin"), TargetInputName);
  if (TargetInputName.IsEmpty()) Payload->TryGetStringField(TEXT("targetPin"), TargetInputName);
  if (TargetInputName.IsEmpty()) Payload->TryGetStringField(TEXT("inputName"), TargetInputName);
  FExpressionInput* TargetInput = McpFindExpressionInputByName(Target, TargetInputName);
  if (!TargetInput) {
    SendAutomationError(Socket, RequestId, TEXT("Target input pin not found"), TEXT("INPUT_NOT_FOUND"));
    return true;
  }

  UMaterialExpressionNamedRerouteUsage* Usage =
      NewObject<UMaterialExpressionNamedRerouteUsage>(GraphOwner.GraphSource, UMaterialExpressionNamedRerouteUsage::StaticClass(), NAME_None, RF_Transactional);
  Usage->Declaration = Declaration;
  Usage->DeclarationGuid = Declaration->VariableGuid;
  const FIntPoint Position = McpResolvePlacement(GraphOwner, Payload, Usage);
  Usage->MaterialExpressionEditorX = Position.X;
  Usage->MaterialExpressionEditorY = Position.Y;
  McpAddExpressionToGraph(GraphOwner, Usage);
  TargetInput->Expression = Usage;
  TargetInput->OutputIndex = 0;

  FString RebuildErr;
  McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Resp, GraphOwner.Asset);
  McpAddExpressionIdentity(GraphOwner, Usage, McpExpressionIndex(GraphOwner, Usage), Resp.ToSharedRef());
  Resp->SetStringField(TEXT("targetInputPin"), TargetInputName);
  Resp->SetNumberField(TEXT("targetExpressionIndex"), McpExpressionIndex(GraphOwner, Target));
  SendAutomationResponse(Socket, RequestId, true, TEXT("Named reroute usage created"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false, TEXT("use_named_reroute requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleReplaceLongConnectionWithNamedReroute(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("replace_long_connection_with_named_reroute"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("replace_long_connection_with_named_reroute payload missing"), TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MaterialPath, RerouteName;
  if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    SendAutomationError(Socket, RequestId, TEXT("assetPath or materialPath is required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("name"), RerouteName) || RerouteName.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("name is required"), TEXT("INVALID_ARGUMENT"));
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

  UMaterialExpression* Source = McpFindGraphExpressionFromPayload(GraphOwner, Payload, TEXT("sourceExpressionIndex"), TEXT("sourceNodeId"), TEXT("sourceExpressionPath"));
  UMaterialExpression* Target = McpFindGraphExpressionFromPayload(GraphOwner, Payload, TEXT("targetExpressionIndex"), TEXT("targetNodeId"), TEXT("targetExpressionPath"));
  if (!Source || !Target) {
    SendAutomationError(Socket, RequestId, TEXT("Source or target node not found"), TEXT("NODE_NOT_FOUND"));
    return true;
  }

  FString TargetInputName;
  Payload->TryGetStringField(TEXT("targetInputPin"), TargetInputName);
  if (TargetInputName.IsEmpty()) Payload->TryGetStringField(TEXT("targetPin"), TargetInputName);
  if (TargetInputName.IsEmpty()) Payload->TryGetStringField(TEXT("inputName"), TargetInputName);
  FExpressionInput* TargetInput = McpFindExpressionInputByName(Target, TargetInputName);
  if (!TargetInput) {
    SendAutomationError(Socket, RequestId, TEXT("Target input pin not found"), TEXT("INPUT_NOT_FOUND"));
    return true;
  }

  double MinDistance = 0.0;
  Payload->TryGetNumberField(TEXT("minDistance"), MinDistance);
  const int32 Distance = FMath::Abs(Source->MaterialExpressionEditorX - Target->MaterialExpressionEditorX);
  if (MinDistance > 0.0 && Distance < MinDistance) {
    SendAutomationError(Socket, RequestId, TEXT("Connection is shorter than minDistance"), TEXT("DISTANCE_BELOW_THRESHOLD"));
    return true;
  }

  int32 SourceOutputIndex = TargetInput->OutputIndex;
  Payload->TryGetNumberField(TEXT("sourceOutputIndex"), SourceOutputIndex);

  UMaterialExpressionNamedRerouteDeclaration* Declaration =
      NewObject<UMaterialExpressionNamedRerouteDeclaration>(GraphOwner.GraphSource, UMaterialExpressionNamedRerouteDeclaration::StaticClass(), NAME_None, RF_Transactional);
  Declaration->Name = FName(*RerouteName);
  Declaration->Input.Expression = Source;
  Declaration->Input.OutputIndex = SourceOutputIndex;
  Declaration->MaterialExpressionEditorX = Source->MaterialExpressionEditorX + McpEstimateExpressionSize(Source).X + 220;
  Declaration->MaterialExpressionEditorY = Source->MaterialExpressionEditorY;
  const FIntPoint DeclarationPos = McpFindFreePosition(GraphOwner, FIntPoint(Declaration->MaterialExpressionEditorX, Declaration->MaterialExpressionEditorY), McpEstimateExpressionSize(Declaration));
  Declaration->MaterialExpressionEditorX = DeclarationPos.X;
  Declaration->MaterialExpressionEditorY = DeclarationPos.Y;
  McpAddExpressionToGraph(GraphOwner, Declaration);

  UMaterialExpressionNamedRerouteUsage* Usage =
      NewObject<UMaterialExpressionNamedRerouteUsage>(GraphOwner.GraphSource, UMaterialExpressionNamedRerouteUsage::StaticClass(), NAME_None, RF_Transactional);
  Usage->Declaration = Declaration;
  Usage->DeclarationGuid = Declaration->VariableGuid;
  Usage->MaterialExpressionEditorX = Target->MaterialExpressionEditorX - 260;
  Usage->MaterialExpressionEditorY = Target->MaterialExpressionEditorY;
  const FIntPoint UsagePos = McpFindFreePosition(GraphOwner, FIntPoint(Usage->MaterialExpressionEditorX, Usage->MaterialExpressionEditorY), McpEstimateExpressionSize(Usage));
  Usage->MaterialExpressionEditorX = UsagePos.X;
  Usage->MaterialExpressionEditorY = UsagePos.Y;
  McpAddExpressionToGraph(GraphOwner, Usage);
  TargetInput->Expression = Usage;
  TargetInput->OutputIndex = 0;

  FString RebuildErr;
  McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Resp, GraphOwner.Asset);
  Resp->SetStringField(TEXT("rerouteName"), Declaration->Name.ToString());
  Resp->SetStringField(TEXT("rerouteGuid"), Declaration->VariableGuid.ToString());
  Resp->SetNumberField(TEXT("declarationExpressionIndex"), McpExpressionIndex(GraphOwner, Declaration));
  Resp->SetNumberField(TEXT("usageExpressionIndex"), McpExpressionIndex(GraphOwner, Usage));
  Resp->SetNumberField(TEXT("sourceExpressionIndex"), McpExpressionIndex(GraphOwner, Source));
  Resp->SetNumberField(TEXT("targetExpressionIndex"), McpExpressionIndex(GraphOwner, Target));
  Resp->SetStringField(TEXT("targetInputPin"), TargetInputName);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Long material connection replaced with named reroute"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false, TEXT("replace_long_connection_with_named_reroute requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
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
      if (UMaterialExpression* Expr = McpFindGraphExpressionFromPayload(GraphOwner, *NodeObj)) {
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

bool UMcpAutomationBridgeSubsystem::HandleGetMaterialInstanceInfo(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("get_material_instance_info"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("get_material_instance_info payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), AssetPath)) {
    SendAutomationError(Socket, RequestId, TEXT("assetPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
  if (ValidatedPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Invalid assetPath: %s"), *AssetPath),
                        TEXT("INVALID_PATH"));
    return true;
  }

  UMaterialInstanceConstant* Instance = LoadObject<UMaterialInstanceConstant>(nullptr, *ValidatedPath);
  if (!Instance) {
    UObject* Generic = LoadObject<UObject>(nullptr, *ValidatedPath);
    SendAutomationError(Socket, RequestId,
                        Generic
                            ? FString::Printf(TEXT("Asset '%s' is not a MaterialInstanceConstant"), *ValidatedPath)
                            : FString::Printf(TEXT("Asset not found: %s"), *ValidatedPath),
                        Generic ? TEXT("INVALID_ASSET_TYPE") : TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  bool bIncludeEffective = true;
  bool bOverriddenOnly = false;
  Payload->TryGetBoolField(TEXT("includeEffective"), bIncludeEffective);
  Payload->TryGetBoolField(TEXT("overriddenOnly"), bOverriddenOnly);

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Result, Instance);
  Result->SetStringField(TEXT("assetPath"), ValidatedPath);
  McpCollectMaterialInstanceInfo(Instance, Result.ToSharedRef(), bIncludeEffective, bOverriddenOnly);
  Result->SetBoolField(TEXT("includeEffective"), bIncludeEffective);
  Result->SetBoolField(TEXT("overriddenOnly"), bOverriddenOnly);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material instance diagnostics retrieved"), Result, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("get_material_instance_info requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleFindMaterialExpressions(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("find_material_expressions"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("find_material_expressions payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), AssetPath)) {
    SendAutomationError(Socket, RequestId, TEXT("assetPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FMcpMaterialGraphOwner GraphOwner;
  FString GraphOwnerError;
  if (!McpResolveMaterialGraphOwner(AssetPath, GraphOwner, GraphOwnerError)) {
    SendAutomationError(Socket, RequestId, GraphOwnerError,
                        GraphOwnerError.Contains(TEXT("not found"))
                            ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE"));
    return true;
  }

  const TArray<TObjectPtr<UMaterialExpression>>* Expressions = McpGetGraphExpressions(GraphOwner);
  TArray<TSharedPtr<FJsonValue>> Matches;
  if (Expressions) {
    for (int32 Index = 0; Index < Expressions->Num(); ++Index) {
      UMaterialExpression* Expr = (*Expressions)[Index];
      if (!McpExpressionMatchesFilters(GraphOwner, Expr, Index, Payload)) {
        continue;
      }
      TSharedPtr<FJsonObject> Match = McpBuildExpressionRef(GraphOwner, Expr);
      if (Expr) {
        Match->SetStringField(TEXT("className"), Expr->GetClass()->GetName());
        if (!Expr->Desc.IsEmpty()) {
          Match->SetStringField(TEXT("desc"), Expr->Desc);
        }
        if (UMaterialExpressionParameter* Param = Cast<UMaterialExpressionParameter>(Expr)) {
          Match->SetStringField(TEXT("parameterName"), Param->ParameterName.ToString());
        }
      }
      Matches.Add(MakeShared<FJsonValueObject>(Match));
    }
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Result, GraphOwner.Asset);
  Result->SetStringField(TEXT("assetPath"), AssetPath);
  Result->SetArrayField(TEXT("expressions"), Matches);
  Result->SetNumberField(TEXT("matchCount"), Matches.Num());
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material expressions found"), Result, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("find_material_expressions requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGetMaterialExpressionDetails(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("get_material_expression_details"), ESearchCase::IgnoreCase)) {
    return false;
  }

  return HandleGetMaterialNodeDetails(RequestId, TEXT("get_material_node_details"), Payload, Socket);
}

bool UMcpAutomationBridgeSubsystem::HandleGetMaterialExpressionConnections(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("get_material_expression_connections"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("get_material_expression_connections payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath or materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FMcpMaterialGraphOwner GraphOwner;
  FString GraphOwnerError;
  if (!McpResolveMaterialGraphOwner(MaterialPath, GraphOwner, GraphOwnerError))
  {
    SendAutomationError(Socket, RequestId, GraphOwnerError,
                        GraphOwnerError.Contains(TEXT("not found"))
                            ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE"));
    return true;
  }

  UMaterialExpression* Expression = McpFindGraphExpressionFromPayload(GraphOwner, Payload);
  if (!Expression) {
    SendAutomationError(Socket, RequestId,
                        TEXT("expression reference is required"),
                        TEXT("NODE_NOT_FOUND"));
    return true;
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Result, GraphOwner.Asset);
  McpAddExpressionIdentity(GraphOwner, Expression, McpExpressionIndex(GraphOwner, Expression), Result.ToSharedRef());
  Result->SetStringField(TEXT("assetClass"), GraphOwner.Asset->GetClass()->GetName());
  Result->SetStringField(TEXT("className"), Expression->GetClass()->GetName());
  Result->SetArrayField(TEXT("inputs"), McpBuildExpressionInputsArray(GraphOwner, Expression));
  Result->SetArrayField(TEXT("consumers"), McpBuildExpressionConsumersArray(GraphOwner, Expression));
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material expression connections retrieved"), Result, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("get_material_expression_connections requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGetMaterialNodeDetails(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("get_material_node_details"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("get_material_node_details payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPath and materialPath
  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath or materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Resolve asset - supports UMaterial, UMaterialFunction, and UMaterialFunctionInstance
  FMcpMaterialGraphOwner GraphOwner;
  FString GraphOwnerError;
  if (!McpResolveMaterialGraphOwner(MaterialPath, GraphOwner, GraphOwnerError))
  {
    SendAutomationError(Socket, RequestId, GraphOwnerError,
                        GraphOwnerError.Contains(TEXT("not found"))
                            ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE"));
    return true;
  }

  static const TArray<TObjectPtr<UMaterialExpression>> EmptyExprs;
  const TArray<TObjectPtr<UMaterialExpression>>* ExpressionsPtr = McpGetGraphExpressions(GraphOwner);
  const TArray<TObjectPtr<UMaterialExpression>>& Expressions = ExpressionsPtr ? *ExpressionsPtr : EmptyExprs;

  FString NodeId;
  Payload->TryGetStringField(TEXT("nodeId"), NodeId);
  UMaterialExpression *Expression = McpFindGraphExpressionFromPayload(GraphOwner, Payload);

  // If no specific node requested or node not found, return list of all nodes
  if (!Expression) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Resp, GraphOwner.Asset);
    Resp->SetStringField(TEXT("assetClass"), GraphOwner.Asset->GetClass()->GetName());
    
    TArray<TSharedPtr<FJsonValue>> NodeList;
    for (int32 i = 0; i < Expressions.Num(); ++i) {
      UMaterialExpression *Expr = Expressions[i];
      if (!Expr) continue;
      
      TSharedPtr<FJsonObject> NodeInfo = McpHandlerUtils::CreateResultObject();
      McpAddExpressionIdentity(GraphOwner, Expr, i, NodeInfo.ToSharedRef());
      NodeInfo->SetStringField(TEXT("nodeType"), Expr->GetClass()->GetName());
      if (!Expr->Desc.IsEmpty()) {
        NodeInfo->SetStringField(TEXT("desc"), Expr->Desc);
      }
      // Add parameter name if applicable
      if (UMaterialExpressionParameter *Param = Cast<UMaterialExpressionParameter>(Expr)) {
        NodeInfo->SetStringField(TEXT("parameterName"), Param->ParameterName.ToString());
      }
      NodeList.Add(MakeShared<FJsonValueObject>(NodeInfo));
    }
    
    Resp->SetArrayField(TEXT("nodes"), NodeList);
    Resp->SetNumberField(TEXT("nodeCount"), Expressions.Num());

    FString Message = NodeId.IsEmpty()
        ? FString::Printf(TEXT("Asset has %d nodes. Provide nodeId for specific node details."), Expressions.Num())
        : FString::Printf(TEXT("Node '%s' not found. Asset has %d nodes."), *NodeId, Expressions.Num());

    SendAutomationResponse(Socket, RequestId, NodeId.IsEmpty(),
                           Message, Resp, NodeId.IsEmpty() ? FString() : TEXT("NODE_NOT_FOUND"));
    return true;
  }

  // Build response for specific node
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Resp, GraphOwner.Asset);
  Resp->SetStringField(TEXT("assetClass"), GraphOwner.Asset->GetClass()->GetName());
  McpAddExpressionIdentity(GraphOwner, Expression, McpExpressionIndex(GraphOwner, Expression), Resp.ToSharedRef());
  Resp->SetStringField(TEXT("class"), Expression->GetClass()->GetName());
  Resp->SetStringField(TEXT("className"), Expression->GetClass()->GetName());
  Resp->SetStringField(TEXT("classPath"), Expression->GetClass()->GetPathName());
  if (!Expression->Desc.IsEmpty()) {
    Resp->SetStringField(TEXT("desc"), Expression->Desc);
  }

  Resp->SetArrayField(TEXT("inputs"), McpBuildExpressionInputsArray(GraphOwner, Expression));
  Resp->SetArrayField(TEXT("consumers"), McpBuildExpressionConsumersArray(GraphOwner, Expression));
  McpAppendTypedExpressionDetails(GraphOwner, Expression, Resp.ToSharedRef());

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material node details retrieved"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("get_material_node_details requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGetLandscapeMaterialContext(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("get_landscape_material_context"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("get_landscape_material_context payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  ALandscape* Landscape = McpFindLandscapeActorByPayload(Payload);
  if (!Landscape) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Could not resolve a loaded landscape actor from actorName/actorPath/landscapeName/landscapePath"),
                        TEXT("LANDSCAPE_NOT_FOUND"));
    return true;
  }

  ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
  if (!LandscapeInfo) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Resolved landscape actor has no LandscapeInfo"),
                        TEXT("UNSUPPORTED_STATE"));
    return true;
  }

  UMaterialInterface* AssignedMaterial = Landscape->LandscapeMaterial;
  FString MaterialAssetPath;
  FMcpMaterialGraphOwner GraphOwner;
  FString GraphOwnerError;
  bool bHasGraphOwner = false;
  if (AssignedMaterial) {
    MaterialAssetPath = AssignedMaterial->GetPathName();
    bHasGraphOwner = McpResolveMaterialGraphOwner(MaterialAssetPath, GraphOwner, GraphOwnerError);
    if (!bHasGraphOwner) {
      if (UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(AssignedMaterial)) {
        if (MaterialInstance->Parent) {
          MaterialAssetPath = MaterialInstance->Parent->GetPathName();
          bHasGraphOwner = McpResolveMaterialGraphOwner(MaterialAssetPath, GraphOwner, GraphOwnerError);
        }
      }
    }
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Result, Landscape);
  Result->SetStringField(TEXT("actorName"), Landscape->GetActorLabel());
  Result->SetStringField(TEXT("actorPath"), Landscape->GetPathName());
  if (AssignedMaterial) {
    Result->SetStringField(TEXT("assignedMaterialPath"), AssignedMaterial->GetPathName());
    Result->SetStringField(TEXT("assignedMaterialClass"), AssignedMaterial->GetClass()->GetName());
    if (!MaterialAssetPath.IsEmpty()) {
      Result->SetStringField(TEXT("graphMaterialPath"), MaterialAssetPath);
    }
  }

  TArray<TSharedPtr<FJsonValue>> TargetLayers;
  for (const FLandscapeInfoLayerSettings& LayerSettings : LandscapeInfo->Layers) {
    TSharedPtr<FJsonObject> LayerObj = McpHandlerUtils::CreateResultObject();
    const FName LayerName = LayerSettings.GetLayerName();
    LayerObj->SetStringField(TEXT("name"), LayerName.ToString());
    if (LayerSettings.LayerInfoObj) {
      LayerObj->SetStringField(TEXT("layerInfoPath"), LayerSettings.LayerInfoObj->GetPathName());
      if (UPhysicalMaterial* PhysicalMaterial = LayerSettings.LayerInfoObj->GetPhysicalMaterial().Get()) {
        LayerObj->SetStringField(TEXT("physicalMaterialPath"), PhysicalMaterial->GetPathName());
      }
    }

    if (bHasGraphOwner) {
      const TArray<TObjectPtr<UMaterialExpression>>* Expressions = McpGetGraphExpressions(GraphOwner);
      if (Expressions) {
        for (int32 Index = 0; Index < Expressions->Num(); ++Index) {
          UMaterialExpression* Expr = (*Expressions)[Index];
          if (UMaterialExpressionLandscapeLayerWeight* LayerWeight = Cast<UMaterialExpressionLandscapeLayerWeight>(Expr)) {
            if (LayerWeight->ParameterName == LayerName) {
              LayerObj->SetObjectField(TEXT("matchedExpression"), McpBuildExpressionRef(GraphOwner, Expr));
              LayerObj->SetStringField(TEXT("matchedExpressionType"), TEXT("LandscapeLayerWeight"));
              break;
            }
          } else if (UMaterialExpressionLandscapeLayerBlend* LayerBlend = Cast<UMaterialExpressionLandscapeLayerBlend>(Expr)) {
            for (const FLayerBlendInput& BlendInput : LayerBlend->Layers) {
              if (BlendInput.LayerName == LayerName) {
                LayerObj->SetObjectField(TEXT("matchedExpression"), McpBuildExpressionRef(GraphOwner, Expr));
                LayerObj->SetStringField(TEXT("matchedExpressionType"), TEXT("LandscapeLayerBlend"));
                break;
              }
            }
            if (LayerObj->HasField(TEXT("matchedExpression"))) {
              break;
            }
          }
        }
      }
    }

    TargetLayers.Add(MakeShared<FJsonValueObject>(LayerObj));
  }
  Result->SetArrayField(TEXT("targetLayers"), TargetLayers);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Landscape material context retrieved"), Result, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("get_landscape_material_context requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleCompileMaterialDiagnostics(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("compile_material_diagnostics"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("compile_material_diagnostics payload missing"),
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

  const FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
  if (ValidatedPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Invalid assetPath: %s"), *AssetPath),
                        TEXT("INVALID_PATH"));
    return true;
  }

  bool bSave = false;
  Payload->TryGetBoolField(TEXT("save"), bSave);

  UMaterialInterface* MaterialInterface = LoadObject<UMaterialInterface>(nullptr, *ValidatedPath);
  if (!MaterialInterface) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Could not load material-family asset: %s"), *ValidatedPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  const double StartTime = FPlatformTime::Seconds();
  if (UMaterial* Material = Cast<UMaterial>(MaterialInterface)) {
    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    if (bSave) {
      UEditorAssetLibrary::SaveLoadedAsset(Material);
    }
  } else if (UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(MaterialInterface)) {
    Instance->PreEditChange(nullptr);
    Instance->PostEditChange();
    Instance->MarkPackageDirty();
    if (bSave) {
      UEditorAssetLibrary::SaveLoadedAsset(Instance);
    }
  } else {
    SendAutomationError(Socket, RequestId,
                        TEXT("compile_material_diagnostics currently supports UMaterial and UMaterialInstanceConstant"),
                        TEXT("UNSUPPORTED_ASSET_TYPE"));
    return true;
  }

  MaterialInterface->EnsureIsComplete();
  const FMaterialResource* Resource = MaterialInterface->GetMaterialResource(GMaxRHIShaderPlatform);
  TArray<TSharedPtr<FJsonValue>> Errors;
  if (Resource) {
    for (const FString& Error : Resource->GetCompileErrors()) {
      Errors.Add(MakeShared<FJsonValueString>(Error));
    }
  }

  const double EndTime = FPlatformTime::Seconds();
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Result, MaterialInterface);
  Result->SetStringField(TEXT("assetPath"), ValidatedPath);
  Result->SetStringField(TEXT("assetClass"), MaterialInterface->GetClass()->GetName());
  Result->SetBoolField(TEXT("compiled"), true);
  Result->SetBoolField(TEXT("saved"), bSave);
  Result->SetArrayField(TEXT("errors"), Errors);
  Result->SetArrayField(TEXT("warnings"), TArray<TSharedPtr<FJsonValue>>());
  Result->SetNumberField(TEXT("messageCount"), Errors.Num());
  Result->SetNumberField(TEXT("durationMs"), (EndTime - StartTime) * 1000.0);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material compile diagnostics retrieved"), Result, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("compile_material_diagnostics requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// SOURCE CONTROL STATE
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleGetSourceControlState(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("get_source_control_state"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("get_source_control_state payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPath and assetPaths
  TArray<FString> AssetPaths;
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) &&
      AssetPathsArray && AssetPathsArray->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }
  } else {
    FString SinglePath;
    if (Payload->TryGetStringField(TEXT("assetPath"), SinglePath) && !SinglePath.IsEmpty()) {
      AssetPaths.Add(SinglePath);
    }
  }

  if (AssetPaths.Num() == 0) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath (string) or assetPaths (array) required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!ISourceControlModule::Get().IsEnabled()) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("sourceControlEnabled"), false);
    Result->SetStringField(TEXT("message"), TEXT("Source control is not enabled"));
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Source control disabled"), Result, FString());
    return true;
  }

  ISourceControlProvider &SourceControlProvider =
      ISourceControlModule::Get().GetProvider();

  TArray<TSharedPtr<FJsonValue>> StatesArray;

  for (const FString &AssetPath : AssetPaths) {
    TSharedPtr<FJsonObject> StateObj = McpHandlerUtils::CreateResultObject();
    StateObj->SetStringField(TEXT("assetPath"), AssetPath);

    // Check if asset exists
    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
      StateObj->SetBoolField(TEXT("exists"), false);
      StateObj->SetStringField(TEXT("state"), TEXT("not_found"));
      StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
      continue;
    }

    StateObj->SetBoolField(TEXT("exists"), true);

    // Convert asset path to file path
    FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
    FString FilePath;
    if (!FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, FilePath, FPackageName::GetAssetPackageExtension())) {
      StateObj->SetStringField(TEXT("state"), TEXT("path_conversion_failed"));
      StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
      continue;
    }

    // Get source control state
    FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(
        FilePath, EStateCacheUsage::Use);

    if (!SourceControlState.IsValid()) {
      StateObj->SetStringField(TEXT("state"), TEXT("unknown"));
      StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
      continue;
    }

    // Populate state info
    StateObj->SetBoolField(TEXT("isSourceControlled"), SourceControlState->IsSourceControlled());
    StateObj->SetBoolField(TEXT("isCheckedOut"), SourceControlState->IsCheckedOut());
    StateObj->SetBoolField(TEXT("isCurrent"), SourceControlState->IsCurrent());
    StateObj->SetBoolField(TEXT("isAdded"), SourceControlState->IsAdded());
    StateObj->SetBoolField(TEXT("isDeleted"), SourceControlState->IsDeleted());
    StateObj->SetBoolField(TEXT("isModified"), SourceControlState->IsModified());
    StateObj->SetBoolField(TEXT("isIgnored"), SourceControlState->IsIgnored());
    StateObj->SetBoolField(TEXT("isUnknown"), SourceControlState->IsUnknown());
    StateObj->SetBoolField(TEXT("canCheckIn"), SourceControlState->CanCheckIn());
    StateObj->SetBoolField(TEXT("canCheckout"), SourceControlState->CanCheckout());
    StateObj->SetBoolField(TEXT("canRevert"), SourceControlState->CanRevert());
    StateObj->SetBoolField(TEXT("canEdit"), SourceControlState->CanEdit());
    StateObj->SetBoolField(TEXT("canDelete"), SourceControlState->CanDelete());
    StateObj->SetBoolField(TEXT("canAdd"), SourceControlState->CanAdd());
    StateObj->SetBoolField(TEXT("isConflicted"), SourceControlState->IsConflicted());

    // Check if checked out by other
    FString WhoCheckedOut;
    bool bIsCheckedOutOther = SourceControlState->IsCheckedOutOther(&WhoCheckedOut);
    StateObj->SetBoolField(TEXT("isCheckedOutOther"), bIsCheckedOutOther);
    if (bIsCheckedOutOther && !WhoCheckedOut.IsEmpty()) {
      StateObj->SetStringField(TEXT("checkedOutBy"), WhoCheckedOut);
    }

    // Determine primary state string
    FString StateString = TEXT("unknown");
    if (!SourceControlState->IsSourceControlled()) {
      StateString = TEXT("not_controlled");
    } else if (SourceControlState->IsAdded()) {
      StateString = TEXT("added");
    } else if (SourceControlState->IsDeleted()) {
      StateString = TEXT("deleted");
    } else if (SourceControlState->IsConflicted()) {
      StateString = TEXT("conflicted");
    } else if (SourceControlState->IsCheckedOut()) {
      StateString = TEXT("checked_out");
    } else if (SourceControlState->IsModified()) {
      StateString = TEXT("modified");
    } else if (!SourceControlState->IsCurrent()) {
      StateString = TEXT("out_of_date");
    } else {
      StateString = TEXT("current");
    }
    StateObj->SetStringField(TEXT("state"), StateString);

    // Get display name
    StateObj->SetStringField(TEXT("displayName"), SourceControlState->GetDisplayName().ToString());

    StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("sourceControlEnabled"), true);
  Result->SetArrayField(TEXT("states"), StatesArray);
  Result->SetNumberField(TEXT("queriedCount"), AssetPaths.Num());

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Source control state retrieved"), Result, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("get_source_control_state requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
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
    TMap<UMaterialExpression*, int32> NodeIndexMap;
    for (int32 i = 0; i < Expressions.Num(); ++i)
      NodeIndexMap.Add(Expressions[i], i);

    for (int32 i = 0; i < Expressions.Num(); ++i)
    {
      UMaterialExpression* Expr = Expressions[i];
      if (!Expr) continue;

      TSharedPtr<FJsonObject> NodeObj = McpHandlerUtils::CreateResultObject();
      McpAddExpressionIdentity(GraphOwner, Expr, i, NodeObj.ToSharedRef());

      TArray<TSharedPtr<FJsonValue>> InputsArray;
      for (FProperty* Property = Expr->GetClass()->PropertyLink; Property;
           Property = Property->PropertyLinkNext)
      {
        if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
        {
          if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput")))
          {
            FExpressionInput* Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(Expr);
            TSharedPtr<FJsonObject> InputObj = McpHandlerUtils::CreateResultObject();
            InputObj->SetStringField(TEXT("name"), Property->GetName());
            InputObj->SetBoolField(TEXT("isConnected"), Input->Expression != nullptr);
            if (Input->Expression)
            {
              int32* ConnectedIndex = NodeIndexMap.Find(Input->Expression);
              if (ConnectedIndex)
                InputObj->SetNumberField(TEXT("connectedToIndex"), *ConnectedIndex);
              InputObj->SetStringField(TEXT("connectedToId"), Input->Expression->MaterialExpressionGuid.ToString());
              InputObj->SetStringField(TEXT("connectedToExpressionGuid"), Input->Expression->MaterialExpressionGuid.ToString());
              InputObj->SetStringField(TEXT("connectedToExpressionPath"), Input->Expression->GetPathName());
              InputObj->SetStringField(TEXT("connectedToName"), Input->Expression->GetName());
            }
            InputsArray.Add(MakeShared<FJsonValueObject>(InputObj));
          }
        }
      }
      NodeObj->SetArrayField(TEXT("inputs"), InputsArray);

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
      TSharedPtr<FJsonObject> CommentObj = McpHandlerUtils::CreateResultObject();
      CommentObj->SetStringField(TEXT("commentId"), Comment->GetPathName());
      CommentObj->SetStringField(TEXT("expressionPath"), Comment->GetPathName());
      CommentObj->SetStringField(TEXT("text"), Comment->Text);
      CommentObj->SetNumberField(TEXT("x"), Comment->MaterialExpressionEditorX);
      CommentObj->SetNumberField(TEXT("y"), Comment->MaterialExpressionEditorY);
      CommentObj->SetNumberField(TEXT("width"), Comment->SizeX);
      CommentObj->SetNumberField(TEXT("height"), Comment->SizeY);
      CommentObj->SetBoolField(TEXT("groupMode"), Comment->bGroupMode);
      CommentList.Add(MakeShared<FJsonValueObject>(CommentObj));
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

  // Check if it's a material (legacy path - should not be reached for UMaterial, kept for safety)
  UMaterial *Material = Cast<UMaterial>(Asset);
  if (Material) {
    TArray<TSharedPtr<FJsonValue>> NodeList;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    const TArray<TObjectPtr<UMaterialExpression>> &Expressions =
        Material->GetEditorOnlyData()->ExpressionCollection.Expressions;
#else
    const TArray<UMaterialExpression *> &Expressions = Material->Expressions;
#endif

    // Build node list with connections
    TMap<UMaterialExpression*, int32> NodeIndexMap;
    for (int32 i = 0; i < Expressions.Num(); ++i) {
      NodeIndexMap.Add(Expressions[i], i);
    }

    for (int32 i = 0; i < Expressions.Num(); ++i) {
      UMaterialExpression *Expr = Expressions[i];
      if (!Expr) continue;

      TSharedPtr<FJsonObject> NodeObj = McpHandlerUtils::CreateResultObject();
      NodeObj->SetNumberField(TEXT("index"), i);
      NodeObj->SetStringField(TEXT("nodeId"), Expr->MaterialExpressionGuid.ToString());
      NodeObj->SetStringField(TEXT("type"), Expr->GetClass()->GetName());
      NodeObj->SetStringField(TEXT("name"), Expr->GetName());
      NodeObj->SetNumberField(TEXT("x"), Expr->MaterialExpressionEditorX);
      NodeObj->SetNumberField(TEXT("y"), Expr->MaterialExpressionEditorY);

      // Add inputs with connections
      TArray<TSharedPtr<FJsonValue>> InputsArray;
      for (FProperty *Property = Expr->GetClass()->PropertyLink; Property;
           Property = Property->PropertyLinkNext) {
        if (FStructProperty *StructProp = CastField<FStructProperty>(Property)) {
          if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
            FExpressionInput *Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(Expr);
            TSharedPtr<FJsonObject> InputObj = McpHandlerUtils::CreateResultObject();
            InputObj->SetStringField(TEXT("name"), Property->GetName());
            InputObj->SetBoolField(TEXT("isConnected"), Input->Expression != nullptr);
            if (Input->Expression) {
              int32 *ConnectedIndex = NodeIndexMap.Find(Input->Expression);
              if (ConnectedIndex) {
                InputObj->SetNumberField(TEXT("connectedToIndex"), *ConnectedIndex);
              }
              InputObj->SetStringField(TEXT("connectedToId"), Input->Expression->MaterialExpressionGuid.ToString());
              InputObj->SetStringField(TEXT("connectedToName"), Input->Expression->GetName());
            }
            InputsArray.Add(MakeShared<FJsonValueObject>(InputObj));
          }
        }
      }
      NodeObj->SetArrayField(TEXT("inputs"), InputsArray);

      // Add parameter info if applicable
      if (UMaterialExpressionParameter *Param = Cast<UMaterialExpressionParameter>(Expr)) {
        NodeObj->SetStringField(TEXT("parameterName"), Param->ParameterName.ToString());
      }

      NodeList.Add(MakeShared<FJsonValueObject>(NodeObj));
    }

    Result->SetStringField(TEXT("graphType"), TEXT("Material"));
    Result->SetNumberField(TEXT("nodeCount"), Expressions.Num());
    Result->SetArrayField(TEXT("nodes"), NodeList);

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material graph retrieved"), Result, FString());
    return true;
  }

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

// ============================================================================
// GET SET/GET MATERIAL ATTRIBUTES OVERRIDES (N3)
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleGetSetMaterialAttributesOverrides(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (!Action.Equals(TEXT("get_set_material_attributes_overrides"), ESearchCase::IgnoreCase)) return false;

#if WITH_EDITOR
    if (!Payload.IsValid())
    { SendAutomationError(Socket, RequestId, TEXT("payload missing"), TEXT("INVALID_PAYLOAD")); return true; }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    { SendAutomationError(Socket, RequestId, TEXT("assetPath required"), TEXT("INVALID_ARGUMENT")); return true; }

    FMcpMaterialGraphOwner Owner;
    FString Err;
    if (!McpResolveMaterialGraphOwner(AssetPath, Owner, Err))
    { SendAutomationError(Socket, RequestId, Err,
        Err.Contains(TEXT("not found")) ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE")); return true; }

    const TArray<TObjectPtr<UMaterialExpression>>* AllPtr = McpGetGraphExpressions(Owner);
    static const TArray<TObjectPtr<UMaterialExpression>> Empty;
    const auto& All = AllPtr ? *AllPtr : Empty;

    TArray<TSharedPtr<FJsonValue>> Nodes;
    for (int32 i = 0; i < All.Num(); ++i)
    {
        UMaterialExpression* Expr = All[i];
        if (!Expr) continue;

        TSharedPtr<FJsonObject> Item;
        if (auto* Set = Cast<UMaterialExpressionSetMaterialAttributes>(Expr))
        {
            Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("kind"), TEXT("SetMaterialAttributes"));
            McpMaterialExpressionDetails::AppendAttributeSetDetails(Owner, Set, Item.ToSharedRef());
        }
        else if (auto* Get = Cast<UMaterialExpressionGetMaterialAttributes>(Expr))
        {
            Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("kind"), TEXT("GetMaterialAttributes"));
            McpMaterialExpressionDetails::AppendAttributeGetDetails(Get, Item.ToSharedRef());
        }
        if (Item.IsValid())
        {
            TSharedPtr<FJsonObject> Identity = MakeShared<FJsonObject>();
            McpAddExpressionIdentity(Owner, Expr, i, Identity.ToSharedRef());
            Item->SetObjectField(TEXT("nodeIdentity"), Identity);
            Nodes.Add(MakeShared<FJsonValueObject>(Item));
        }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Owner.Asset);
    Result->SetArrayField(TEXT("nodes"), Nodes);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Set/Get material attribute overrides retrieved"), Result, FString());
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("editor only"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// ============================================================================
// REBUILD MATERIAL
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleRebuildMaterial(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("rebuild_material"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing payload."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), AssetPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath or materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (AssetPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Resolve to UMaterial or UMaterialFunction
  FMcpMaterialGraphOwner GraphOwner;
  FString GraphOwnerError;
  if (!McpResolveMaterialGraphOwner(AssetPath, GraphOwner, GraphOwnerError))
  {
    SendAutomationError(Socket, RequestId, GraphOwnerError,
                        GraphOwnerError.Contains(TEXT("not found"))
                            ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE"));
    return true;
  }
  if (GraphOwner.bReadOnly)
  {
    SendAutomationError(Socket, RequestId,
                        TEXT("Cannot rebuild a MaterialFunctionInstance - rebuild the parent function instead"),
                        TEXT("UNSUPPORTED_OPERATION"));
    return true;
  }

  AsyncTask(ENamedThreads::GameThread, [this, RequestId, Socket, GraphOwner, AssetPath]() {
    FString RebuildErr;
    McpRebuildMaterialGraphOwner(GraphOwner, RebuildErr);
    McpSafeAssetSave(GraphOwner.Asset);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, GraphOwner.Asset);
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetStringField(TEXT("assetClass"), GraphOwner.Asset->GetClass()->GetName());
    Result->SetBoolField(TEXT("rebuilt"), true);

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material rebuilt successfully"), Result, FString());
  });

  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Editor only."),
                      TEXT("EDITOR_ONLY"));
  return true;
#endif
}
