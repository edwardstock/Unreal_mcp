// =============================================================================
// McpAutomationBridge_AssetWorkflowHandlers.cpp
// =============================================================================
// manage_asset action dispatcher and shared material-graph helpers.
//
// G.2: Following the manage_asset/manage_material redesign, this file is now a
// thin dispatcher. The 26 canonical action handlers for manage_asset live in
// per-domain Asset_*.cpp files (Asset_CRUD, Asset_Search, Asset_Import,
// Asset_Metadata, Asset_Dependencies, Asset_SourceControl, Asset_Thumbnails,
// Asset_Validation, Asset_Reports, Asset_NaniteOps).
//
// Some legacy material graph helpers still live here because older
// Material_*.cpp translation units forward into them. The old native single-pin
// connect path was removed; connect_material_pins is owned only by
// McpHandle_ConnectMaterialPins in Material_GraphWrites.cpp.
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
#include "McpFunctionInputTypeName.h"
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

  // Canonical spec field: "identifier" — mixed-type number (index) or string (GUID/name/path)
  const TSharedPtr<FJsonValue> IdentifierField = Payload->TryGetField(TEXT("identifier"));
  if (IdentifierField.IsValid())
  {
    if (IdentifierField->Type == EJson::Number)
    {
      return McpFindGraphExpression(Owner, FString(), (int32)IdentifierField->AsNumber());
    }
    if (IdentifierField->Type == EJson::String)
    {
      const FString Str = IdentifierField->AsString();
      // numeric string is ambiguous; reject by returning nullptr (caller produces NODE_NOT_FOUND)
      if (!Str.IsEmpty() && !Str.IsNumeric())
      {
        return McpFindGraphExpression(Owner, Str);
      }
    }
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

// External: canonical undecorated pin name (defined in GraphWrites.cpp).
extern FString McpGetUndecoratedInputName(UMaterialExpression* Expr, int32 InputIndex);

// Instance-based pin lookup. Iterates FExpressionInputIterator so dynamic-input
// nodes (UMaterialExpressionMaterialFunctionCall::FunctionInputs, etc.) are
// visible. Accepted handle forms per pin: canonical (undecorated GetInputName),
// decorated GetInputName, FExpressionInput::InputName, UPROPERTY name, numeric
// index. On match, InOutInputName is set to the canonical form for echo.
// On miss (and empty input name), defaults to pin 0 (legacy behavior).
FExpressionInput* McpFindExpressionInputByName(
    UMaterialExpression* Expression,
    FString& InOutInputName)
{
  if (!Expression)
  {
    return nullptr;
  }

  // Empty name -> pin 0 (legacy contract: callers may pass empty meaning "default").
  if (InOutInputName.IsEmpty())
  {
    FExpressionInput* Pin0 = Expression->GetInput(0);
    if (Pin0)
    {
      InOutInputName = McpGetUndecoratedInputName(Expression, 0);
    }
    return Pin0;
  }

  // Build parallel UPROPERTY-name map for nodes whose pins back static fields.
  TMap<const FExpressionInput*, FString> PtrToPropName;
  const FName ExprInputName(TEXT("ExpressionInput"));
  for (FProperty* Property = Expression->GetClass()->PropertyLink;
       Property; Property = Property->PropertyLinkNext)
  {
    FStructProperty* StructProp = CastField<FStructProperty>(Property);
    if (!StructProp || !StructProp->Struct) continue;
    bool bIsExprInput = false;
    for (UStruct* S = StructProp->Struct; S; S = S->GetSuperStruct())
    {
      if (S->GetFName() == ExprInputName) { bIsExprInput = true; break; }
    }
    if (!bIsExprInput) continue;
    const FExpressionInput* P = StructProp->ContainerPtrToValuePtr<FExpressionInput>(Expression);
    PtrToPropName.Add(P, Property->GetName());
  }

  for (FExpressionInputIterator It{ Expression }; It; ++It)
  {
    FExpressionInput* In = It.Input;
    if (!In) continue;

    const FString Canonical  = McpGetUndecoratedInputName(Expression, It.Index);
    const FString Decorated  = Expression->GetInputName(It.Index).ToString();
    const FString InstanceNm = In->InputName.ToString();
    const FString* PropName  = PtrToPropName.Find(In);
    const FString IndexStr   = FString::FromInt(It.Index);

    if (Canonical.Equals(InOutInputName, ESearchCase::IgnoreCase) ||
        Decorated.Equals(InOutInputName, ESearchCase::IgnoreCase) ||
        (!InstanceNm.IsEmpty() && InstanceNm.Equals(InOutInputName, ESearchCase::IgnoreCase)) ||
        (PropName && !PropName->IsEmpty()
            && PropName->Equals(InOutInputName, ESearchCase::IgnoreCase)) ||
        IndexStr.Equals(InOutInputName))
    {
      InOutInputName = Canonical;
      return In;
    }
  }

  return nullptr;
}

FIntPoint McpEstimateExpressionSize(UMaterialExpression* Expr)
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

// Note: this function was previously static; some material handlers were
// moved to sibling TUs (Material_NamedReroutes.cpp etc.) and now call it via
// extern declaration. To avoid default-argument redefinition under unity
// builds, all callers (in this TU and others) pass StepY explicitly.
FIntPoint McpFindFreePosition(
    const FMcpMaterialGraphOwner& Owner,
    const FIntPoint& Start,
    const FIntPoint& Size,
    int32 StepY)
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

// Note: this function was previously static; some material handlers were moved
// to sibling TUs (Material_NamedReroutes.cpp etc.) and now call it via extern
// declaration.
FIntPoint McpResolvePlacement(
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
  return McpFindFreePosition(Owner, Start, Size, 180);
}

bool McpAddExpressionToGraph(const FMcpMaterialGraphOwner& Owner, UMaterialExpression* Expression)
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

bool McpAddCommentToGraph(const FMcpMaterialGraphOwner& Owner, UMaterialExpressionComment* Comment)
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

// G.2: was static; promoted to TU-external so Asset_Dependencies.cpp can call it.
TConstArrayView<TObjectPtr<UMaterialExpressionComment>> McpGetGraphComments(const FMcpMaterialGraphOwner& Owner)
{
  if (Owner.Kind == EMcpMaterialGraphOwnerKind::Material)
  {
    return CastChecked<UMaterial>(Owner.GraphSource)->GetEditorComments();
  }
  return CastChecked<UMaterialFunction>(Owner.GraphSource)->GetEditorComments();
}

// Build a unified JSON shape for a UMaterialExpressionComment.
// Emits both legacy keys (text, commentId, expressionPath) and the wider keys
// added by the 2026-05-06 readback iteration (commentText, nodeId, expressionGuid,
// type, className, editorX/Y, desc, fontSize, commentColor). Two API surfaces
// (find_material_expressions and get_asset_graph) consume comments and historically
// emitted divergent shapes; this helper is the single source of truth.
// Pass IndexHint >= 0 to add `index` and `expressionIndex` fields (useful when
// appending comments into a flat list); pass INDEX_NONE to skip them.
// G.2: was static; promoted to TU-external so Asset_Dependencies.cpp can call it.
TSharedRef<FJsonObject> McpBuildCommentJson(UMaterialExpressionComment* Comment, int32 IndexHint)
{
  TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
  if (!Comment) { return Item; }

  if (IndexHint >= 0)
  {
    Item->SetNumberField(TEXT("index"),           IndexHint);
    Item->SetNumberField(TEXT("expressionIndex"), IndexHint);
  }

  // Identifiers: emit both forms so callers reading either API see what they expect.
  // commentId/expressionPath = full path (legacy get_asset_graph shape).
  // nodeId/expressionGuid    = MaterialExpressionGuid (find_material_expressions shape).
  Item->SetStringField(TEXT("nodeId"),         Comment->MaterialExpressionGuid.ToString());
  Item->SetStringField(TEXT("expressionGuid"), Comment->MaterialExpressionGuid.ToString());
  Item->SetStringField(TEXT("commentId"),      Comment->GetPathName());
  Item->SetStringField(TEXT("expressionPath"), Comment->GetPathName());
  Item->SetStringField(TEXT("expressionName"), Comment->GetName());
  Item->SetStringField(TEXT("name"),           Comment->GetName());
  Item->SetStringField(TEXT("type"),           TEXT("MaterialExpressionComment"));
  Item->SetStringField(TEXT("className"),      TEXT("MaterialExpressionComment"));

  // Position and size; editorX/Y are aliases of x/y kept for symmetry with other expressions.
  Item->SetNumberField(TEXT("x"),       Comment->MaterialExpressionEditorX);
  Item->SetNumberField(TEXT("y"),       Comment->MaterialExpressionEditorY);
  Item->SetNumberField(TEXT("editorX"), Comment->MaterialExpressionEditorX);
  Item->SetNumberField(TEXT("editorY"), Comment->MaterialExpressionEditorY);
  Item->SetNumberField(TEXT("width"),   Comment->SizeX);
  Item->SetNumberField(TEXT("height"),  Comment->SizeY);

  // Text content: `text` is the legacy key, `commentText` the newer one; emit both.
  Item->SetStringField(TEXT("text"),        Comment->Text);
  Item->SetStringField(TEXT("commentText"), Comment->Text);
  Item->SetStringField(TEXT("desc"),        Comment->Desc);

  // Display attributes
  Item->SetBoolField  (TEXT("groupMode"), Comment->bGroupMode);
  Item->SetNumberField(TEXT("fontSize"),  Comment->FontSize);

  TSharedRef<FJsonObject> Color = MakeShared<FJsonObject>();
  Color->SetNumberField(TEXT("r"), Comment->CommentColor.R);
  Color->SetNumberField(TEXT("g"), Comment->CommentColor.G);
  Color->SetNumberField(TEXT("b"), Comment->CommentColor.B);
  Color->SetNumberField(TEXT("a"), Comment->CommentColor.A);
  Item->SetObjectField(TEXT("commentColor"), Color);

  return Item;
}

UMaterialExpressionNamedRerouteDeclaration* McpFindNamedRerouteDeclaration(
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
            McpEmitInputPinJson(Owner, Input, Property->GetName(), ConsumerObj.ToSharedRef());
            Consumers.Add(MakeShared<FJsonValueObject>(ConsumerObj));
          }
        }
      }
    }
  }

  return Consumers;
}

// E.1: McpExpressionMatchesFilters moved to McpAutomationBridge_Material_GraphReads.cpp
// (extended with parameterGroup/samplerType/referencesTexture/isOrphan filters).

static void McpAppendTypedExpressionDetails(
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpression* Expression,
    const TSharedRef<FJsonObject>& Resp)
{
    McpMaterialExpressionDetails::AppendTypedDetails(Owner, Expression, Resp);
}

// Build the JSON object describing one material expression - assetClass, identity,
// className/classPath/desc, inputs, consumers (optional), and type-specific details.
// Used by HandleGetMaterialNodeDetails (single-node) and HandleBulkGetMaterialExpressionDetails.
static TSharedPtr<FJsonObject> McpBuildExpressionDetailsObject(
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpression* Expression,
    bool bIncludeConsumers)
{
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    if (!Expression) return Resp;

    McpHandlerUtils::AddVerification(Resp, Owner.Asset);
    Resp->SetStringField(TEXT("assetClass"), Owner.Asset->GetClass()->GetName());
    McpAddExpressionIdentity(Owner, Expression, McpExpressionIndex(Owner, Expression), Resp.ToSharedRef());
    Resp->SetStringField(TEXT("class"), Expression->GetClass()->GetName());
    Resp->SetStringField(TEXT("className"), Expression->GetClass()->GetName());
    Resp->SetStringField(TEXT("classPath"), Expression->GetClass()->GetPathName());
    Resp->SetStringField(TEXT("desc"), Expression->Desc);

    Resp->SetArrayField(TEXT("inputs"), McpBuildExpressionInputsArray(Owner, Expression));
    if (bIncludeConsumers)
    {
        Resp->SetArrayField(TEXT("consumers"), McpBuildExpressionConsumersArray(Owner, Expression));
    }
    McpAppendTypedExpressionDetails(Owner, Expression, Resp.ToSharedRef());
    return Resp;
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

  // When the wrapping action is the generic "manage_asset" tool, the payload
  // carries the canonical `subAction` field. The legacy `action` alias is a
  // hard-break path for the material-tools redesign.
  if (Lower == TEXT("manage_asset")) {
    if (!Payload.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing payload for manage_asset"),
                          TEXT("INVALID_PAYLOAD"));
      return true;
    }
    FString SubAction;
    if (!Payload->TryGetStringField(TEXT("subAction"), SubAction) ||
        SubAction.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing 'subAction' for manage_asset"),
                          TEXT("MISSING_SUB_ACTION"));
      return true;
    }
    Lower = SubAction.ToLower();
  }

  if (Lower.IsEmpty())
    return false;

  // Dispatch to specific handlers.
  //
  // G.2: The 26 manage_asset actions use canonical plural names per spec sec 4.
  // Old singular/aliased names (delete, duplicate, rename, list, etc.) are no
  // longer accepted (hard-break per spec sec 11). All material-graph dispatch
  // entries previously routed here now live in manage_material (handlers in
  // Material_*.cpp domain files).

  // CRUD
  if (Lower == TEXT("delete_assets"))
    return HandleDeleteAssets(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("duplicate_assets"))
    return HandleDuplicateAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("rename_assets"))
    return HandleRenameAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("move_assets"))
    return HandleMoveAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_folders"))
    return HandleCreateFolder(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("assets_exist"))
    return HandleDoesAssetExist(RequestId, Payload, RequestingSocket);

  // Search
  if (Lower == TEXT("list_assets"))
    return HandleListAssets(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("search_assets"))
    return HandleSearchAssets(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("find_assets_by_tag"))
    return HandleFindByTag(RequestId, Lower, Payload, RequestingSocket);

  // Import / generation
  if (Lower == TEXT("import_assets"))
    return HandleImportAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("generate_lods"))
    return HandleGenerateLODs(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("create_render_targets"))
    return HandleCreateRenderTarget(RequestId, Lower, Payload, RequestingSocket);

  // Metadata / tags
  if (Lower == TEXT("get_assets_metadata"))
    return HandleGetMetadata(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("set_assets_metadata"))
    return HandleSetMetadata(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("set_assets_tags"))
    return HandleSetTags(RequestId, Payload, RequestingSocket);

  // Dependency analysis
  if (Lower == TEXT("get_assets_dependencies"))
    return HandleGetDependencies(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("get_assets_graph"))
    return HandleGetAssetGraph(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("analyze_assets_graph"))
    return HandleAnalyzeGraph(RequestId, Lower, Payload, RequestingSocket);

  // Source control
  if (Lower == TEXT("source_control_checkout_assets"))
    return HandleSourceControlCheckout(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("source_control_submit_assets"))
    return HandleSourceControlSubmit(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("get_assets_source_control_state"))
    return HandleGetSourceControlState(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("source_control_enable"))
    return HandleSourceControlEnable(RequestId, Lower, Payload, RequestingSocket);

  // Thumbnails / Nanite / validation / reports / redirectors
  if (Lower == TEXT("create_thumbnails"))
    return HandleGenerateThumbnail(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("nanite_rebuild_meshes"))
    return HandleNaniteRebuildMesh(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("validate_assets"))
    return HandleValidateAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("generate_assets_report"))
    return HandleGenerateReport(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("fixup_redirectors"))
    return HandleFixupRedirectors(RequestId, Lower, Payload, RequestingSocket);

  // If the original Action was "manage_asset" the dispatcher already extracted
  // a subAction from the payload; reaching this point means the subAction is
  // genuinely unknown and no other handler will pick it up. Emit an explicit
  // error rather than letting the request silently time out.
  if (Action.ToLower() == TEXT("manage_asset"))
  {
    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Unknown subAction '%s' for manage_asset"), *Lower),
        TEXT("UNKNOWN_SUB_ACTION"));
    return true;
  }

  return false;
}

// ============================================================================
// MATERIAL-GRAPH HANDLERS (kept here because other Material_*.cpp TUs forward
// into them; the dispatcher entries themselves moved to manage_material in G.2).
// ============================================================================

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

// HandleSetMaterialNodePositions - moved to McpAutomationBridge_Material_NodePositioning.cpp (D.1)

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

// HandleCreateMaterialComment       - moved to McpAutomationBridge_Material_Comments.cpp (D.2)
// HandleWrapMaterialNodesInComment   - moved to McpAutomationBridge_Material_Comments.cpp (D.2)

// HandleCreateNamedReroute                    - moved to McpAutomationBridge_Material_NamedReroutes.cpp (D.2)
// HandleUseNamedReroute                       - moved to McpAutomationBridge_Material_NamedReroutes.cpp (D.2)
// HandleReplaceLongConnectionWithNamedReroute - moved to McpAutomationBridge_Material_NamedReroutes.cpp (D.2)
// HandleAlignMaterialNodes                    - moved to McpAutomationBridge_Material_NodePositioning.cpp (D.1)

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
// BULK GET MATERIAL EXPRESSION DETAILS (R10)
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleBulkGetMaterialExpressionDetails(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (!Action.Equals(TEXT("bulk_get_material_expression_details"), ESearchCase::IgnoreCase)) return false;

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

    bool bIncludeConsumers = false;
    Payload->TryGetBoolField(TEXT("includeConsumers"), bIncludeConsumers);

    const TArray<TSharedPtr<FJsonValue>>* IndicesPtr = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* GuidsPtr = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* NodeIdsPtr = nullptr;
    Payload->TryGetArrayField(TEXT("indices"), IndicesPtr);
    Payload->TryGetArrayField(TEXT("guids"), GuidsPtr);
    Payload->TryGetArrayField(TEXT("nodeIds"), NodeIdsPtr);

    int32 KindCount = (IndicesPtr ? 1 : 0) + (GuidsPtr ? 1 : 0) + (NodeIdsPtr ? 1 : 0);
    if (KindCount != 1)
    {
        SendAutomationError(Socket, RequestId,
                            TEXT("exactly one of indices/guids/nodeIds is required"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    auto AppendItem = [&](const TSharedPtr<FJsonObject>& Identifier, const TSharedPtr<FJsonObject>& SyntheticPayload, TArray<TSharedPtr<FJsonValue>>& Results, int32& OkCount, int32& ErrorCount)
    {
        UMaterialExpression* Expr = McpFindGraphExpressionFromPayload(Owner, SyntheticPayload);
        TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetObjectField(TEXT("identifier"), Identifier);
        if (!Expr)
        {
            Item->SetStringField(TEXT("error"), TEXT("NODE_NOT_FOUND"));
            Item->SetStringField(TEXT("message"), TEXT("expression not found in graph"));
            ++ErrorCount;
        }
        else
        {
            Item->SetObjectField(TEXT("details"),
                                 McpBuildExpressionDetailsObject(Owner, Expr, bIncludeConsumers));
            ++OkCount;
        }
        Results.Add(MakeShared<FJsonValueObject>(Item));
    };

    TArray<TSharedPtr<FJsonValue>> Results;
    int32 OkCount = 0, ErrorCount = 0;

    if (IndicesPtr)
    {
        if (IndicesPtr->Num() == 0)
        { SendAutomationError(Socket, RequestId, TEXT("indices[] is empty"), TEXT("INVALID_ARGUMENT")); return true; }

        for (const TSharedPtr<FJsonValue>& V : *IndicesPtr)
        {
            int32 Idx = INDEX_NONE;
            if (V.IsValid() && V->TryGetNumber(Idx))
            {
                TSharedPtr<FJsonObject> Identifier = MakeShared<FJsonObject>();
                Identifier->SetNumberField(TEXT("index"), Idx);

                TSharedPtr<FJsonObject> Syn = MakeShared<FJsonObject>();
                Syn->SetNumberField(TEXT("expressionIndex"), Idx);
                AppendItem(Identifier, Syn, Results, OkCount, ErrorCount);
            }
        }
    }
    else if (GuidsPtr)
    {
        if (GuidsPtr->Num() == 0)
        { SendAutomationError(Socket, RequestId, TEXT("guids[] is empty"), TEXT("INVALID_ARGUMENT")); return true; }

        for (const TSharedPtr<FJsonValue>& V : *GuidsPtr)
        {
            FString Guid;
            if (V.IsValid() && V->TryGetString(Guid))
            {
                TSharedPtr<FJsonObject> Identifier = MakeShared<FJsonObject>();
                Identifier->SetStringField(TEXT("guid"), Guid);

                TSharedPtr<FJsonObject> Syn = MakeShared<FJsonObject>();
                Syn->SetStringField(TEXT("expressionGuid"), Guid);
                AppendItem(Identifier, Syn, Results, OkCount, ErrorCount);
            }
        }
    }
    else // NodeIdsPtr
    {
        if (NodeIdsPtr->Num() == 0)
        { SendAutomationError(Socket, RequestId, TEXT("nodeIds[] is empty"), TEXT("INVALID_ARGUMENT")); return true; }

        for (const TSharedPtr<FJsonValue>& V : *NodeIdsPtr)
        {
            FString NodeId;
            if (V.IsValid() && V->TryGetString(NodeId))
            {
                TSharedPtr<FJsonObject> Identifier = MakeShared<FJsonObject>();
                Identifier->SetStringField(TEXT("nodeId"), NodeId);

                TSharedPtr<FJsonObject> Syn = MakeShared<FJsonObject>();
                Syn->SetStringField(TEXT("nodeId"), NodeId);
                AppendItem(Identifier, Syn, Results, OkCount, ErrorCount);
            }
        }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Owner.Asset);
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetArrayField(TEXT("results"), Results);
    Result->SetNumberField(TEXT("okCount"), OkCount);
    Result->SetNumberField(TEXT("errorCount"), ErrorCount);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Bulk material expression details retrieved"), Result, FString());
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("editor only"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
