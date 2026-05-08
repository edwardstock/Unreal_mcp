// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_Material_GraphReads.cpp
//
// Task E.1 - find_material_expressions consolidated read API.
// Plan: docs/superpowers/plans/2026-05-07-mcp-material-tools-redesign.md
// Spec: docs/superpowers/specs/2026-05-07-mcp-material-tools-redesign-design.md (sec 6)
//
// Subsumes the legacy find_material_expressions / get_material_expression_details /
// bulk_get_material_expression_details / get_material_expression_connections
// surfaces. Adds:
//   - mixed identifiers[] resolution (number index, GUID, expression name, parameter name)
//   - filters: parameterGroup, samplerType, referencesTexture, isOrphan
//   - pagination (limit/offset, max 500 with silent clamp + warnings[])
//   - reflection-driven details[] (catalog-derived non-semantic fields)
//   - top-level connections[] in symmetric add_material_nodes shape
//   - per-expression consumers[] (when includeConsumers)
//   - orphan detection via backward BFS from sinks (root pins / function outputs)
//   - notFound[] per-identifier failures with did-you-mean suggestions

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Internationalization/Regex.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/Class.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpMaterialExpressionCatalog.h"
#include "McpAutomationBridge_MaterialExpressionDetails.h"

#if WITH_EDITOR

#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "MaterialShared.h"
#include "SceneTypes.h"
#include "Engine/Texture.h"

// =============================================================================
// Externs into the shared helpers in McpAutomationBridge_AssetWorkflowHandlers.cpp.
// These remain there as the canonical definitions; this TU only consumes them.
// =============================================================================
extern int32 McpExpressionIndex(const FMcpMaterialGraphOwner& Owner, const UMaterialExpression* Expr);
extern void McpAddExpressionIdentity(
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpression* Expr,
    int32 Index,
    const TSharedRef<FJsonObject>& Obj);
extern TSharedPtr<FJsonObject> McpBuildExpressionRef(
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpression* Expression);
extern FString McpGetOutputName(UMaterialExpression* Expression, int32 OutputIndex, bool& bOutResolved);

#endif // WITH_EDITOR

#if WITH_EDITOR

namespace
{

// =============================================================================
// Filters helper (extension of the legacy McpExpressionMatchesFilters that lived
// in AssetWorkflowHandlers.cpp:828). Walks all spec-§6 filter fields with AND
// semantics. The legacy fields (className, parameterName, expressionName, desc,
// expressionPath, expressionGuid, expressionIndex, nodeId) are preserved; new
// E.1 filters (parameterGroup, samplerType, referencesTexture, isOrphan) are
// added.
// =============================================================================
bool McpExpressionMatchesFilters(
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpression* Expr,
    int32 Index,
    const TSharedPtr<FJsonObject>& Payload,
    const TSet<UMaterialExpression*>* OrphanSet)
{
    if (!Expr || !Payload.IsValid())
    {
        return false;
    }

    // --- legacy filters preserved ---
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
        const UMaterialExpressionTextureSampleParameter* TexParam =
            Cast<UMaterialExpressionTextureSampleParameter>(Expr);
        bool bMatch = false;
        if (Param)
        {
            bMatch = Param->ParameterName.ToString().Contains(ParameterName, ESearchCase::IgnoreCase);
        }
        else if (TexParam)
        {
            bMatch = TexParam->ParameterName.ToString().Contains(ParameterName, ESearchCase::IgnoreCase);
        }
        if (!bMatch)
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
    if ((Payload->TryGetStringField(TEXT("expressionGuid"), Guid) ||
         Payload->TryGetStringField(TEXT("nodeId"), Guid)) &&
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

    // --- E.1 new filters ---
    FString GroupFilter;
    if (Payload->TryGetStringField(TEXT("parameterGroup"), GroupFilter) && !GroupFilter.IsEmpty())
    {
        FString Group;
        if (UMaterialExpressionParameter* Param = Cast<UMaterialExpressionParameter>(Expr))
        {
            Group = Param->Group.ToString();
        }
        else if (UMaterialExpressionTextureSampleParameter* TexParam =
                     Cast<UMaterialExpressionTextureSampleParameter>(Expr))
        {
            Group = TexParam->Group.ToString();
        }
        if (Group.IsEmpty() || !Group.Contains(GroupFilter, ESearchCase::IgnoreCase))
        {
            return false;
        }
    }

    FString SamplerTypeFilter;
    if (Payload->TryGetStringField(TEXT("samplerType"), SamplerTypeFilter) && !SamplerTypeFilter.IsEmpty())
    {
        // exact match against canonical enum (e.g., "VirtualLinearColor")
        UMaterialExpressionTextureBase* TexBase = Cast<UMaterialExpressionTextureBase>(Expr);
        if (!TexBase)
        {
            return false;
        }
        // Look up the enum value name and strip the SAMPLERTYPE_ prefix.
        const UEnum* SamplerEnum = StaticEnum<EMaterialSamplerType>();
        if (!SamplerEnum)
        {
            return false;
        }
        const FString Raw = SamplerEnum->GetNameStringByValue(static_cast<int64>(TexBase->SamplerType));
        FString Stripped = Raw;
        const FString Prefix = TEXT("SAMPLERTYPE_");
        if (Stripped.StartsWith(Prefix))
        {
            Stripped.MidInline(Prefix.Len());
        }
        if (!Stripped.Equals(SamplerTypeFilter, ESearchCase::CaseSensitive))
        {
            return false;
        }
    }

    FString ReferencesTexture;
    if (Payload->TryGetStringField(TEXT("referencesTexture"), ReferencesTexture) && !ReferencesTexture.IsEmpty())
    {
        UMaterialExpressionTextureBase* TexBase = Cast<UMaterialExpressionTextureBase>(Expr);
        if (!TexBase || !TexBase->Texture)
        {
            return false;
        }
        if (!TexBase->Texture->GetPathName().Equals(ReferencesTexture, ESearchCase::IgnoreCase))
        {
            return false;
        }
    }

    bool bIsOrphanFilter = false;
    if (Payload->TryGetBoolField(TEXT("isOrphan"), bIsOrphanFilter))
    {
        if (!OrphanSet)
        {
            // Graph could not classify orphans; reject when filter requested.
            return false;
        }
        const bool bIsOrphan = OrphanSet->Contains(Expr);
        if (bIsOrphan != bIsOrphanFilter)
        {
            return false;
        }
    }

    return true;
}

// =============================================================================
// Decorative-class predicate. Matches the spec rule for omitting `isOrphan`:
// MaterialExpressionComment, MaterialExpressionNamedRerouteDeclaration, and
// any named-reroute usage. The first cannot reach here (Comments live in a
// separate collection), but the predicate is symmetric with the filter side.
// =============================================================================
bool McpIsDecorativeForOrphanLabel(const UMaterialExpression* Expr)
{
    if (!Expr) return true;
    if (Expr->IsA<UMaterialExpressionComment>()) return true;
    if (Expr->IsA<UMaterialExpressionNamedRerouteDeclaration>()) return true;
    if (Expr->IsA<UMaterialExpressionNamedRerouteUsage>()) return true;
    return false;
}

// =============================================================================
// Walk all FExpressionInput-derived properties on Expr; for each connected one,
// invoke `Visit` with (PinName, OutputIndex, SourceExpression).
// =============================================================================
template <typename FnT>
void McpForEachExpressionInputConnection(UMaterialExpression* Expr, FnT Visit)
{
    if (!Expr) return;
    const FName ExprInputName(TEXT("ExpressionInput"));
    for (FProperty* Property = Expr->GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext)
    {
        FStructProperty* StructProp = CastField<FStructProperty>(Property);
        if (!StructProp || !StructProp->Struct) continue;

        // Walk super chain for FExpressionInput ancestry (matches the
        // detection used in break/connect flows).
        bool bIsExprInput = false;
        for (UStruct* S = StructProp->Struct; S; S = S->GetSuperStruct())
        {
            if (S->GetFName() == ExprInputName) { bIsExprInput = true; break; }
        }
        if (!bIsExprInput) continue;

        FExpressionInput* Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(Expr);
        if (!Input || !Input->Expression) continue;
        Visit(Property->GetName(), Input->OutputIndex, Input->Expression);
    }
}

// =============================================================================
// Material root pin sinks. These are the connected inputs on the UMaterial root
// (BaseColor, Metallic, ...). Each connected pin contributes (PinName, OutputIndex,
// SourceExpression) for orphan-BFS seeding and connections[] emission.
// =============================================================================
static const TCHAR* const GMaterialRootPinNames[] = {
    TEXT("BaseColor"),
    TEXT("Metallic"),
    TEXT("Specular"),
    TEXT("Roughness"),
    TEXT("Anisotropy"),
    TEXT("EmissiveColor"),
    TEXT("Opacity"),
    TEXT("OpacityMask"),
    TEXT("Normal"),
    TEXT("Tangent"),
    TEXT("WorldPositionOffset"),
    TEXT("Refraction"),
    TEXT("AmbientOcclusion"),
    TEXT("PixelDepthOffset"),
    TEXT("SubsurfaceColor"),
    TEXT("MaterialAttributes"),
    TEXT("Displacement"),
};

template <typename FnT>
void McpForEachMaterialRootPin(const FMcpMaterialGraphOwner& Owner, FnT Visit)
{
    if (Owner.Kind != EMcpMaterialGraphOwnerKind::Material) return;
    UMaterial* Mat = Cast<UMaterial>(Owner.GraphSource);
    if (!Mat) return;

    for (const TCHAR* PinName : GMaterialRootPinNames)
    {
        const FString NameStr(PinName);
        EMaterialProperty Prop = MP_MAX;
        if      (NameStr == TEXT("BaseColor"))            Prop = MP_BaseColor;
        else if (NameStr == TEXT("Metallic"))             Prop = MP_Metallic;
        else if (NameStr == TEXT("Specular"))             Prop = MP_Specular;
        else if (NameStr == TEXT("Roughness"))            Prop = MP_Roughness;
        else if (NameStr == TEXT("Anisotropy"))           Prop = MP_Anisotropy;
        else if (NameStr == TEXT("EmissiveColor"))        Prop = MP_EmissiveColor;
        else if (NameStr == TEXT("Opacity"))              Prop = MP_Opacity;
        else if (NameStr == TEXT("OpacityMask"))          Prop = MP_OpacityMask;
        else if (NameStr == TEXT("Normal"))               Prop = MP_Normal;
        else if (NameStr == TEXT("Tangent"))              Prop = MP_Tangent;
        else if (NameStr == TEXT("WorldPositionOffset"))  Prop = MP_WorldPositionOffset;
        else if (NameStr == TEXT("Refraction"))           Prop = MP_Refraction;
        else if (NameStr == TEXT("AmbientOcclusion"))     Prop = MP_AmbientOcclusion;
        else if (NameStr == TEXT("PixelDepthOffset"))     Prop = MP_PixelDepthOffset;
        else if (NameStr == TEXT("SubsurfaceColor"))      Prop = MP_SubsurfaceColor;
        else if (NameStr == TEXT("MaterialAttributes"))   Prop = MP_MaterialAttributes;
        else if (NameStr == TEXT("Displacement"))         Prop = MP_Displacement;
        if (Prop == MP_MAX) continue;

        FExpressionInput* In = Mat->GetExpressionInputForProperty(Prop);
        if (!In || !In->Expression) continue;
        Visit(NameStr, In->OutputIndex, In->Expression);
    }
}

// =============================================================================
// Reachability BFS from sinks. Returns the orphan set = (all expressions) - reachable
// - decorative nodes. Decorative nodes are excluded from labelling per spec.
// =============================================================================
TSet<UMaterialExpression*> McpComputeOrphanSet(const FMcpMaterialGraphOwner& Owner)
{
    TSet<UMaterialExpression*> Reachable;
    TSet<UMaterialExpression*> Orphans;

    const TArray<TObjectPtr<UMaterialExpression>>* All = McpGetGraphExpressions(Owner);
    if (!All) return Orphans;

    TArray<UMaterialExpression*> Queue;

    if (Owner.Kind == EMcpMaterialGraphOwnerKind::Material)
    {
        McpForEachMaterialRootPin(Owner, [&](const FString& /*PinName*/, int32 /*OutIdx*/, UMaterialExpression* Source)
        {
            if (Source && !Reachable.Contains(Source))
            {
                Reachable.Add(Source);
                Queue.Add(Source);
            }
        });
    }
    else
    {
        // MaterialFunction (and Instance reusing the Base) - sinks are FunctionOutput nodes.
        for (UMaterialExpression* Expr : *All)
        {
            if (Cast<UMaterialExpressionFunctionOutput>(Expr))
            {
                if (!Reachable.Contains(Expr))
                {
                    Reachable.Add(Expr);
                    Queue.Add(Expr);
                }
            }
        }
    }

    // BFS expand: walk each node's ExpressionInput-typed properties, push each
    // upstream expression once.
    while (Queue.Num() > 0)
    {
        UMaterialExpression* Current = Queue.Pop(EAllowShrinking::No);
        if (!Current) continue;
        McpForEachExpressionInputConnection(Current, [&](const FString& /*PinName*/, int32 /*OutIdx*/, UMaterialExpression* Source)
        {
            if (Source && !Reachable.Contains(Source))
            {
                Reachable.Add(Source);
                Queue.Add(Source);
            }
        });
    }

    for (UMaterialExpression* Expr : *All)
    {
        if (!Expr) continue;
        if (McpIsDecorativeForOrphanLabel(Expr)) continue;
        if (Reachable.Contains(Expr)) continue;
        Orphans.Add(Expr);
    }
    return Orphans;
}

// =============================================================================
// Reflection-driven details emission. Walks the catalog's applicable fields for
// the resolved class, skipping the MCP-semantic ones (those are emitted by the
// existing AppendTypedDetails path), and reads the property value via the
// catalog-recorded UE property name. Result is appended to `details` (created
// if needed). Enums are emitted as stripped-form strings.
// =============================================================================
TSharedPtr<FJsonValue> McpReadPropertyAsJson(const FProperty* Prop, const void* Container)
{
    if (!Prop || !Container) return MakeShared<FJsonValueNull>();

    if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
    {
        return MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue_InContainer(Container));
    }
    if (const FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
    {
        const uint8 V = ByteProp->GetPropertyValue_InContainer(Container);
        if (UEnum* En = ByteProp->Enum)
        {
            FString Raw = En->GetNameStringByValue(static_cast<int64>(V));
            int32 UnderscoreIdx = INDEX_NONE;
            if (Raw.FindChar(TEXT('_'), UnderscoreIdx) && UnderscoreIdx > 0 && UnderscoreIdx + 1 < Raw.Len())
            {
                Raw = Raw.Mid(UnderscoreIdx + 1);
            }
            return MakeShared<FJsonValueString>(Raw);
        }
        return MakeShared<FJsonValueNumber>(static_cast<double>(V));
    }
    if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
    {
        const int64 V = EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(
            EnumProp->ContainerPtrToValuePtr<void>(Container));
        UEnum* En = EnumProp->GetEnum();
        if (En)
        {
            FString Raw = En->GetNameStringByValue(V);
            int32 UnderscoreIdx = INDEX_NONE;
            if (Raw.FindChar(TEXT('_'), UnderscoreIdx) && UnderscoreIdx > 0 && UnderscoreIdx + 1 < Raw.Len())
            {
                Raw = Raw.Mid(UnderscoreIdx + 1);
            }
            return MakeShared<FJsonValueString>(Raw);
        }
        return MakeShared<FJsonValueNumber>(static_cast<double>(V));
    }
    if (const FIntProperty* IntProp = CastField<FIntProperty>(Prop))
    {
        return MakeShared<FJsonValueNumber>(static_cast<double>(IntProp->GetPropertyValue_InContainer(Container)));
    }
    if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
    {
        return MakeShared<FJsonValueNumber>(static_cast<double>(FloatProp->GetPropertyValue_InContainer(Container)));
    }
    if (const FDoubleProperty* DblProp = CastField<FDoubleProperty>(Prop))
    {
        return MakeShared<FJsonValueNumber>(DblProp->GetPropertyValue_InContainer(Container));
    }
    if (const FStrProperty* StrProp = CastField<FStrProperty>(Prop))
    {
        return MakeShared<FJsonValueString>(StrProp->GetPropertyValue_InContainer(Container));
    }
    if (const FNameProperty* NameProp = CastField<FNameProperty>(Prop))
    {
        return MakeShared<FJsonValueString>(NameProp->GetPropertyValue_InContainer(Container).ToString());
    }
    if (const FTextProperty* TextProp = CastField<FTextProperty>(Prop))
    {
        return MakeShared<FJsonValueString>(TextProp->GetPropertyValue_InContainer(Container).ToString());
    }
    if (const FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
    {
        UObject* O = ObjProp->GetObjectPropertyValue_InContainer(Container);
        return MakeShared<FJsonValueString>(O ? O->GetPathName() : FString());
    }
    // Fallback: stringify via ExportText.
    FString Text;
    Prop->ExportTextItem_Direct(Text, Prop->ContainerPtrToValuePtr<void>(Container), nullptr, nullptr, PPF_None);
    return MakeShared<FJsonValueString>(Text);
}

void McpEmitReflectedDetails(UMaterialExpression* Expr, const TSharedRef<FJsonObject>& Out)
{
    if (!Expr) return;
    const FString ClassName = Expr->GetClass()->GetName();
    const FMcpMaterialExpressionCatalog& Cat = FMcpMaterialExpressionCatalog::Get();
    const TArray<FString>* Fields = Cat.GetApplicableFields(ClassName);
    if (!Fields || Fields->Num() == 0) return;

    TSharedPtr<FJsonObject> Details;
    {
        const TSharedPtr<FJsonObject>* ExistingPtr = nullptr;
        if (Out->TryGetObjectField(TEXT("details"), ExistingPtr) && ExistingPtr && ExistingPtr->IsValid())
        {
            Details = *ExistingPtr;
        }
        else
        {
            Details = MakeShared<FJsonObject>();
        }
    }

    for (const FString& Field : *Fields)
    {
        // Skip non-reflected (MCP-semantic) fields; the catalog only records
        // a UE property name for reflected ones.
        const FString UePropName = Cat.GetUePropertyName(ClassName, Field);
        if (UePropName.IsEmpty()) continue;

        // Skip semantic fields already emitted by AppendTypedDetails / top-level.
        if (Details->HasField(Field)) continue;

        FProperty* Prop = Expr->GetClass()->FindPropertyByName(*UePropName);
        if (!Prop) continue;

        Details->SetField(Field, McpReadPropertyAsJson(Prop, Expr));
    }

    Out->SetObjectField(TEXT("details"), Details);
}

// =============================================================================
// Top-level connections[] emission. Walks every matched expression's input pins
// and emits one entry per connected input. The shape mirrors
// add_material_nodes.connections[] / connect_material_pins.connections[]:
// {fromNode, fromPin, fromOutputIndex, fromOutputName, toNode, toPin}.
//
// Edges are de-duplicated within the call (same fromExpr+fromOutput+toExpr+toPin
// won't appear twice if a node is matched and is also a sink of root pins).
// =============================================================================
struct FConnectionKey
{
    UMaterialExpression* From = nullptr;
    int32 FromOutputIndex = 0;
    UMaterialExpression* To = nullptr;
    FString ToPin;
    bool bRoot = false;

    bool operator==(const FConnectionKey& Other) const
    {
        return From == Other.From && To == Other.To &&
               FromOutputIndex == Other.FromOutputIndex && bRoot == Other.bRoot &&
               ToPin == Other.ToPin;
    }
};

uint32 GetTypeHash(const FConnectionKey& K)
{
    uint32 H = PointerHash(K.From);
    H = HashCombine(H, PointerHash(K.To));
    H = HashCombine(H, ::GetTypeHash(K.FromOutputIndex));
    H = HashCombine(H, GetTypeHash(K.ToPin));
    H = HashCombine(H, ::GetTypeHash(K.bRoot));
    return H;
}

void McpAppendConnectionEntry(
    TArray<TSharedPtr<FJsonValue>>& OutArr,
    TSet<FConnectionKey>& Dedup,
    UMaterialExpression* FromExpr,
    int32 FromOutputIndex,
    const FString& ToNodeName,
    const FString& ToPinName,
    bool bRoot)
{
    if (!FromExpr) return;
    FConnectionKey Key;
    Key.From = FromExpr;
    Key.FromOutputIndex = FromOutputIndex;
    Key.To = nullptr;
    Key.ToPin = ToPinName;
    Key.bRoot = bRoot;
    if (Dedup.Contains(Key)) return;
    Dedup.Add(Key);

    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("fromNode"), FromExpr->GetName());
    bool bResolved = false;
    const FString OutName = McpGetOutputName(FromExpr, FromOutputIndex, bResolved);
    Obj->SetStringField(TEXT("fromPin"), OutName);
    Obj->SetNumberField(TEXT("fromOutputIndex"), FromOutputIndex);
    if (!OutName.IsEmpty())
    {
        Obj->SetStringField(TEXT("fromOutputName"), OutName);
    }
    Obj->SetStringField(TEXT("toNode"), ToNodeName);
    Obj->SetStringField(TEXT("toPin"), ToPinName);
    OutArr.Add(MakeShared<FJsonValueObject>(Obj));
}

// =============================================================================
// Identifier resolution. Maps each entry of `identifiers[]` to either a matched
// UMaterialExpression* (good path) or a notFound[] entry. JSON numbers route to
// expressionIndex; numeric-string identifiers are rejected per-item; GUID-shaped
// strings go through GUID path; other strings go through the legacy multi-key
// resolver. Misses produce a notFound entry with did-you-mean suggestions.
// =============================================================================
bool McpLooksLikeGuid(const FString& S)
{
    // Match the {} or no-{} hex-with-hyphens GUID forms via FGuid::Parse.
    FGuid Tmp;
    return FGuid::Parse(S, Tmp);
}

TSharedRef<FJsonObject> McpMakeNotFoundEntry(
    const TSharedPtr<FJsonValue>& Identifier,
    const FString& Kind,
    const FString& Message,
    const TArray<FString>* DidYouMean = nullptr)
{
    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
    if (Identifier.IsValid())
    {
        Obj->SetField(TEXT("identifier"), Identifier);
    }
    Obj->SetStringField(TEXT("kind"), Kind);
    Obj->SetStringField(TEXT("message"), Message);
    if (DidYouMean && DidYouMean->Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> Arr;
        for (const FString& S : *DidYouMean)
        {
            Arr.Add(MakeShared<FJsonValueString>(S));
        }
        Obj->SetArrayField(TEXT("didYouMean"), Arr);
    }
    return Obj;
}

// Levenshtein-based name suggestion against the live graph's expression /
// parameter names. Returns up to MaxCount best matches for a name miss.
TArray<FString> McpSuggestNames(
    const FMcpMaterialGraphOwner& Owner,
    const FString& Query,
    int32 MaxCount)
{
    TArray<FString> Out;
    if (Query.IsEmpty() || MaxCount <= 0) return Out;

    auto LevDist = [](const FString& A, const FString& B) -> int32
    {
        const int32 M = A.Len();
        const int32 N = B.Len();
        if (M == 0) return N;
        if (N == 0) return M;
        TArray<int32> Prev, Curr;
        Prev.SetNum(N + 1);
        Curr.SetNum(N + 1);
        for (int32 j = 0; j <= N; ++j) Prev[j] = j;
        for (int32 i = 1; i <= M; ++i)
        {
            Curr[0] = i;
            const TCHAR Ai = A[i - 1];
            for (int32 j = 1; j <= N; ++j)
            {
                const TCHAR Bj = B[j - 1];
                const int32 Cost = (Ai == Bj) ? 0 : 1;
                int32 Best = FMath::Min(Prev[j] + 1, Curr[j - 1] + 1);
                Best = FMath::Min(Best, Prev[j - 1] + Cost);
                Curr[j] = Best;
            }
            Swap(Prev, Curr);
        }
        return Prev[N];
    };

    const TArray<TObjectPtr<UMaterialExpression>>* All = McpGetGraphExpressions(Owner);
    if (!All) return Out;

    TArray<TPair<FString, int32>> Scored;
    for (UMaterialExpression* Expr : *All)
    {
        if (!Expr) continue;
        const FString N = Expr->GetName();
        Scored.Add({ N, LevDist(Query, N) });
        if (UMaterialExpressionParameter* P = Cast<UMaterialExpressionParameter>(Expr))
        {
            const FString PN = P->ParameterName.ToString();
            if (!PN.IsEmpty()) Scored.Add({ PN, LevDist(Query, PN) });
        }
        else if (UMaterialExpressionTextureSampleParameter* TP =
                     Cast<UMaterialExpressionTextureSampleParameter>(Expr))
        {
            const FString PN = TP->ParameterName.ToString();
            if (!PN.IsEmpty()) Scored.Add({ PN, LevDist(Query, PN) });
        }
    }
    Scored.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B)
    {
        if (A.Value != B.Value) return A.Value < B.Value;
        return A.Key < B.Key;
    });
    const int32 Take = FMath::Min(MaxCount, Scored.Num());
    Out.Reserve(Take);
    for (int32 i = 0; i < Take; ++i)
    {
        Out.Add(Scored[i].Key);
    }
    return Out;
}

struct FResolvedIdentifier
{
    UMaterialExpression* Expr = nullptr;     // null on miss
    TSharedPtr<FJsonObject> NotFoundEntry;   // populated on miss
};

FResolvedIdentifier McpResolveIdentifier(
    const FMcpMaterialGraphOwner& Owner,
    const TSharedPtr<FJsonValue>& Id)
{
    FResolvedIdentifier R;
    const TArray<TObjectPtr<UMaterialExpression>>* All = McpGetGraphExpressions(Owner);
    const int32 GraphSize = All ? All->Num() : 0;

    if (!Id.IsValid())
    {
        R.NotFoundEntry = McpMakeNotFoundEntry(Id, TEXT("invalid_type"),
            TEXT("identifier is null/invalid"));
        return R;
    }

    // JSON number -> expressionIndex
    if (Id->Type == EJson::Number)
    {
        const int32 Idx = static_cast<int32>(Id->AsNumber());
        if (All && Idx >= 0 && Idx < GraphSize)
        {
            R.Expr = (*All)[Idx];
            return R;
        }
        R.NotFoundEntry = McpMakeNotFoundEntry(Id, TEXT("index"),
            FString::Printf(TEXT("expressionIndex %d out of range (graph has %d)"),
                Idx, GraphSize));
        return R;
    }

    // String identifier
    if (Id->Type != EJson::String)
    {
        R.NotFoundEntry = McpMakeNotFoundEntry(Id, TEXT("invalid_type"),
            TEXT("identifier must be a JSON number (index) or string (guid/name/parameterName)"));
        return R;
    }

    const FString S = Id->AsString();

    // Numeric-string -> rejected per spec table (do not auto-coerce to index).
    if (S.IsNumeric())
    {
        R.NotFoundEntry = McpMakeNotFoundEntry(Id, TEXT("invalid_type"),
            FString::Printf(TEXT("numeric-string identifier '%s' is ambiguous - pass JSON number for index"), *S));
        return R;
    }

    // GUID
    if (McpLooksLikeGuid(S))
    {
        FGuid ParsedGuid;
        if (FGuid::Parse(S, ParsedGuid) && All)
        {
            for (UMaterialExpression* Expr : *All)
            {
                if (Expr && Expr->MaterialExpressionGuid == ParsedGuid)
                {
                    R.Expr = Expr;
                    return R;
                }
            }
        }
        R.NotFoundEntry = McpMakeNotFoundEntry(Id, TEXT("guid"),
            FString::Printf(TEXT("no expression with MaterialExpressionGuid '%s' in graph"), *S));
        return R;
    }

    // Other strings -> name / path / parameterName via legacy resolver.
    if (UMaterialExpression* Found = McpFindGraphExpression(Owner, S))
    {
        R.Expr = Found;
        return R;
    }

    // Miss: with did-you-mean.
    TArray<FString> DYM = McpSuggestNames(Owner, S, 5);
    R.NotFoundEntry = McpMakeNotFoundEntry(Id, TEXT("name"),
        FString::Printf(TEXT("no expression matching '%s' (by name, path, or parameterName)"), *S),
        &DYM);
    return R;
}

// =============================================================================
// Per-expression top-level fields (always present for each entry).
// =============================================================================
void McpEmitExpressionTopLevel(
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpression* Expr,
    int32 Index,
    const TSet<UMaterialExpression*>* OrphanSet,
    const TSharedRef<FJsonObject>& Out)
{
    if (!Expr) return;
    McpAddExpressionIdentity(Owner, Expr, Index, Out);
    Out->SetStringField(TEXT("className"), Expr->GetClass()->GetName());
    Out->SetStringField(TEXT("desc"), Expr->Desc);

    if (UMaterialExpressionParameter* Param = Cast<UMaterialExpressionParameter>(Expr))
    {
        Out->SetStringField(TEXT("parameterName"), Param->ParameterName.ToString());
    }
    else if (UMaterialExpressionTextureSampleParameter* TexParam =
                 Cast<UMaterialExpressionTextureSampleParameter>(Expr))
    {
        Out->SetStringField(TEXT("parameterName"), TexParam->ParameterName.ToString());
    }

    // Spec: functionPath/functionName always at top-level for MaterialFunctionCall (post-C.6).
    if (UMaterialExpressionMaterialFunctionCall* FuncCall =
            Cast<UMaterialExpressionMaterialFunctionCall>(Expr))
    {
        if (FuncCall->MaterialFunction)
        {
            Out->SetStringField(TEXT("functionPath"), FuncCall->MaterialFunction->GetPathName());
            Out->SetStringField(TEXT("functionName"), FuncCall->MaterialFunction->GetName());
        }
    }

    // isOrphan (omit for decorative classes).
    if (OrphanSet && !McpIsDecorativeForOrphanLabel(Expr))
    {
        Out->SetBoolField(TEXT("isOrphan"), OrphanSet->Contains(Expr));
    }
}

// =============================================================================
// Per-expression consumers[]. Walks all OTHER expressions in the graph; for each
// FExpressionInput pointing back at `Source`, emit a consumer entry. Also walks
// the material root pins (when applicable) so consumers via $material show up.
// =============================================================================
TArray<TSharedPtr<FJsonValue>> McpBuildConsumers(
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpression* Source)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    if (!Source) return Out;
    const TArray<TObjectPtr<UMaterialExpression>>* All = McpGetGraphExpressions(Owner);
    if (!All) return Out;

    for (UMaterialExpression* Candidate : *All)
    {
        if (!Candidate || Candidate == Source) continue;
        McpForEachExpressionInputConnection(Candidate,
            [&](const FString& PinName, int32 OutIdx, UMaterialExpression* Up)
            {
                if (Up != Source) return;
                TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
                Obj->SetStringField(TEXT("nodeId"), Candidate->MaterialExpressionGuid.ToString());
                Obj->SetStringField(TEXT("expressionName"), Candidate->GetName());
                Obj->SetNumberField(TEXT("expressionIndex"), McpExpressionIndex(Owner, Candidate));
                Obj->SetStringField(TEXT("inputName"), PinName);
                Obj->SetNumberField(TEXT("outputIndex"), OutIdx);
                bool bResolved = false;
                const FString OutName = McpGetOutputName(Source, OutIdx, bResolved);
                if (!OutName.IsEmpty())
                {
                    Obj->SetStringField(TEXT("outputName"), OutName);
                }
                Out.Add(MakeShared<FJsonValueObject>(Obj));
            });
    }

    // Material root consumers (toNode = "$material").
    McpForEachMaterialRootPin(Owner,
        [&](const FString& PinName, int32 OutIdx, UMaterialExpression* Up)
        {
            if (Up != Source) return;
            TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("nodeId"), TEXT("$material"));
            Obj->SetStringField(TEXT("expressionName"), TEXT("$material"));
            Obj->SetStringField(TEXT("inputName"), PinName);
            Obj->SetNumberField(TEXT("outputIndex"), OutIdx);
            bool bResolved = false;
            const FString OutName = McpGetOutputName(Source, OutIdx, bResolved);
            if (!OutName.IsEmpty())
            {
                Obj->SetStringField(TEXT("outputName"), OutName);
            }
            Out.Add(MakeShared<FJsonValueObject>(Obj));
        });

    return Out;
}

} // namespace

#endif // WITH_EDITOR

// =============================================================================
// Test-only forwarders. Pure helpers so tests can drive validator-style logic
// without spinning up a subsystem / socket.
// =============================================================================
#if WITH_EDITOR
namespace McpFindMaterialExpressionsForTests
{
    TSet<UMaterialExpression*> ComputeOrphanSet(const FMcpMaterialGraphOwner& Owner)
    {
        return McpComputeOrphanSet(Owner);
    }

    bool LooksLikeGuid(const FString& S)
    {
        return McpLooksLikeGuid(S);
    }

    // Resolves a single identifier; returns nullptr + fills out kind/message on miss.
    UMaterialExpression* ResolveIdentifier(
        const FMcpMaterialGraphOwner& Owner,
        const TSharedPtr<FJsonValue>& Id,
        FString& OutKind,
        FString& OutMessage)
    {
        FResolvedIdentifier R = McpResolveIdentifier(Owner, Id);
        if (R.Expr) return R.Expr;
        if (R.NotFoundEntry.IsValid())
        {
            R.NotFoundEntry->TryGetStringField(TEXT("kind"), OutKind);
            R.NotFoundEntry->TryGetStringField(TEXT("message"), OutMessage);
        }
        return nullptr;
    }

    bool ExpressionMatchesFilters(
        const FMcpMaterialGraphOwner& Owner,
        UMaterialExpression* Expr,
        int32 Index,
        const TSharedPtr<FJsonObject>& Payload,
        const TSet<UMaterialExpression*>* OrphanSet)
    {
        return McpExpressionMatchesFilters(Owner, Expr, Index, Payload, OrphanSet);
    }
}
#endif

// =============================================================================
// Free-function dispatch entry point. Wired in McpAutomationBridge_MaterialAuthoringHandlers.cpp.
// =============================================================================
bool McpHandle_FindMaterialExpressions(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (!Sub)
    {
        return true;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("find_material_expressions payload missing"),
            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) &&
        !Payload->TryGetStringField(TEXT("materialPath"), AssetPath))
    {
        Sub->SendAutomationError(Socket, RequestId, TEXT("assetPath is required"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FMcpMaterialGraphOwner Owner;
    FString OwnerError;
    if (!McpResolveMaterialGraphOwner(AssetPath, Owner, OwnerError))
    {
        Sub->SendAutomationError(Socket, RequestId, OwnerError,
            OwnerError.Contains(TEXT("not found"))
                ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE"));
        return true;
    }

    bool bIncludeDetails = false;
    bool bIncludeConnections = false;
    bool bIncludeConsumers = false;
    Payload->TryGetBoolField(TEXT("includeDetails"),     bIncludeDetails);
    Payload->TryGetBoolField(TEXT("includeConnections"), bIncludeConnections);
    Payload->TryGetBoolField(TEXT("includeConsumers"),   bIncludeConsumers);

    // Pagination (clamped silently; warning emitted on clamp).
    TArray<FString> Warnings;
    int32 Limit = 50;
    int32 Offset = 0;
    Payload->TryGetNumberField(TEXT("limit"), Limit);
    Payload->TryGetNumberField(TEXT("offset"), Offset);
    if (Limit > 500)
    {
        Warnings.Add(TEXT("limit clamped to 500"));
        Limit = 500;
    }
    if (Limit < 0) Limit = 0;
    if (Offset < 0) Offset = 0;

    // Orphan set is only computed when needed (filter or top-level emission).
    bool bIsOrphanFilterPresent = false;
    {
        bool Tmp = false;
        bIsOrphanFilterPresent = Payload->TryGetBoolField(TEXT("isOrphan"), Tmp);
    }
    TSet<UMaterialExpression*> OrphanSet = McpComputeOrphanSet(Owner);
    const TSet<UMaterialExpression*>* OrphanSetPtr = &OrphanSet;

    const TArray<TObjectPtr<UMaterialExpression>>* AllExprs = McpGetGraphExpressions(Owner);

    // --- Build the matched set ---
    TArray<UMaterialExpression*> MatchedExprs;        // expression pointer per slot
    TArray<int32> MatchedIndices;                     // expressionIndex per slot
    TArray<TSharedPtr<FJsonValue>> NotFound;

    const TArray<TSharedPtr<FJsonValue>>* IdentifiersArr = nullptr;
    const bool bHasIdentifiers = Payload->TryGetArrayField(TEXT("identifiers"), IdentifiersArr) &&
        IdentifiersArr && IdentifiersArr->Num() > 0;

    if (bHasIdentifiers)
    {
        // Identifier-driven: pagination is disabled; resolve each one, apply filters
        // afterwards (filters still AND-conjoin per spec).
        for (const TSharedPtr<FJsonValue>& Id : *IdentifiersArr)
        {
            FResolvedIdentifier R = McpResolveIdentifier(Owner, Id);
            if (R.Expr)
            {
                const int32 Idx = McpExpressionIndex(Owner, R.Expr);
                if (McpExpressionMatchesFilters(Owner, R.Expr, Idx, Payload, OrphanSetPtr))
                {
                    MatchedExprs.Add(R.Expr);
                    MatchedIndices.Add(Idx);
                }
                else
                {
                    // Resolved but failed filters - report as a notFound entry so
                    // the caller can tell which identifier was elided.
                    NotFound.Add(MakeShared<FJsonValueObject>(McpMakeNotFoundEntry(
                        Id, TEXT("filtered"),
                        TEXT("identifier resolved but did not match the active filters"))));
                }
            }
            else if (R.NotFoundEntry.IsValid())
            {
                NotFound.Add(MakeShared<FJsonValueObject>(R.NotFoundEntry));
            }
        }
    }
    else if (AllExprs)
    {
        for (int32 i = 0; i < AllExprs->Num(); ++i)
        {
            UMaterialExpression* Expr = (*AllExprs)[i];
            if (!Expr) continue;
            if (!McpExpressionMatchesFilters(Owner, Expr, i, Payload, OrphanSetPtr)) continue;
            MatchedExprs.Add(Expr);
            MatchedIndices.Add(i);
        }
    }

    // matchCount/returnedCount/hasMore math is post-filter, pre-includes.
    const int32 MatchCount = MatchedExprs.Num();

    // Slice the page when not identifier-driven.
    int32 SliceStart = 0;
    int32 SliceEnd = MatchCount;
    if (!bHasIdentifiers)
    {
        SliceStart = FMath::Min(Offset, MatchCount);
        SliceEnd   = FMath::Min(SliceStart + Limit, MatchCount);
    }
    const int32 ReturnedCount = SliceEnd - SliceStart;
    const bool bHasMore = !bHasIdentifiers && SliceEnd < MatchCount;

    // --- Emit the per-expression entries ---
    TArray<TSharedPtr<FJsonValue>> Expressions;
    Expressions.Reserve(ReturnedCount);

    TSet<UMaterialExpression*> EmittedSet;          // for connections[] de-duplication
    TArray<TSharedPtr<FJsonValue>> Connections;
    TSet<FConnectionKey> ConnDedup;

    for (int32 Slot = SliceStart; Slot < SliceEnd; ++Slot)
    {
        UMaterialExpression* Expr = MatchedExprs[Slot];
        const int32 Idx = MatchedIndices[Slot];

        TSharedRef<FJsonObject> Match = MakeShared<FJsonObject>();
        McpEmitExpressionTopLevel(Owner, Expr, Idx, OrphanSetPtr, Match);

        if (bIncludeDetails && Expr)
        {
            // Existing semantic detail emission. Preserves shape used by the
            // legacy bulk_get_material_expression_details / get_material_node_details.
            McpMaterialExpressionDetails::AppendTypedDetails(Owner, Expr, Match);
            // Reflection-driven details (catalog walk). Runs AFTER semantic
            // details so the latter take precedence on shared keys.
            McpEmitReflectedDetails(Expr, Match);
        }

        if (bIncludeConsumers && Expr)
        {
            Match->SetArrayField(TEXT("consumers"), McpBuildConsumers(Owner, Expr));
        }

        Expressions.Add(MakeShared<FJsonValueObject>(Match));

        if (bIncludeConnections && Expr)
        {
            EmittedSet.Add(Expr);
            // Walk this expression's input pins (edges where this is the
            // downstream side); fromNode = upstream expression name.
            McpForEachExpressionInputConnection(Expr,
                [&](const FString& PinName, int32 OutIdx, UMaterialExpression* Up)
                {
                    McpAppendConnectionEntry(Connections, ConnDedup, Up, OutIdx,
                        Expr->GetName(), PinName, /*bRoot*/ false);
                });
        }
    }

    // For connections[], also walk the material root pins so any matched
    // expression that feeds the root is represented as toNode="$material".
    if (bIncludeConnections)
    {
        McpForEachMaterialRootPin(Owner,
            [&](const FString& PinName, int32 OutIdx, UMaterialExpression* Up)
            {
                if (!EmittedSet.Contains(Up)) return;
                McpAppendConnectionEntry(Connections, ConnDedup, Up, OutIdx,
                    TEXT("$material"), PinName, /*bRoot*/ true);
            });
    }

    // --- Build the response ---
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Owner.Asset);
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetArrayField(TEXT("expressions"), Expressions);
    Result->SetNumberField(TEXT("matchCount"),    MatchCount);
    Result->SetNumberField(TEXT("returnedCount"), ReturnedCount);
    Result->SetBoolField  (TEXT("hasMore"),       bHasMore);
    Result->SetNumberField(TEXT("limit"),  Limit);
    Result->SetNumberField(TEXT("offset"), Offset);

    if (bIncludeConnections)
    {
        Result->SetArrayField(TEXT("connections"), Connections);
    }

    if (NotFound.Num() > 0)
    {
        Result->SetArrayField(TEXT("notFound"), NotFound);
    }

    if (Warnings.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> WarningsArr;
        for (const FString& W : Warnings)
        {
            WarningsArr.Add(MakeShared<FJsonValueString>(W));
        }
        Result->SetArrayField(TEXT("warnings"), WarningsArr);
    }

    Sub->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Material expressions found"), Result, FString());
    return true;
#else
    Sub->SendAutomationResponse(Socket, RequestId, false,
        TEXT("find_material_expressions requires editor build"),
        nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =============================================================================
// E.2 - list_material_expression_classes discovery action.
// Spec: docs/superpowers/specs/2026-05-07-mcp-material-tools-redesign-design.md (sec 8a)
//
// Returns the cached FMcpMaterialExpressionCatalog filtered by optional `filter`
// (Contains, case-insensitive on class name) and `category` (UE menu category,
// case-insensitive exact match). Always emits the static material root pin
// schema (sentinel + aliases + pins) so the agent gets the catalog and the root
// addressing in a single round-trip.
// =============================================================================
#if WITH_EDITOR
namespace
{

// Spec sec 8a: 13 root pins, in declared order.
struct FMcpRootPinSpec
{
    const TCHAR* Name;
    const TCHAR* Type;
    const TCHAR* ApplicableWhen; // nullptr when always applicable
};

static const FMcpRootPinSpec GMcpRootPinSpecs[] = {
    { TEXT("BaseColor"),           TEXT("FVector3"),             nullptr },
    { TEXT("Metallic"),            TEXT("FScalar"),              nullptr },
    { TEXT("Roughness"),           TEXT("FScalar"),              nullptr },
    { TEXT("Specular"),            TEXT("FScalar"),              nullptr },
    { TEXT("Normal"),              TEXT("FVector3"),             nullptr },
    { TEXT("EmissiveColor"),       TEXT("FVector3"),             nullptr },
    { TEXT("Opacity"),             TEXT("FScalar"),              TEXT("blendMode in [Translucent, AlphaComposite]") },
    { TEXT("OpacityMask"),         TEXT("FScalar"),              TEXT("blendMode in [Masked]") },
    { TEXT("WorldPositionOffset"), TEXT("FVector3"),             nullptr },
    { TEXT("Refraction"),          TEXT("FVector3"),             nullptr },
    { TEXT("AmbientOcclusion"),    TEXT("FScalar"),              nullptr },
    { TEXT("PixelDepthOffset"),    TEXT("FScalar"),              nullptr },
    { TEXT("MaterialAttributes"),  TEXT("FMaterialAttributes"),  TEXT("materialAttributesMode = true") },
};

TArray<TSharedPtr<FJsonValue>> McpBuildMaterialRootPinsJson()
{
    TArray<TSharedPtr<FJsonValue>> Out;
    Out.Reserve(UE_ARRAY_COUNT(GMcpRootPinSpecs));
    for (const FMcpRootPinSpec& P : GMcpRootPinSpecs)
    {
        TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"), P.Name);
        Obj->SetStringField(TEXT("type"), P.Type);
        if (P.ApplicableWhen)
        {
            Obj->SetStringField(TEXT("applicableWhen"), P.ApplicableWhen);
        }
        Out.Add(MakeShared<FJsonValueObject>(Obj));
    }
    return Out;
}

// Aggregate the union of categories the catalog has registered. Walks the
// catalog's full class list and unions GetCategories(class). Sorted ascending
// for deterministic output.
TArray<FString> McpCollectAllCategories(const FMcpMaterialExpressionCatalog& Cat)
{
    TSet<FString> Set;
    for (const FString& ClassName : Cat.GetAllClasses())
    {
        if (const TArray<FString>* Cats = Cat.GetCategories(ClassName))
        {
            for (const FString& C : *Cats)
            {
                if (!C.IsEmpty()) Set.Add(C);
            }
        }
    }
    TArray<FString> Out = Set.Array();
    Out.Sort();
    return Out;
}

// Case-insensitive exact-match against the class's category list.
bool McpClassMatchesCategory(
    const FMcpMaterialExpressionCatalog& Cat,
    const FString& ClassName,
    const FString& Category)
{
    const TArray<FString>* Cats = Cat.GetCategories(ClassName);
    if (!Cats) return false;
    for (const FString& C : *Cats)
    {
        if (C.Equals(Category, ESearchCase::IgnoreCase)) return true;
    }
    return false;
}

} // namespace
#endif // WITH_EDITOR

bool McpHandle_ListMaterialExpressionClasses(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (!Sub)
    {
        return true;
    }

#if WITH_EDITOR
    const FMcpMaterialExpressionCatalog& Cat = FMcpMaterialExpressionCatalog::Get();

    // Optional filters
    FString Filter;
    FString Category;
    if (Payload.IsValid())
    {
        Payload->TryGetStringField(TEXT("filter"), Filter);
        Payload->TryGetStringField(TEXT("category"), Category);
    }

    // Aggregate categories up front; needed for both INVALID_CATEGORY error
    // path and the response payload.
    const TArray<FString> AllCategories = McpCollectAllCategories(Cat);

    // Validate `category` against the registered set (case-insensitive).
    if (!Category.IsEmpty())
    {
        bool bKnown = false;
        for (const FString& C : AllCategories)
        {
            if (C.Equals(Category, ESearchCase::IgnoreCase)) { bKnown = true; break; }
        }
        if (!bKnown)
        {
            // Build "[a, b, c]" available list in the message.
            FString List;
            for (int32 i = 0; i < AllCategories.Num(); ++i)
            {
                if (i > 0) List += TEXT(", ");
                List += AllCategories[i];
            }
            const FString Message = FString::Printf(
                TEXT("Unknown category '%s'. Available: [%s]"),
                *Category, *List);
            Sub->SendAutomationError(Socket, RequestId, Message, TEXT("INVALID_CATEGORY"));
            return true;
        }
    }

    // Walk catalog, apply both filters with AND semantics.
    TArray<TSharedPtr<FJsonValue>> Classes;
    Classes.Reserve(Cat.GetAllClasses().Num());

    for (const FString& ClassName : Cat.GetAllClasses())
    {
        if (!Filter.IsEmpty() && !ClassName.Contains(Filter, ESearchCase::IgnoreCase))
        {
            continue;
        }
        if (!Category.IsEmpty() && !McpClassMatchesCategory(Cat, ClassName, Category))
        {
            continue;
        }

        TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("nodeType"), ClassName);

        // Category: spec shows a single string. Use the first registered category
        // as the canonical one; classes can have multiple in MenuCategories metadata
        // but the discovery payload picks the primary.
        if (const TArray<FString>* Cats = Cat.GetCategories(ClassName))
        {
            if (Cats->Num() > 0)
            {
                Entry->SetStringField(TEXT("category"), (*Cats)[0]);
            }
        }

        Entry->SetStringField(TEXT("description"), Cat.GetDescription(ClassName));

        // applicableFields[] - always emit (may be empty for unusual classes).
        TArray<TSharedPtr<FJsonValue>> Fields;
        if (const TArray<FString>* AppFields = Cat.GetApplicableFields(ClassName))
        {
            Fields.Reserve(AppFields->Num());
            for (const FString& F : *AppFields)
            {
                Fields.Add(MakeShared<FJsonValueString>(F));
            }
        }
        Entry->SetArrayField(TEXT("applicableFields"), Fields);

        // fieldEnums{} - emit only when at least one applicable field has enum values.
        TSharedPtr<FJsonObject> FieldEnums;
        if (const TArray<FString>* AppFields = Cat.GetApplicableFields(ClassName))
        {
            for (const FString& F : *AppFields)
            {
                const TArray<FString>* EnumVals = Cat.GetFieldEnumValues(ClassName, F);
                if (!EnumVals || EnumVals->Num() == 0) continue;
                if (!FieldEnums.IsValid())
                {
                    FieldEnums = MakeShared<FJsonObject>();
                }
                TArray<TSharedPtr<FJsonValue>> Vs;
                Vs.Reserve(EnumVals->Num());
                for (const FString& V : *EnumVals)
                {
                    Vs.Add(MakeShared<FJsonValueString>(V));
                }
                FieldEnums->SetArrayField(F, Vs);
            }
        }
        if (FieldEnums.IsValid())
        {
            Entry->SetObjectField(TEXT("fieldEnums"), FieldEnums);
        }

        Classes.Add(MakeShared<FJsonValueObject>(Entry));
    }

    // Build response.
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("classes"), Classes);

    TArray<TSharedPtr<FJsonValue>> CategoriesArr;
    CategoriesArr.Reserve(AllCategories.Num());
    for (const FString& C : AllCategories)
    {
        CategoriesArr.Add(MakeShared<FJsonValueString>(C));
    }
    Result->SetArrayField(TEXT("categories"), CategoriesArr);

    Result->SetStringField(TEXT("materialRootSentinel"), TEXT("$material"));

    TArray<TSharedPtr<FJsonValue>> Aliases;
    Aliases.Add(MakeShared<FJsonValueString>(TEXT("$root")));
    Aliases.Add(MakeShared<FJsonValueString>(TEXT("MaterialOutput")));
    Aliases.Add(MakeShared<FJsonValueString>(TEXT("Material")));
    Aliases.Add(MakeShared<FJsonValueString>(TEXT("Root")));
    Result->SetArrayField(TEXT("materialRootSentinelAliases"), Aliases);

    Result->SetArrayField(TEXT("materialRootPins"), McpBuildMaterialRootPinsJson());

    Sub->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Material expression classes listed"), Result, FString());
    return true;
#else
    Sub->SendAutomationResponse(Socket, RequestId, false,
        TEXT("list_material_expression_classes requires editor build"),
        nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// Test forwarders (catalog-derived helpers visible to tests).
#if WITH_EDITOR
namespace McpListMaterialExpressionClassesForTests
{
    TArray<FString> CollectAllCategories()
    {
        return McpCollectAllCategories(FMcpMaterialExpressionCatalog::Get());
    }

    int32 MaterialRootPinCount()
    {
        return UE_ARRAY_COUNT(GMcpRootPinSpecs);
    }
}
#endif
