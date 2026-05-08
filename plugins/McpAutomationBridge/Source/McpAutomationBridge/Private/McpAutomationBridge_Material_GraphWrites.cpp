// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_Material_GraphWrites.cpp
//
// Task C.1 - add_material_nodes transactional handler.
// Plan: docs/superpowers/plans/2026-05-07-mcp-material-tools-redesign.md
// Spec: docs/superpowers/specs/2026-05-07-mcp-material-tools-redesign-design.md (sec 7)

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/Class.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpMaterialExpressionCatalog.h"

#if WITH_EDITOR

#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"

#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionStaticBool.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionPanner.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "Materials/MaterialExpressionRotator.h"
#endif

#include "MaterialEditingLibrary.h"
#include "ScopedTransaction.h"
#include "Engine/Texture.h"
#include "SceneTypes.h"

#endif // WITH_EDITOR

// =============================================================================
// Internal data structures (file-private)
// =============================================================================
namespace
{

struct FMcpNodeValidationError
{
    int32           Index = INDEX_NONE;
    FString         LocalId;
    FString         Field;
    FString         Code;
    FString         Message;
    TArray<FString> DidYouMean;
};

struct FMcpConnectionValidationError
{
    int32   Index = INDEX_NONE;
    FString Field;
    FString Code;
    FString Message;
};

#if WITH_EDITOR

struct FMcpResolvedConnection
{
    int32                 Index = INDEX_NONE;
    bool                  bToMaterialRoot = false;
    UMaterialExpression*  FromExpression = nullptr;  // null when from refers to a localId in current batch
    UMaterialExpression*  ToExpression = nullptr;    // null when to is localId or root
    FString               FromLocalId;
    FString               ToLocalId;
    FString               FromPin;
    int32                 FromOutputIndex = INDEX_NONE;
    FString               ToPin;
};

// =============================================================================
// Helper: enum value matching (case-insensitive, accept stripped or full form)
// =============================================================================
static bool McpEnumValueMatches(const FString& StrValue, const TArray<FString>& Allowed)
{
    for (const FString& V : Allowed)
    {
        if (V.Equals(StrValue, ESearchCase::IgnoreCase)) return true;
    }
    return false;
}

// =============================================================================
// Helper: walk the class for FExpressionInput properties whose name matches.
// =============================================================================
static bool McpClassHasInputPin(UClass* Cls, const FString& PinName, TArray<FString>& OutPinNames)
{
    OutPinNames.Reset();
    if (!Cls) return false;
    bool bFound = false;
    for (TFieldIterator<FProperty> PropIt(Cls); PropIt; ++PropIt)
    {
        FProperty* Prop = *PropIt;
        if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
        {
            if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput")))
            {
                const FString Name = Prop->GetName();
                OutPinNames.Add(Name);
                if (Name.Equals(PinName, ESearchCase::IgnoreCase))
                {
                    bFound = true;
                }
            }
        }
    }
    return bFound;
}

// =============================================================================
// Helper: material root pin string -> EMaterialProperty
// =============================================================================
static EMaterialProperty McpMaterialPropertyFromName(const FString& Name)
{
    if (Name == TEXT("BaseColor"))            return MP_BaseColor;
    if (Name == TEXT("Metallic"))             return MP_Metallic;
    if (Name == TEXT("Roughness"))            return MP_Roughness;
    if (Name == TEXT("Specular"))             return MP_Specular;
    if (Name == TEXT("Normal"))               return MP_Normal;
    if (Name == TEXT("EmissiveColor"))        return MP_EmissiveColor;
    if (Name == TEXT("Opacity"))              return MP_Opacity;
    if (Name == TEXT("OpacityMask"))          return MP_OpacityMask;
    if (Name == TEXT("WorldPositionOffset"))  return MP_WorldPositionOffset;
    if (Name == TEXT("Refraction"))           return MP_Refraction;
    if (Name == TEXT("AmbientOcclusion"))     return MP_AmbientOcclusion;
    if (Name == TEXT("PixelDepthOffset"))     return MP_PixelDepthOffset;
    if (Name == TEXT("MaterialAttributes"))   return MP_MaterialAttributes;
    return MP_MAX;
}

static bool McpIsValidMaterialRootPinName(const FString& Name)
{
    return McpMaterialPropertyFromName(Name) != MP_MAX;
}

// =============================================================================
// McpValidateApplicableFields
//
// Shared core: walks Item's keys, checks each against the catalog's applicable
// list for the resolved class, and validates enum-typed values. Skips keys
// listed in IgnoredKeys (e.g. "nodeType" for add, "identifier" for update).
// =============================================================================
static bool McpValidateApplicableFields(
    const TSharedPtr<FJsonObject>& Item,
    UClass* ResolvedClass,
    const TArray<FString>& IgnoredKeys,
    FMcpNodeValidationError& OutError)
{
    if (!Item.IsValid() || !ResolvedClass) return true;

    const FMcpMaterialExpressionCatalog& Cat = FMcpMaterialExpressionCatalog::Get();
    const FString CanonicalName = ResolvedClass->GetName();
    const TArray<FString>* Applicable = Cat.GetApplicableFields(CanonicalName);
    if (!Applicable)
    {
        OutError.Code = TEXT("INVALID_NODE_TYPE");
        OutError.Field = TEXT("nodeType");
        OutError.Message = FString::Printf(TEXT("No applicable-field metadata for '%s'"), *CanonicalName);
        return false;
    }

    for (const auto& Pair : Item->Values)
    {
        const FString& Key = Pair.Key;
        bool bIgnore = false;
        for (const FString& I : IgnoredKeys)
        {
            if (Key.Equals(I, ESearchCase::CaseSensitive)) { bIgnore = true; break; }
        }
        if (bIgnore) continue;

        // applicable list contains: localId, x, y, desc + per-class semantic + reflected fields
        const bool bApplicable = Applicable->ContainsByPredicate(
            [&Key](const FString& F) { return F.Equals(Key, ESearchCase::CaseSensitive); });

        if (!bApplicable)
        {
            OutError.Code = TEXT("FIELD_NOT_APPLICABLE");
            OutError.Field = Key;
            FString Joined;
            const int32 N = FMath::Min(Applicable->Num(), 24);
            for (int32 i = 0; i < N; ++i)
            {
                if (i > 0) Joined += TEXT(", ");
                Joined += (*Applicable)[i];
            }
            if (Applicable->Num() > N) Joined += TEXT(", ...");
            OutError.Message = FString::Printf(
                TEXT("Field '%s' is not applicable for '%s'. Applicable: %s"),
                *Key, *CanonicalName, *Joined);
            return false;
        }

        // Enum-typed fields: validate the JSON value is a string and matches.
        const TArray<FString>* AllowedValues = Cat.GetFieldEnumValues(CanonicalName, Key);
        if (AllowedValues && AllowedValues->Num() > 0)
        {
            FString StrValue;
            if (!Pair.Value.IsValid() || !Pair.Value->TryGetString(StrValue))
            {
                OutError.Code = TEXT("INVALID_ENUM_VALUE");
                OutError.Field = Key;
                OutError.Message = FString::Printf(
                    TEXT("Field '%s' is enum-typed; expected string value"), *Key);
                return false;
            }
            if (!McpEnumValueMatches(StrValue, *AllowedValues))
            {
                FString Joined;
                for (int32 i = 0; i < AllowedValues->Num(); ++i)
                {
                    if (i > 0) Joined += TEXT(", ");
                    Joined += (*AllowedValues)[i];
                }
                OutError.Code = TEXT("INVALID_ENUM_VALUE");
                OutError.Field = Key;
                OutError.Message = FString::Printf(
                    TEXT("Invalid value '%s' for enum field '%s'. Valid: %s"),
                    *StrValue, *Key, *Joined);
                return false;
            }
        }
    }

    return true;
}

// =============================================================================
// McpValidateNodeSpec
// =============================================================================
static bool McpValidateNodeSpec(
    int32 ItemIndex,
    const TSharedPtr<FJsonObject>& Item,
    TSet<FString>& InOutSeenLocalIds,
    UClass*& OutResolvedClass,
    bool& bOutAutoPrefixed,
    FMcpNodeValidationError& OutError)
{
    OutResolvedClass = nullptr;
    bOutAutoPrefixed = false;
    OutError = FMcpNodeValidationError();
    OutError.Index = ItemIndex;

    if (!Item.IsValid())
    {
        OutError.Code = TEXT("INVALID_NODE_SPEC");
        OutError.Field = TEXT("nodes[]");
        OutError.Message = TEXT("Node spec is not an object");
        return false;
    }

    // localId capture early (used in error reporting)
    FString LocalId;
    Item->TryGetStringField(TEXT("localId"), LocalId);
    OutError.LocalId = LocalId;

    // nodeType (required)
    FString NodeType;
    if (!Item->TryGetStringField(TEXT("nodeType"), NodeType) || NodeType.IsEmpty())
    {
        OutError.Code = TEXT("MISSING_NODE_TYPE");
        OutError.Field = TEXT("nodeType");
        OutError.Message = TEXT("nodeType is required");
        return false;
    }

    const FMcpMaterialExpressionCatalog& Cat = FMcpMaterialExpressionCatalog::Get();

    UClass* Resolved = Cat.ResolveClassWithAutoPrefix(NodeType, bOutAutoPrefixed);
    if (!Resolved)
    {
        OutError.Code = TEXT("INVALID_NODE_TYPE");
        OutError.Field = TEXT("nodeType");
        OutError.DidYouMean = Cat.SuggestNames(NodeType, 3);
        const FString Top = OutError.DidYouMean.Num() > 0 ? OutError.DidYouMean[0] : FString();
        OutError.Message = Top.IsEmpty()
            ? FString::Printf(TEXT("Unknown nodeType '%s'"), *NodeType)
            : FString::Printf(TEXT("Unknown nodeType '%s'; did you mean '%s'?"), *NodeType, *Top);
        return false;
    }

    // localId duplicate check
    if (!LocalId.IsEmpty())
    {
        if (InOutSeenLocalIds.Contains(LocalId))
        {
            OutError.Code = TEXT("LOCAL_ID_DUPLICATE");
            OutError.Field = TEXT("localId");
            OutError.Message = FString::Printf(TEXT("Duplicate localId '%s' within nodes[]"), *LocalId);
            return false;
        }
        InOutSeenLocalIds.Add(LocalId);
    }

    // applicability + enum value check
    TArray<FString> Ignored;
    Ignored.Add(TEXT("nodeType"));
    if (!McpValidateApplicableFields(Item, Resolved, Ignored, OutError))
    {
        return false;
    }

    OutResolvedClass = Resolved;
    return true;
}

// =============================================================================
// Cycle detection: DFS three-coloring
// =============================================================================
struct FCycleNodeKey
{
    // either an existing-graph expression pointer OR a local-id (synthetic node)
    UMaterialExpression* Expr = nullptr;
    FString              LocalId;

    bool IsLocal() const { return Expr == nullptr; }

    friend bool operator==(const FCycleNodeKey& A, const FCycleNodeKey& B)
    {
        if (A.Expr != B.Expr) return false;
        return A.LocalId == B.LocalId;
    }
    friend uint32 GetTypeHash(const FCycleNodeKey& K)
    {
        return HashCombine(GetTypeHash(K.Expr), GetTypeHash(K.LocalId));
    }
};

// =============================================================================
// McpValidateConnectionsBatch
// =============================================================================
static void McpValidateConnectionsBatch(
    const TArray<TSharedPtr<FJsonValue>>& ConnectionsJson,
    const TMap<FString, UClass*>& BatchLocalIdToClass,
    const FMcpMaterialGraphOwner& Owner,
    TArray<FMcpResolvedConnection>& OutResolved,
    TArray<FMcpConnectionValidationError>& OutErrors,
    TArray<FString>& OutSentinelWarnings)
{
    OutResolved.Reset();
    OutErrors.Reset();
    OutSentinelWarnings.Reset();

    for (int32 i = 0; i < ConnectionsJson.Num(); ++i)
    {
        const TSharedPtr<FJsonValue>& V = ConnectionsJson[i];
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!V.IsValid() || !V->TryGetObject(ObjPtr) || !ObjPtr || !ObjPtr->IsValid())
        {
            FMcpConnectionValidationError E;
            E.Index = i;
            E.Field = TEXT("connections[]");
            E.Code = TEXT("INVALID_CONNECTION");
            E.Message = TEXT("Connection entry is not an object");
            OutErrors.Add(E);
            continue;
        }
        const TSharedPtr<FJsonObject>& Obj = *ObjPtr;

        FString FromNode, ToNode, FromPin, ToPin;
        Obj->TryGetStringField(TEXT("fromNode"), FromNode);
        Obj->TryGetStringField(TEXT("toNode"), ToNode);
        Obj->TryGetStringField(TEXT("fromPin"), FromPin);
        Obj->TryGetStringField(TEXT("toPin"), ToPin);

        int32 FromOutputIndex = INDEX_NONE;
        int32 FromOutputIndexInt = INDEX_NONE;
        if (Obj->HasTypedField<EJson::Number>(TEXT("fromOutputIndex")))
        {
            FromOutputIndexInt = (int32)Obj->GetNumberField(TEXT("fromOutputIndex"));
            FromOutputIndex = FromOutputIndexInt;
        }

        if (FromNode.IsEmpty() || ToNode.IsEmpty())
        {
            FMcpConnectionValidationError E;
            E.Index = i;
            E.Field = FromNode.IsEmpty() ? TEXT("fromNode") : TEXT("toNode");
            E.Code = TEXT("INVALID_CONNECTION");
            E.Message = TEXT("fromNode and toNode are required");
            OutErrors.Add(E);
            continue;
        }

        // pin / index mutual exclusivity
        if (!FromPin.IsEmpty() && FromOutputIndex != INDEX_NONE)
        {
            FMcpConnectionValidationError E;
            E.Index = i;
            E.Field = TEXT("fromPin/fromOutputIndex");
            E.Code = TEXT("CONFLICTING_PIN_REFERENCE");
            E.Message = TEXT("Provide either fromPin or fromOutputIndex, not both");
            OutErrors.Add(E);
            continue;
        }

        FMcpResolvedConnection R;
        R.Index = i;
        R.FromPin = FromPin;
        R.FromOutputIndex = FromOutputIndex;
        R.ToPin = ToPin;

        // Resolve toNode (sentinel allowed)
        const FString ToNodeRaw = ToNode;
        if (ToNodeRaw == TEXT("$material"))
        {
            R.bToMaterialRoot = true;
        }
        else if (ToNodeRaw.Equals(TEXT("$root"), ESearchCase::IgnoreCase) ||
                 ToNodeRaw.Equals(TEXT("MaterialOutput"), ESearchCase::IgnoreCase) ||
                 ToNodeRaw.Equals(TEXT("Material"), ESearchCase::IgnoreCase) ||
                 ToNodeRaw.Equals(TEXT("Root"), ESearchCase::IgnoreCase))
        {
            R.bToMaterialRoot = true;
            OutSentinelWarnings.Add(FString::Printf(
                TEXT("toNode '%s' resolved to canonical '$material'; future calls should use '$material'"),
                *ToNodeRaw));
        }
        else if (UClass* const* BatchCls = BatchLocalIdToClass.Find(ToNodeRaw))
        {
            R.ToLocalId = ToNodeRaw;
            (void)BatchCls; // used for pin-validation below
        }
        else if (UMaterialExpression* Existing = McpFindGraphExpression(Owner, ToNodeRaw, -1))
        {
            R.ToExpression = Existing;
        }
        else
        {
            FMcpConnectionValidationError E;
            E.Index = i;
            E.Field = TEXT("toNode");
            E.Code = TEXT("NODE_NOT_FOUND");
            E.Message = FString::Printf(
                TEXT("toNode '%s' not found in batch localIds and not in existing graph"),
                *ToNodeRaw);
            OutErrors.Add(E);
            continue;
        }

        // Resolve fromNode (no sentinel)
        if (UClass* const* BatchCls = BatchLocalIdToClass.Find(FromNode))
        {
            R.FromLocalId = FromNode;
            (void)BatchCls;
        }
        else if (UMaterialExpression* Existing = McpFindGraphExpression(Owner, FromNode, -1))
        {
            R.FromExpression = Existing;
        }
        else
        {
            FMcpConnectionValidationError E;
            E.Index = i;
            E.Field = TEXT("fromNode");
            E.Code = TEXT("NODE_NOT_FOUND");
            E.Message = FString::Printf(
                TEXT("fromNode '%s' not found in batch localIds and not in existing graph"),
                *FromNode);
            OutErrors.Add(E);
            continue;
        }

        // toPin validation
        if (R.bToMaterialRoot)
        {
            if (!McpIsValidMaterialRootPinName(ToPin))
            {
                FMcpConnectionValidationError E;
                E.Index = i;
                E.Field = TEXT("toPin");
                E.Code = TEXT("INVALID_MATERIAL_ROOT_PIN");
                E.Message = FString::Printf(
                    TEXT("toPin '%s' is not a valid material root pin"), *ToPin);
                OutErrors.Add(E);
                continue;
            }
        }
        else if (!ToPin.IsEmpty())
        {
            UClass* TargetClass = nullptr;
            if (!R.ToLocalId.IsEmpty())
            {
                if (UClass* const* BatchCls2 = BatchLocalIdToClass.Find(R.ToLocalId))
                {
                    TargetClass = *BatchCls2;
                }
            }
            else if (R.ToExpression)
            {
                TargetClass = R.ToExpression->GetClass();
            }

            if (TargetClass)
            {
                TArray<FString> PinNames;
                if (!McpClassHasInputPin(TargetClass, ToPin, PinNames))
                {
                    FString Joined;
                    for (int32 j = 0; j < PinNames.Num(); ++j)
                    {
                        if (j > 0) Joined += TEXT(", ");
                        Joined += PinNames[j];
                    }
                    FMcpConnectionValidationError E;
                    E.Index = i;
                    E.Field = TEXT("toPin");
                    E.Code = TEXT("INVALID_INPUT_PIN");
                    E.Message = FString::Printf(
                        TEXT("toPin '%s' is not a valid input on %s. Valid pins: %s"),
                        *ToPin, *TargetClass->GetName(), *Joined);
                    OutErrors.Add(E);
                    continue;
                }
            }
        }

        OutResolved.Add(R);
    }

    if (OutErrors.Num() > 0)
    {
        return; // skip cycle detection if there are already errors
    }

    // Cycle detection
    // Build edges: from-key -> to-key (key = (Expr, LocalId)).
    // Existing edges: walk every existing expression for FExpressionInput pins and add edges
    // FROM the input's source expression TO the holder expression.
    // Proposed edges: each resolved connection with bToMaterialRoot=false is a real edge.

    auto MakeKey = [](UMaterialExpression* Expr, const FString& LocalId) -> FCycleNodeKey
    {
        FCycleNodeKey K;
        K.Expr = Expr;
        K.LocalId = LocalId;
        return K;
    };

    TMap<FCycleNodeKey, TArray<FCycleNodeKey>> Edges;

    // Existing edges
    if (const TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressions(Owner))
    {
        for (UMaterialExpression* Holder : *Exprs)
        {
            if (!Holder) continue;
            for (TFieldIterator<FProperty> PropIt(Holder->GetClass()); PropIt; ++PropIt)
            {
                FProperty* Prop = *PropIt;
                if (FStructProperty* SP = CastField<FStructProperty>(Prop))
                {
                    if (SP->Struct && SP->Struct->GetFName() == FName(TEXT("ExpressionInput")))
                    {
                        FExpressionInput* In = SP->ContainerPtrToValuePtr<FExpressionInput>(Holder);
                        if (In && In->Expression)
                        {
                            FCycleNodeKey From = MakeKey(In->Expression, FString());
                            FCycleNodeKey To   = MakeKey(Holder,         FString());
                            Edges.FindOrAdd(From).Add(To);
                        }
                    }
                }
            }
        }
    }

    // Proposed edges (skip material-root: root is a sink)
    for (const FMcpResolvedConnection& C : OutResolved)
    {
        if (C.bToMaterialRoot) continue;
        FCycleNodeKey From = MakeKey(C.FromExpression, C.FromLocalId);
        FCycleNodeKey To   = MakeKey(C.ToExpression,   C.ToLocalId);
        Edges.FindOrAdd(From).Add(To);
    }

    enum EColor : uint8 { White = 0, Gray = 1, Black = 2 };
    TMap<FCycleNodeKey, uint8> Color;

    auto KeyLabel = [](const FCycleNodeKey& K) -> FString
    {
        if (K.IsLocal()) return FString::Printf(TEXT("$%s"), *K.LocalId);
        return K.Expr->GetName();
    };

    TArray<FCycleNodeKey> Stack;
    bool bCycleFound = false;
    FString CyclePath;

    TFunction<void(const FCycleNodeKey&)> Visit = [&](const FCycleNodeKey& K)
    {
        if (bCycleFound) return;
        Color.FindOrAdd(K, White);
        Color[K] = Gray;
        Stack.Add(K);

        if (TArray<FCycleNodeKey>* Outgoing = Edges.Find(K))
        {
            for (const FCycleNodeKey& N : *Outgoing)
            {
                if (bCycleFound) break;
                uint8 Col = Color.FindOrAdd(N, White);
                if (Col == Gray)
                {
                    // cycle found - build path from N's first occurrence in Stack
                    int32 StartIdx = Stack.IndexOfByPredicate([&N](const FCycleNodeKey& X) { return X == N; });
                    if (StartIdx == INDEX_NONE) StartIdx = 0;
                    FString Path;
                    for (int32 i = StartIdx; i < Stack.Num(); ++i)
                    {
                        if (i > StartIdx) Path += TEXT(" -> ");
                        Path += KeyLabel(Stack[i]);
                    }
                    Path += TEXT(" -> ") + KeyLabel(N);
                    CyclePath = Path;
                    bCycleFound = true;
                    return;
                }
                if (Col == White)
                {
                    Visit(N);
                }
            }
        }

        Color[K] = Black;
        Stack.Pop();
    };

    // start from each potential root
    TArray<FCycleNodeKey> AllKeys;
    for (const auto& Pair : Edges) AllKeys.Add(Pair.Key);
    for (const auto& Pair : Edges) for (const FCycleNodeKey& V2 : Pair.Value) AllKeys.AddUnique(V2);

    for (const FCycleNodeKey& K : AllKeys)
    {
        if (bCycleFound) break;
        uint8 Col = Color.FindOrAdd(K, White);
        if (Col == White) Visit(K);
    }

    if (bCycleFound)
    {
        FMcpConnectionValidationError E;
        E.Index = INDEX_NONE;
        E.Field = TEXT("connections");
        E.Code = TEXT("CYCLIC_CONNECTION");
        E.Message = FString::Printf(TEXT("Cycle detected: %s"), *CyclePath);
        OutErrors.Add(E);
    }
}

// =============================================================================
// Apply helpers (semantic and reflected)
// =============================================================================
static EMaterialSamplerType McpReadSamplerType(
    const TSharedPtr<FJsonObject>& Item, bool& bOutPresent)
{
    bOutPresent = false;
    FString S;
    if (!Item->TryGetStringField(TEXT("samplerType"), S) || S.IsEmpty()) return SAMPLERTYPE_Color;
    bool bRecognized = false;
    EMaterialSamplerType ST = McpParseSamplerTypeString(S, &bRecognized);
    if (bRecognized) { bOutPresent = true; return ST; }
    return SAMPLERTYPE_Color;
}

static bool McpReadMipValueMode(
    const TSharedPtr<FJsonObject>& Item, ETextureMipValueMode& Out)
{
    FString S;
    if (!Item->TryGetStringField(TEXT("mipValueMode"), S) || S.IsEmpty()) return false;
    if (S == TEXT("MVM_None") || S.Equals(TEXT("None"), ESearchCase::IgnoreCase))             { Out = TMVM_None; return true; }
    if (S == TEXT("MVM_MipLevel") || S.Equals(TEXT("MipLevel"), ESearchCase::IgnoreCase))     { Out = TMVM_MipLevel; return true; }
    if (S == TEXT("MVM_MipBias") || S.Equals(TEXT("MipBias"), ESearchCase::IgnoreCase))       { Out = TMVM_MipBias; return true; }
    if (S == TEXT("MVM_Derivative") || S.Equals(TEXT("Derivative"), ESearchCase::IgnoreCase)) { Out = TMVM_Derivative; return true; }
    return false;
}

static void McpApplyChannelNames(const TSharedPtr<FJsonObject>& Item, FParameterChannelNames& Out)
{
    const TSharedPtr<FJsonObject>* ChObjPtr = nullptr;
    if (!Item->TryGetObjectField(TEXT("channelNames"), ChObjPtr) || !ChObjPtr || !ChObjPtr->IsValid()) return;
    const TSharedPtr<FJsonObject>& ChObj = *ChObjPtr;
    FString R, G, B, A;
    if (ChObj->TryGetStringField(TEXT("r"), R)) Out.R = FText::FromString(R);
    if (ChObj->TryGetStringField(TEXT("g"), G)) Out.G = FText::FromString(G);
    if (ChObj->TryGetStringField(TEXT("b"), B)) Out.B = FText::FromString(B);
    if (ChObj->TryGetStringField(TEXT("a"), A)) Out.A = FText::FromString(A);
}

static void McpApplyVectorDefaultValue(const TSharedPtr<FJsonObject>& Item, FLinearColor& Out)
{
    const TSharedPtr<FJsonObject>* VObjPtr = nullptr;
    if (!Item->TryGetObjectField(TEXT("defaultValue"), VObjPtr) || !VObjPtr || !VObjPtr->IsValid()) return;
    const TSharedPtr<FJsonObject>& VObj = *VObjPtr;
    double R = Out.R, G = Out.G, B = Out.B, A = Out.A;
    VObj->TryGetNumberField(TEXT("r"), R);
    VObj->TryGetNumberField(TEXT("g"), G);
    VObj->TryGetNumberField(TEXT("b"), B);
    VObj->TryGetNumberField(TEXT("a"), A);
    Out = FLinearColor((float)R, (float)G, (float)B, (float)A);
}

template <typename TSamplerStorage>
static void McpApplySamplerWithValidation(
    TSamplerStorage& InOutSamplerType, UTexture* Texture,
    const TSharedPtr<FJsonObject>& Item,
    TArray<FMcpSamplerWarning>& OutSamplerWarnings)
{
    bool bPresent = false;
    EMaterialSamplerType ST = McpReadSamplerType(Item, bPresent);
    if (bPresent)
    {
        InOutSamplerType = ST;
        if (Texture)
        {
            FMcpSamplerWarning W;
            if (!McpValidateSamplerTextureCompatibility(ST, Texture, W))
            {
                OutSamplerWarnings.Add(W);
            }
        }
    }
}

static void McpApplySemanticFields(
    UMaterialExpression* Expr, UClass* /*Cls*/, const TSharedPtr<FJsonObject>& Item,
    TArray<FMcpSamplerWarning>& OutSamplerWarnings)
{
    if (!Expr || !Item.IsValid()) return;

    if (UMaterialExpressionScalarParameter* P = Cast<UMaterialExpressionScalarParameter>(Expr))
    {
        FString Name; if (Item->TryGetStringField(TEXT("parameterName"), Name)) P->ParameterName = FName(*Name);
        double V;     if (Item->TryGetNumberField(TEXT("defaultValue"), V))     P->DefaultValue = (float)V;
        FString G;    if (Item->TryGetStringField(TEXT("group"), G))            P->Group = FName(*G);
        int32 SP = 0; if (Item->TryGetNumberField(TEXT("sortPriority"), SP))    P->SortPriority = SP;
        return;
    }
    if (UMaterialExpressionVectorParameter* P = Cast<UMaterialExpressionVectorParameter>(Expr))
    {
        FString Name; if (Item->TryGetStringField(TEXT("parameterName"), Name)) P->ParameterName = FName(*Name);
        McpApplyVectorDefaultValue(Item, P->DefaultValue);
        FString G;    if (Item->TryGetStringField(TEXT("group"), G))            P->Group = FName(*G);
        int32 SP = 0; if (Item->TryGetNumberField(TEXT("sortPriority"), SP))    P->SortPriority = SP;
        McpApplyChannelNames(Item, P->ChannelNames);
        return;
    }
    if (UMaterialExpressionStaticBoolParameter* P = Cast<UMaterialExpressionStaticBoolParameter>(Expr))
    {
        FString Name; if (Item->TryGetStringField(TEXT("parameterName"), Name)) P->ParameterName = FName(*Name);
        bool DV = false; if (Item->TryGetBoolField(TEXT("defaultValue"), DV))   P->DefaultValue = DV;
        FString G;    if (Item->TryGetStringField(TEXT("group"), G))            P->Group = FName(*G);
        int32 SP = 0; if (Item->TryGetNumberField(TEXT("sortPriority"), SP))    P->SortPriority = SP;
        return;
    }
    if (UMaterialExpressionStaticSwitchParameter* P = Cast<UMaterialExpressionStaticSwitchParameter>(Expr))
    {
        FString Name; if (Item->TryGetStringField(TEXT("parameterName"), Name)) P->ParameterName = FName(*Name);
        bool DV = false; if (Item->TryGetBoolField(TEXT("defaultValue"), DV))   P->DefaultValue = DV;
        FString G;    if (Item->TryGetStringField(TEXT("group"), G))            P->Group = FName(*G);
        int32 SP = 0; if (Item->TryGetNumberField(TEXT("sortPriority"), SP))    P->SortPriority = SP;
        return;
    }
    if (UMaterialExpressionStaticBool* P = Cast<UMaterialExpressionStaticBool>(Expr))
    {
        bool DV = false; if (Item->TryGetBoolField(TEXT("defaultValue"), DV))   P->Value = DV;
        return;
    }
    if (UMaterialExpressionConstant* P = Cast<UMaterialExpressionConstant>(Expr))
    {
        double DV; if (Item->TryGetNumberField(TEXT("defaultValue"), DV))       P->R = (float)DV;
        return;
    }
    if (UMaterialExpressionConstant3Vector* P = Cast<UMaterialExpressionConstant3Vector>(Expr))
    {
        McpApplyVectorDefaultValue(Item, P->Constant);
        return;
    }
    if (UMaterialExpressionConstant4Vector* P = Cast<UMaterialExpressionConstant4Vector>(Expr))
    {
        McpApplyVectorDefaultValue(Item, P->Constant);
        return;
    }
    if (UMaterialExpressionTextureSampleParameter2D* P = Cast<UMaterialExpressionTextureSampleParameter2D>(Expr))
    {
        FString Name; if (Item->TryGetStringField(TEXT("parameterName"), Name)) P->ParameterName = FName(*Name);
        FString G;    if (Item->TryGetStringField(TEXT("group"), G))            P->Group = FName(*G);
        int32 SP = 0; if (Item->TryGetNumberField(TEXT("sortPriority"), SP))    P->SortPriority = SP;
        McpApplyChannelNames(Item, P->ChannelNames);
        FString TexPath; if (Item->TryGetStringField(TEXT("texturePath"), TexPath) && !TexPath.IsEmpty())
        {
            if (UTexture* T = LoadObject<UTexture>(nullptr, *TexPath)) P->Texture = T;
        }
        McpApplySamplerWithValidation(P->SamplerType, P->Texture, Item, OutSamplerWarnings);
        int32 CI = 0; if (Item->TryGetNumberField(TEXT("coordinateIndex"), CI)) P->ConstCoordinate = (uint32)CI;
        ETextureMipValueMode MM; if (McpReadMipValueMode(Item, MM)) P->MipValueMode = MM;
        return;
    }
    if (UMaterialExpressionTextureObjectParameter* P = Cast<UMaterialExpressionTextureObjectParameter>(Expr))
    {
        FString Name; if (Item->TryGetStringField(TEXT("parameterName"), Name)) P->ParameterName = FName(*Name);
        FString G;    if (Item->TryGetStringField(TEXT("group"), G))            P->Group = FName(*G);
        int32 SP = 0; if (Item->TryGetNumberField(TEXT("sortPriority"), SP))    P->SortPriority = SP;
        FString TexPath; if (Item->TryGetStringField(TEXT("texturePath"), TexPath) && !TexPath.IsEmpty())
        {
            if (UTexture* T = LoadObject<UTexture>(nullptr, *TexPath)) P->Texture = T;
        }
        McpApplySamplerWithValidation(P->SamplerType, P->Texture, Item, OutSamplerWarnings);
        return;
    }
    if (UMaterialExpressionTextureObject* P = Cast<UMaterialExpressionTextureObject>(Expr))
    {
        FString TexPath; if (Item->TryGetStringField(TEXT("texturePath"), TexPath) && !TexPath.IsEmpty())
        {
            if (UTexture* T = LoadObject<UTexture>(nullptr, *TexPath)) P->Texture = T;
        }
        McpApplySamplerWithValidation(P->SamplerType, P->Texture, Item, OutSamplerWarnings);
        return;
    }
    if (UMaterialExpressionTextureSample* P = Cast<UMaterialExpressionTextureSample>(Expr))
    {
        FString TexPath; if (Item->TryGetStringField(TEXT("texturePath"), TexPath) && !TexPath.IsEmpty())
        {
            if (UTexture* T = LoadObject<UTexture>(nullptr, *TexPath)) P->Texture = T;
        }
        McpApplySamplerWithValidation(P->SamplerType, P->Texture, Item, OutSamplerWarnings);
        int32 CI = 0; if (Item->TryGetNumberField(TEXT("coordinateIndex"), CI)) P->ConstCoordinate = (uint32)CI;
        ETextureMipValueMode MM; if (McpReadMipValueMode(Item, MM)) P->MipValueMode = MM;
        return;
    }
    if (UMaterialExpressionTextureCoordinate* P = Cast<UMaterialExpressionTextureCoordinate>(Expr))
    {
        int32 CI = 0; if (Item->TryGetNumberField(TEXT("coordinateIndex"), CI)) P->CoordinateIndex = CI;
        double UT; if (Item->TryGetNumberField(TEXT("uTiling"), UT))            P->UTiling = (float)UT;
        double VT; if (Item->TryGetNumberField(TEXT("vTiling"), VT))            P->VTiling = (float)VT;
        return;
    }
    if (UMaterialExpressionPanner* P = Cast<UMaterialExpressionPanner>(Expr))
    {
        int32 CI = 0; if (Item->TryGetNumberField(TEXT("coordinateIndex"), CI)) P->ConstCoordinate = (uint32)CI;
        double SP; if (Item->TryGetNumberField(TEXT("speed"), SP))              { P->SpeedX = (float)SP; P->SpeedY = (float)SP; }
        return;
    }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    if (UMaterialExpressionRotator* P = Cast<UMaterialExpressionRotator>(Expr))
    {
        int32 CI = 0; if (Item->TryGetNumberField(TEXT("coordinateIndex"), CI)) P->ConstCoordinate = (uint32)CI;
        double SP; if (Item->TryGetNumberField(TEXT("speed"), SP))              P->Speed = (float)SP;
        return;
    }
#endif
}

// =============================================================================
// Apply reflected (catalog UePropName) fields. Skips MCP-semantic fields.
// =============================================================================
static int64 McpResolveEnumValue(UEnum* E, const FString& StrValue)
{
    if (!E) return INDEX_NONE;
    for (int32 i = 0; i < E->NumEnums(); ++i)
    {
        const FString FullName = E->GetNameStringByIndex(i);
        if (FullName.EndsWith(TEXT("_MAX"))) continue;
        if (FullName.Equals(StrValue, ESearchCase::IgnoreCase))
        {
            return E->GetValueByIndex(i);
        }
        const int32 UPos = FullName.Find(TEXT("_"));
        if (UPos > 0 && UPos < FullName.Len() - 1)
        {
            const FString Stripped = FullName.RightChop(UPos + 1);
            if (Stripped.Equals(StrValue, ESearchCase::IgnoreCase))
            {
                return E->GetValueByIndex(i);
            }
        }
    }
    return INDEX_NONE;
}

static void McpApplyReflectedFields(
    UMaterialExpression* Expr, UClass* Cls, const TSharedPtr<FJsonObject>& Item)
{
    if (!Expr || !Cls || !Item.IsValid()) return;
    const FMcpMaterialExpressionCatalog& Cat = FMcpMaterialExpressionCatalog::Get();
    const FString ClassName = Cls->GetName();

    for (const auto& Pair : Item->Values)
    {
        const FString& Key = Pair.Key;
        if (Key.Equals(TEXT("nodeType"), ESearchCase::CaseSensitive)) continue;
        if (Key.Equals(TEXT("localId"), ESearchCase::CaseSensitive)) continue;
        if (Key.Equals(TEXT("x"), ESearchCase::CaseSensitive)) continue;
        if (Key.Equals(TEXT("y"), ESearchCase::CaseSensitive)) continue;
        if (Key.Equals(TEXT("desc"), ESearchCase::CaseSensitive)) continue;

        const FString UePropName = Cat.GetUePropertyName(ClassName, Key);
        if (UePropName.IsEmpty()) continue; // MCP-semantic, handled elsewhere

        FProperty* Prop = Cls->FindPropertyByName(FName(*UePropName));
        if (!Prop) continue;

        if (FEnumProperty* EP = CastField<FEnumProperty>(Prop))
        {
            FString StrVal;
            if (!Pair.Value.IsValid() || !Pair.Value->TryGetString(StrVal)) continue;
            const int64 EnumVal = McpResolveEnumValue(EP->GetEnum(), StrVal);
            if (EnumVal == INDEX_NONE) continue;
            EP->GetUnderlyingProperty()->SetIntPropertyValue(EP->ContainerPtrToValuePtr<void>(Expr), EnumVal);
            continue;
        }
        if (FByteProperty* BP = CastField<FByteProperty>(Prop))
        {
            if (BP->Enum)
            {
                FString StrVal;
                if (!Pair.Value.IsValid() || !Pair.Value->TryGetString(StrVal)) continue;
                const int64 EnumVal = McpResolveEnumValue(BP->Enum, StrVal);
                if (EnumVal == INDEX_NONE) continue;
                *BP->ContainerPtrToValuePtr<uint8>(Expr) = (uint8)EnumVal;
            }
            else
            {
                double Num = 0.0;
                if (Pair.Value.IsValid() && Pair.Value->TryGetNumber(Num))
                {
                    *BP->ContainerPtrToValuePtr<uint8>(Expr) = (uint8)Num;
                }
            }
            continue;
        }
        if (FBoolProperty* BoolP = CastField<FBoolProperty>(Prop))
        {
            bool V = false;
            if (Pair.Value.IsValid() && Pair.Value->TryGetBool(V))
            {
                BoolP->SetPropertyValue_InContainer(Expr, V);
            }
            continue;
        }
        if (FNumericProperty* NP = CastField<FNumericProperty>(Prop))
        {
            double V = 0.0;
            if (Pair.Value.IsValid() && Pair.Value->TryGetNumber(V))
            {
                if (NP->IsFloatingPoint())
                {
                    NP->SetFloatingPointPropertyValue(NP->ContainerPtrToValuePtr<void>(Expr), V);
                }
                else
                {
                    NP->SetIntPropertyValue(NP->ContainerPtrToValuePtr<void>(Expr), (int64)V);
                }
            }
            continue;
        }
        if (FStrProperty* SP = CastField<FStrProperty>(Prop))
        {
            FString V;
            if (Pair.Value.IsValid() && Pair.Value->TryGetString(V))
            {
                SP->SetPropertyValue_InContainer(Expr, V);
            }
            continue;
        }
        if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
        {
            FString V;
            if (Pair.Value.IsValid() && Pair.Value->TryGetString(V))
            {
                NameProp->SetPropertyValue_InContainer(Expr, FName(*V));
            }
            continue;
        }
    }
}

// =============================================================================
// Format helpers
// =============================================================================
static TSharedPtr<FJsonValue> McpFormatNodeError(const FMcpNodeValidationError& E)
{
    TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
    O->SetStringField(TEXT("scope"),   TEXT("node"));
    O->SetNumberField(TEXT("index"),   E.Index);
    if (!E.LocalId.IsEmpty()) O->SetStringField(TEXT("localId"), E.LocalId);
    O->SetStringField(TEXT("field"),   E.Field);
    O->SetStringField(TEXT("code"),    E.Code);
    O->SetStringField(TEXT("message"), E.Message);
    if (E.DidYouMean.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> Arr;
        for (const FString& S : E.DidYouMean) Arr.Add(MakeShared<FJsonValueString>(S));
        O->SetArrayField(TEXT("didYouMean"), Arr);
    }
    return MakeShared<FJsonValueObject>(O);
}

static TSharedPtr<FJsonValue> McpFormatConnError(const FMcpConnectionValidationError& E)
{
    TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
    O->SetStringField(TEXT("scope"),   TEXT("connection"));
    O->SetNumberField(TEXT("index"),   E.Index);
    O->SetStringField(TEXT("field"),   E.Field);
    O->SetStringField(TEXT("code"),    E.Code);
    O->SetStringField(TEXT("message"), E.Message);
    return MakeShared<FJsonValueObject>(O);
}

#endif // WITH_EDITOR

} // namespace

// =============================================================================
// External entry: McpHandle_AddMaterialNodes
// =============================================================================
extern bool McpHandle_AddMaterialNodes(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool McpHandle_AddMaterialNodes(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    if (!Sub || !Payload.IsValid())
    {
        if (Sub) Sub->SendAutomationError(Socket, RequestId,
            TEXT("Invalid payload"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("assetPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (AssetPath.StartsWith(TEXT("/Engine/")) || AssetPath.StartsWith(TEXT("/EnginePlugins/")))
    {
        Sub->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Asset path '%s' is under engine content. Copy to /Game first."), *AssetPath),
            TEXT("ENGINE_ASSET_BLOCKED"));
        return true;
    }

    FMcpMaterialGraphOwner Owner;
    FString OwnerErr;
    if (!McpResolveMaterialGraphOwner(AssetPath, Owner, OwnerErr) || Owner.bReadOnly)
    {
        const FString Code = OwnerErr.Contains(TEXT("not found"))
            ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE");
        Sub->SendAutomationError(Socket, RequestId,
            OwnerErr.IsEmpty() ? TEXT("Cannot mutate this asset") : OwnerErr, Code);
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* NodesArr = nullptr;
    if (!Payload->TryGetArrayField(TEXT("nodes"), NodesArr) || !NodesArr || NodesArr->Num() == 0)
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("nodes[] is required, minimum length 1"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* ConnsArr = nullptr;
    Payload->TryGetArrayField(TEXT("connections"), ConnsArr);

    // Phase A.1: validate every node spec
    TSet<FString>                   SeenLocalIds;
    TArray<UClass*>                 ResolvedClasses;
    TArray<bool>                    AutoPrefixedFlags;
    TArray<FMcpNodeValidationError> NodeErrors;
    TArray<FString>                 AutoPrefixWarnings;
    TMap<FString, UClass*>          LocalIdToClass;

    ResolvedClasses.Reserve(NodesArr->Num());
    AutoPrefixedFlags.Reserve(NodesArr->Num());

    for (int32 i = 0; i < NodesArr->Num(); ++i)
    {
        TSharedPtr<FJsonObject> Item;
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if ((*NodesArr)[i].IsValid() && (*NodesArr)[i]->TryGetObject(ObjPtr) && ObjPtr && ObjPtr->IsValid())
        {
            Item = *ObjPtr;
        }
        UClass* Cls = nullptr;
        bool bAuto = false;
        FMcpNodeValidationError E;
        if (!McpValidateNodeSpec(i, Item, SeenLocalIds, Cls, bAuto, E))
        {
            NodeErrors.Add(E);
            ResolvedClasses.Add(nullptr);
            AutoPrefixedFlags.Add(false);
            continue;
        }
        ResolvedClasses.Add(Cls);
        AutoPrefixedFlags.Add(bAuto);

        if (bAuto)
        {
            FString OriginalNodeType;
            Item->TryGetStringField(TEXT("nodeType"), OriginalNodeType);
            AutoPrefixWarnings.Add(FString::Printf(
                TEXT("nodes[%d]: nodeType '%s' resolved to canonical '%s'"),
                i, *OriginalNodeType, *Cls->GetName()));
        }
        FString LocalId;
        Item->TryGetStringField(TEXT("localId"), LocalId);
        if (!LocalId.IsEmpty()) LocalIdToClass.Add(LocalId, Cls);
    }

    // Phase A.2: validate connections (only when no node errors)
    TArray<FMcpResolvedConnection>        ResolvedConnections;
    TArray<FMcpConnectionValidationError> ConnErrors;
    TArray<FString>                       SentinelWarnings;
    if (NodeErrors.Num() == 0 && ConnsArr)
    {
        McpValidateConnectionsBatch(*ConnsArr, LocalIdToClass, Owner,
                                    ResolvedConnections, ConnErrors, SentinelWarnings);
    }

    if (NodeErrors.Num() > 0 || ConnErrors.Num() > 0)
    {
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField  (TEXT("success"), false);
        Resp->SetStringField(TEXT("errorCode"), TEXT("VALIDATION_FAILED"));
        Resp->SetStringField(TEXT("message"),
            FString::Printf(TEXT("Batch rejected; 0 mutations applied. %d node + %d connection errors."),
                NodeErrors.Num(), ConnErrors.Num()));
        TArray<TSharedPtr<FJsonValue>> ErrorsJson;
        for (const auto& E : NodeErrors) ErrorsJson.Add(McpFormatNodeError(E));
        for (const auto& E : ConnErrors) ErrorsJson.Add(McpFormatConnError(E));
        Resp->SetArrayField(TEXT("errors"), ErrorsJson);
        Sub->SendAutomationResponse(Socket, RequestId, false,
            TEXT("validation failed"), Resp, TEXT("VALIDATION_FAILED"));
        return true;
    }

    // Phase B: apply
    FScopedTransaction Tx(NSLOCTEXT("McpAutomationBridge",
        "McpAddMaterialNodes", "MCP add_material_nodes"));
    if (Owner.Asset) Owner.Asset->Modify();

    UObject* MaterialOuter = Owner.GraphSource ? Owner.GraphSource : Owner.Asset;
    TMap<FString, UMaterialExpression*> LocalIdToExpr;
    TArray<TSharedPtr<FJsonObject>>     Mappings;
    TArray<FMcpSamplerWarning>          SamplerWarnings;

    for (int32 i = 0; i < NodesArr->Num(); ++i)
    {
        UClass* Cls = ResolvedClasses[i];
        if (!Cls) continue; // shouldn't happen since we aborted on errors

        TSharedPtr<FJsonObject> Item;
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if ((*NodesArr)[i].IsValid() && (*NodesArr)[i]->TryGetObject(ObjPtr) && ObjPtr && ObjPtr->IsValid())
        {
            Item = *ObjPtr;
        }
        if (!Item.IsValid()) continue;

        UMaterialExpression* Expr = NewObject<UMaterialExpression>(
            MaterialOuter, Cls, NAME_None, RF_Transactional);
        if (!Expr) continue;

        // Base fields
        double X = 0, Y = 0;
        Item->TryGetNumberField(TEXT("x"), X);
        Item->TryGetNumberField(TEXT("y"), Y);
        Expr->MaterialExpressionEditorX = (int32)X;
        Expr->MaterialExpressionEditorY = (int32)Y;
        Expr->MaterialExpressionGuid = FGuid::NewGuid();
        FString Desc;
        if (Item->TryGetStringField(TEXT("desc"), Desc)) Expr->Desc = Desc;

        // Type-specific
        McpApplySemanticFields(Expr, Cls, Item, SamplerWarnings);
        McpApplyReflectedFields(Expr, Cls, Item);

        TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressionsMutable(Owner);
        if (Exprs) Exprs->Add(Expr);

        FString LocalId;
        Item->TryGetStringField(TEXT("localId"), LocalId);
        if (!LocalId.IsEmpty()) LocalIdToExpr.Add(LocalId, Expr);

        TSharedPtr<FJsonObject> M = MakeShared<FJsonObject>();
        M->SetStringField(TEXT("localId"), LocalId);
        M->SetStringField(TEXT("nodeId"), Expr->GetName());
        M->SetStringField(TEXT("expressionGuid"), Expr->MaterialExpressionGuid.ToString());
        M->SetNumberField(TEXT("expressionIndex"), Exprs ? (Exprs->Num() - 1) : 0);
        Mappings.Add(M);
    }

    // Apply connections
    int32 ConnectionsApplied = 0;
    for (const FMcpResolvedConnection& C : ResolvedConnections)
    {
        UMaterialExpression* From = C.FromExpression
            ? C.FromExpression
            : LocalIdToExpr.FindRef(C.FromLocalId);

        if (!From) continue;

        if (C.bToMaterialRoot)
        {
            EMaterialProperty MP = McpMaterialPropertyFromName(C.ToPin);
            if (MP != MP_MAX)
            {
                if (UMaterialEditingLibrary::ConnectMaterialProperty(From, C.FromPin, MP))
                {
                    ++ConnectionsApplied;
                }
            }
        }
        else
        {
            UMaterialExpression* To = C.ToExpression
                ? C.ToExpression
                : LocalIdToExpr.FindRef(C.ToLocalId);
            if (To)
            {
                if (UMaterialEditingLibrary::ConnectMaterialExpressions(From, C.FromPin, To, C.ToPin))
                {
                    ++ConnectionsApplied;
                }
            }
        }
    }

    // Rebuild + optional save
    FString RebuildErr;
    McpRebuildMaterialGraphOwner(Owner, RebuildErr);

    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    bool bSaved = false;
    if (bSave)
    {
        bSaved = McpSafeAssetSave(Owner.Asset);
        if (!bSaved)
        {
            Tx.Cancel();
            Sub->SendAutomationError(Socket, RequestId,
                TEXT("Save failed; transaction rolled back"), TEXT("APPLY_FAILED"));
            return true;
        }
    }

    // Build success response
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField  (TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetNumberField(TEXT("nodesCreated"), NodesArr->Num());
    Resp->SetNumberField(TEXT("connectionsApplied"), ConnectionsApplied);
    Resp->SetBoolField  (TEXT("saved"), bSaved);

    TArray<TSharedPtr<FJsonValue>> MappingsJson;
    for (const auto& M : Mappings) MappingsJson.Add(MakeShared<FJsonValueObject>(M));
    Resp->SetArrayField(TEXT("mappings"), MappingsJson);

    TArray<TSharedPtr<FJsonValue>> WarnJson;
    for (const FString& W : AutoPrefixWarnings)
    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("code"),    TEXT("NODE_TYPE_RESOLVED"));
        O->SetStringField(TEXT("message"), W);
        WarnJson.Add(MakeShared<FJsonValueObject>(O));
    }
    for (const FString& W : SentinelWarnings)
    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("code"),    TEXT("SENTINEL_RESOLVED"));
        O->SetStringField(TEXT("message"), W);
        WarnJson.Add(MakeShared<FJsonValueObject>(O));
    }
    for (const FMcpSamplerWarning& W : SamplerWarnings)
    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("code"),     W.Code);
        O->SetStringField(TEXT("message"),  W.Message);
        O->SetStringField(TEXT("expected"), W.Expected);
        O->SetStringField(TEXT("got"),      W.Got);
        TSharedPtr<FJsonObject> TF = MakeShared<FJsonObject>();
        TF->SetStringField(TEXT("compressionSettings"), W.CompressionSettingsName);
        TF->SetBoolField  (TEXT("sRGB"),                W.bSRGB);
        TF->SetBoolField  (TEXT("isVirtualTexture"),    W.bVirtualTexture);
        O->SetObjectField (TEXT("textureFlags"),        TF);
        WarnJson.Add(MakeShared<FJsonValueObject>(O));
    }
    if (WarnJson.Num() > 0) Resp->SetArrayField(TEXT("warnings"), WarnJson);

    Sub->SendAutomationResponse(Socket, RequestId, true,
        TEXT("add_material_nodes succeeded"), Resp, FString());
    return true;
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("add_material_nodes requires editor build"), TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =============================================================================
// update_material_nodes (Task C.2)
//
// Per-item shape: { identifier, ...applicableFields } - no nodeType, no
// localId, no connections[]. Field applicability is checked against the
// resolved class of the existing expression, not a class declared in the
// payload.
// =============================================================================
namespace
{
#if WITH_EDITOR

// Levenshtein distance for name suggestions; small ASCII-friendly impl
static int32 McpLevenshtein(const FString& A, const FString& B)
{
    const int32 LA = A.Len();
    const int32 LB = B.Len();
    if (LA == 0) return LB;
    if (LB == 0) return LA;

    TArray<int32> Prev; Prev.SetNum(LB + 1);
    TArray<int32> Curr; Curr.SetNum(LB + 1);
    for (int32 j = 0; j <= LB; ++j) Prev[j] = j;

    for (int32 i = 1; i <= LA; ++i)
    {
        Curr[0] = i;
        const TCHAR Ai = A[i - 1];
        for (int32 j = 1; j <= LB; ++j)
        {
            const TCHAR Bj = B[j - 1];
            const int32 Cost = (FChar::ToLower(Ai) == FChar::ToLower(Bj)) ? 0 : 1;
            const int32 Del = Prev[j] + 1;
            const int32 Ins = Curr[j - 1] + 1;
            const int32 Sub = Prev[j - 1] + Cost;
            Curr[j] = FMath::Min3(Del, Ins, Sub);
        }
        Prev = Curr;
    }
    return Prev[LB];
}

// Top-N nearest expression names by Levenshtein distance.
static TArray<FString> McpSuggestExpressionNames(
    const FMcpMaterialGraphOwner& Owner, const FString& Query, int32 MaxCount)
{
    TArray<FString> Out;
    const TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressions(Owner);
    if (!Exprs || Exprs->Num() == 0 || Query.IsEmpty() || MaxCount <= 0) return Out;

    struct FCand { FString Name; int32 Dist = 0; };
    TArray<FCand> Cands;
    Cands.Reserve(Exprs->Num());
    for (UMaterialExpression* Expr : *Exprs)
    {
        if (!Expr) continue;
        FCand C;
        C.Name = Expr->GetName();
        C.Dist = McpLevenshtein(C.Name, Query);
        Cands.Add(C);
    }
    Cands.Sort([](const FCand& A, const FCand& B) { return A.Dist < B.Dist; });

    const int32 N = FMath::Min(MaxCount, Cands.Num());
    for (int32 i = 0; i < N; ++i) Out.Add(Cands[i].Name);
    return Out;
}

// Walk expressions for a Desc match (case-sensitive equality is fine).
static UMaterialExpression* McpFindExpressionByDesc(
    const FMcpMaterialGraphOwner& Owner, const FString& Desc)
{
    if (Desc.IsEmpty()) return nullptr;
    const TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressions(Owner);
    if (!Exprs) return nullptr;
    for (UMaterialExpression* Expr : *Exprs)
    {
        if (Expr && Expr->Desc.Equals(Desc, ESearchCase::CaseSensitive))
        {
            return Expr;
        }
    }
    return nullptr;
}

// Captures original identifier value as a printable string (used in responses).
static FString McpFormatIdentifierForDisplay(const TSharedPtr<FJsonValue>& V)
{
    if (!V.IsValid()) return TEXT("<null>");
    switch (V->Type)
    {
        case EJson::Number:
        {
            double N = V->AsNumber();
            return FString::Printf(TEXT("%g"), N);
        }
        case EJson::String:
            return V->AsString();
        case EJson::Boolean:
            return V->AsBool() ? TEXT("true") : TEXT("false");
        case EJson::Null:
            return TEXT("null");
        case EJson::Object:
            return TEXT("<object>");
        case EJson::Array:
            return TEXT("<array>");
        default:
            return TEXT("<unknown>");
    }
}

// Resolves a single per-item identifier (mixed-type JSON value) to an
// existing graph expression. Fills OutError on miss.
//
// Priority for a non-numeric, non-numeric-string, non-GUID string:
//   GUID > expressionName > expressionPath > parameterName > desc
// (note: McpFindGraphExpression handles GUID/name/path/parameterName in that
// order; we add a desc fallback here.)
static bool McpResolveUpdateIdentifier(
    const FMcpMaterialGraphOwner& Owner,
    const TSharedPtr<FJsonValue>& IdentifierJson,
    UMaterialExpression*& OutExpression,
    FMcpNodeValidationError& OutError)
{
    OutExpression = nullptr;

    if (!IdentifierJson.IsValid() || IdentifierJson->Type == EJson::Null)
    {
        OutError.Code = TEXT("INVALID_IDENTIFIER_TYPE");
        OutError.Field = TEXT("identifier");
        OutError.Message = TEXT("identifier is required (number or non-numeric string)");
        return false;
    }

    if (IdentifierJson->Type == EJson::Number)
    {
        const int32 Idx = (int32)IdentifierJson->AsNumber();
        const TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressions(Owner);
        const int32 Total = Exprs ? Exprs->Num() : 0;
        UMaterialExpression* Expr = McpFindGraphExpression(Owner, FString(), Idx);
        if (!Expr)
        {
            OutError.Code = TEXT("NODE_NOT_FOUND");
            OutError.Field = TEXT("identifier");
            OutError.Message = FString::Printf(
                TEXT("Index %d is out of range; graph has %d expressions"), Idx, Total);
            return false;
        }
        OutExpression = Expr;
        return true;
    }

    if (IdentifierJson->Type == EJson::String)
    {
        const FString S = IdentifierJson->AsString();
        if (S.IsEmpty())
        {
            OutError.Code = TEXT("INVALID_IDENTIFIER_TYPE");
            OutError.Field = TEXT("identifier");
            OutError.Message = TEXT("identifier string is empty");
            return false;
        }

        // numeric-string is ambiguous; reject explicitly
        if (S.IsNumeric())
        {
            OutError.Code = TEXT("INVALID_IDENTIFIER_TYPE");
            OutError.Field = TEXT("identifier");
            OutError.Message = FString::Printf(
                TEXT("numeric-string identifier '%s' is ambiguous - pass a JSON number for index lookup, or a non-numeric string for name lookup"),
                *S);
            return false;
        }

        // GUID
        FGuid ParsedGuid;
        if (FGuid::Parse(S, ParsedGuid))
        {
            const TArray<TObjectPtr<UMaterialExpression>>* Exprs = McpGetGraphExpressions(Owner);
            if (Exprs)
            {
                for (UMaterialExpression* Expr : *Exprs)
                {
                    if (Expr && Expr->MaterialExpressionGuid == ParsedGuid)
                    {
                        OutExpression = Expr;
                        return true;
                    }
                }
            }
            OutError.Code = TEXT("NODE_NOT_FOUND");
            OutError.Field = TEXT("identifier");
            OutError.Message = FString::Printf(TEXT("No expression with GUID '%s'"), *S);
            return false;
        }

        // name / path / parameterName
        if (UMaterialExpression* Found = McpFindGraphExpression(Owner, S, -1))
        {
            OutExpression = Found;
            return true;
        }

        // desc fallback
        if (UMaterialExpression* Found = McpFindExpressionByDesc(Owner, S))
        {
            OutExpression = Found;
            return true;
        }

        OutError.Code = TEXT("NODE_NOT_FOUND");
        OutError.Field = TEXT("identifier");
        OutError.DidYouMean = McpSuggestExpressionNames(Owner, S, 3);
        const FString Top = OutError.DidYouMean.Num() > 0 ? OutError.DidYouMean[0] : FString();
        OutError.Message = Top.IsEmpty()
            ? FString::Printf(TEXT("No expression matches identifier '%s'"), *S)
            : FString::Printf(TEXT("No expression matches identifier '%s'; did you mean '%s'?"), *S, *Top);
        return false;
    }

    // bool / object / array
    OutError.Code = TEXT("INVALID_IDENTIFIER_TYPE");
    OutError.Field = TEXT("identifier");
    OutError.Message = FString::Printf(
        TEXT("identifier must be a number or non-numeric string; got %s"),
        *McpFormatIdentifierForDisplay(IdentifierJson));
    return false;
}

// Validates fields of a per-item update spec against the resolved class.
// Wraps the shared applicability+enum loop, ignoring "identifier".
static bool McpValidateUpdateNodeSpec(
    int32 ItemIndex,
    const TSharedPtr<FJsonObject>& Item,
    UClass* ResolvedClass,
    FMcpNodeValidationError& OutError)
{
    OutError = FMcpNodeValidationError();
    OutError.Index = ItemIndex;

    if (!Item.IsValid())
    {
        OutError.Code = TEXT("INVALID_NODE_SPEC");
        OutError.Field = TEXT("nodes[]");
        OutError.Message = TEXT("Update spec is not an object");
        return false;
    }
    if (!ResolvedClass)
    {
        OutError.Code = TEXT("INVALID_NODE_SPEC");
        OutError.Field = TEXT("identifier");
        OutError.Message = TEXT("ResolvedClass is null");
        return false;
    }

    TArray<FString> Ignored;
    Ignored.Add(TEXT("identifier"));
    // localId and nodeType are add-only meta keys; on update they are silently
    // skipped (no-op from the caller's perspective). See McpIsUpdateMetaKey.
    Ignored.Add(TEXT("localId"));
    Ignored.Add(TEXT("nodeType"));
    return McpValidateApplicableFields(Item, ResolvedClass, Ignored, OutError);
}

// Top-level keys consumed directly by the apply pipeline (not user fields).
// localId and nodeType are add-only concepts; on update they are silently
// skipped so they do not appear in fieldsUpdated[] or trigger any apply work.
static bool McpIsUpdateMetaKey(const FString& Key)
{
    return Key.Equals(TEXT("identifier"), ESearchCase::CaseSensitive)
        || Key.Equals(TEXT("localId"),    ESearchCase::CaseSensitive)
        || Key.Equals(TEXT("nodeType"),   ESearchCase::CaseSensitive);
}

// Top-level node fields handled by C.1's apply loop directly (not via
// reflected/semantic apply). We keep the same set for parity.
static bool McpIsTopLevelNodeField(const FString& Key)
{
    return Key.Equals(TEXT("x"), ESearchCase::CaseSensitive)
        || Key.Equals(TEXT("y"), ESearchCase::CaseSensitive)
        || Key.Equals(TEXT("desc"), ESearchCase::CaseSensitive);
}

#endif // WITH_EDITOR
} // namespace

extern bool McpHandle_UpdateMaterialNodes(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool McpHandle_UpdateMaterialNodes(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    if (!Sub || !Payload.IsValid())
    {
        if (Sub) Sub->SendAutomationError(Socket, RequestId,
            TEXT("Invalid payload"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("assetPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (AssetPath.StartsWith(TEXT("/Engine/")) || AssetPath.StartsWith(TEXT("/EnginePlugins/")))
    {
        Sub->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Asset path '%s' is under engine content. Copy to /Game first."), *AssetPath),
            TEXT("ENGINE_ASSET_BLOCKED"));
        return true;
    }

    FMcpMaterialGraphOwner Owner;
    FString OwnerErr;
    if (!McpResolveMaterialGraphOwner(AssetPath, Owner, OwnerErr) || Owner.bReadOnly)
    {
        const FString Code = OwnerErr.Contains(TEXT("not found"))
            ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE");
        Sub->SendAutomationError(Socket, RequestId,
            OwnerErr.IsEmpty() ? TEXT("Cannot mutate this asset") : OwnerErr, Code);
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* NodesArr = nullptr;
    if (!Payload->TryGetArrayField(TEXT("nodes"), NodesArr) || !NodesArr || NodesArr->Num() == 0)
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("nodes[] is required, minimum length 1"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Phase A: resolve identifier per item, validate fields against resolved class.
    // All-or-nothing - same posture as add_material_nodes (Task C.1).
    struct FResolvedUpdate
    {
        UMaterialExpression* Expr = nullptr;
        UClass*              Cls  = nullptr;
        TSharedPtr<FJsonObject> Item;
        FString              IdentifierDisplay;
        TArray<FString>      RequestedFields;  // user-facing field keys (excluding "identifier")
    };
    TArray<FResolvedUpdate>          Resolved;
    TArray<FMcpNodeValidationError>  NodeErrors;
    Resolved.Reserve(NodesArr->Num());

    for (int32 i = 0; i < NodesArr->Num(); ++i)
    {
        FResolvedUpdate R;
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!(*NodesArr)[i].IsValid() || !(*NodesArr)[i]->TryGetObject(ObjPtr) || !ObjPtr || !ObjPtr->IsValid())
        {
            FMcpNodeValidationError E;
            E.Index = i;
            E.Code = TEXT("INVALID_NODE_SPEC");
            E.Field = TEXT("nodes[]");
            E.Message = TEXT("Update spec is not an object");
            NodeErrors.Add(E);
            Resolved.Add(R);
            continue;
        }
        R.Item = *ObjPtr;

        const TSharedPtr<FJsonValue> IdentifierJson = R.Item->TryGetField(TEXT("identifier"));
        R.IdentifierDisplay = McpFormatIdentifierForDisplay(IdentifierJson);

        FMcpNodeValidationError E;
        E.Index = i;
        UMaterialExpression* Expr = nullptr;
        if (!McpResolveUpdateIdentifier(Owner, IdentifierJson, Expr, E))
        {
            NodeErrors.Add(E);
            Resolved.Add(R);
            continue;
        }
        R.Expr = Expr;
        R.Cls  = Expr->GetClass();

        FMcpNodeValidationError VE;
        if (!McpValidateUpdateNodeSpec(i, R.Item, R.Cls, VE))
        {
            NodeErrors.Add(VE);
            Resolved.Add(R);
            continue;
        }

        // Capture which fields the caller asked to update (anything except "identifier").
        // We include x, y, desc since C.1's apply loop also handles these as "updated".
        for (const auto& Pair : R.Item->Values)
        {
            const FString& Key = Pair.Key;
            if (McpIsUpdateMetaKey(Key)) continue;
            R.RequestedFields.Add(Key);
        }

        Resolved.Add(R);
    }

    if (NodeErrors.Num() > 0)
    {
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField  (TEXT("success"), false);
        Resp->SetStringField(TEXT("errorCode"), TEXT("VALIDATION_FAILED"));
        Resp->SetStringField(TEXT("message"),
            FString::Printf(TEXT("Batch rejected; 0 mutations applied. %d node errors."),
                NodeErrors.Num()));
        TArray<TSharedPtr<FJsonValue>> ErrorsJson;
        for (const auto& E : NodeErrors) ErrorsJson.Add(McpFormatNodeError(E));
        Resp->SetArrayField(TEXT("errors"), ErrorsJson);
        Sub->SendAutomationResponse(Socket, RequestId, false,
            TEXT("validation failed"), Resp, TEXT("VALIDATION_FAILED"));
        return true;
    }

    // Phase B: apply
    FScopedTransaction Tx(NSLOCTEXT("McpAutomationBridge",
        "McpUpdateMaterialNodes", "MCP update_material_nodes"));
    if (Owner.Asset) Owner.Asset->Modify();

    TArray<TSharedPtr<FJsonObject>> ResultsJson;
    TArray<FMcpSamplerWarning>      SamplerWarnings;

    for (const FResolvedUpdate& R : Resolved)
    {
        if (!R.Expr || !R.Cls || !R.Item.IsValid()) continue;

        R.Expr->Modify();

        // Top-level base fields (parity with add_material_nodes apply loop)
        double X = R.Expr->MaterialExpressionEditorX;
        if (R.Item->TryGetNumberField(TEXT("x"), X)) R.Expr->MaterialExpressionEditorX = (int32)X;
        double Y = R.Expr->MaterialExpressionEditorY;
        if (R.Item->TryGetNumberField(TEXT("y"), Y)) R.Expr->MaterialExpressionEditorY = (int32)Y;
        FString Desc;
        if (R.Item->TryGetStringField(TEXT("desc"), Desc)) R.Expr->Desc = Desc;

        McpApplySemanticFields(R.Expr, R.Cls, R.Item, SamplerWarnings);
        McpApplyReflectedFields(R.Expr, R.Cls, R.Item);

        TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("identifier"),     R.IdentifierDisplay);
        Item->SetStringField(TEXT("expressionName"), R.Expr->GetName());
        Item->SetStringField(TEXT("expressionGuid"), R.Expr->MaterialExpressionGuid.ToString());
        TArray<TSharedPtr<FJsonValue>> Fields;
        for (const FString& F : R.RequestedFields)
        {
            Fields.Add(MakeShared<FJsonValueString>(F));
        }
        Item->SetArrayField(TEXT("fieldsUpdated"), Fields);
        ResultsJson.Add(Item);
    }

    // Rebuild + optional save
    FString RebuildErr;
    McpRebuildMaterialGraphOwner(Owner, RebuildErr);

    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    bool bSaved = false;
    if (bSave)
    {
        bSaved = McpSafeAssetSave(Owner.Asset);
        if (!bSaved)
        {
            Tx.Cancel();
            Sub->SendAutomationError(Socket, RequestId,
                TEXT("Save failed; transaction rolled back"), TEXT("APPLY_FAILED"));
            return true;
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField  (TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetNumberField(TEXT("nodesUpdated"), Resolved.Num());
    Resp->SetBoolField  (TEXT("saved"), bSaved);

    TArray<TSharedPtr<FJsonValue>> ResultsArr;
    for (const auto& R : ResultsJson) ResultsArr.Add(MakeShared<FJsonValueObject>(R));
    Resp->SetArrayField(TEXT("results"), ResultsArr);

    TArray<TSharedPtr<FJsonValue>> WarnJson;
    for (const FMcpSamplerWarning& W : SamplerWarnings)
    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("code"),     W.Code);
        O->SetStringField(TEXT("message"),  W.Message);
        O->SetStringField(TEXT("expected"), W.Expected);
        O->SetStringField(TEXT("got"),      W.Got);
        TSharedPtr<FJsonObject> TF = MakeShared<FJsonObject>();
        TF->SetStringField(TEXT("compressionSettings"), W.CompressionSettingsName);
        TF->SetBoolField  (TEXT("sRGB"),                W.bSRGB);
        TF->SetBoolField  (TEXT("isVirtualTexture"),    W.bVirtualTexture);
        O->SetObjectField (TEXT("textureFlags"),        TF);
        WarnJson.Add(MakeShared<FJsonValueObject>(O));
    }
    if (WarnJson.Num() > 0) Resp->SetArrayField(TEXT("warnings"), WarnJson);

    Sub->SendAutomationResponse(Socket, RequestId, true,
        TEXT("update_material_nodes succeeded"), Resp, FString());
    return true;
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("update_material_nodes requires editor build"), TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =============================================================================
// Test-only forwarders — exposed so unit tests can exercise the validators
// without needing an asset/socket fixture. Defined in "Tests" namespace via
// thin shims; callers in the Tests/ folder reference these by name.
// =============================================================================
namespace McpAddMaterialNodesValidationForTests
{
    bool ValidateNodeSpec(
        int32 ItemIndex,
        const TSharedPtr<FJsonObject>& Item,
        TSet<FString>& InOutSeenLocalIds,
        UClass*& OutResolvedClass,
        bool& bOutAutoPrefixed,
        FString& OutCode,
        FString& OutField,
        FString& OutMessage,
        TArray<FString>& OutDidYouMean)
    {
#if WITH_EDITOR
        FMcpNodeValidationError E;
        const bool b = McpValidateNodeSpec(ItemIndex, Item, InOutSeenLocalIds,
                                           OutResolvedClass, bOutAutoPrefixed, E);
        OutCode = E.Code;
        OutField = E.Field;
        OutMessage = E.Message;
        OutDidYouMean = E.DidYouMean;
        return b;
#else
        OutResolvedClass = nullptr;
        bOutAutoPrefixed = false;
        OutCode = TEXT("EDITOR_ONLY");
        return false;
#endif
    }
}

namespace McpUpdateMaterialNodesValidationForTests
{
#if WITH_EDITOR
    bool ResolveIdentifier(
        const FMcpMaterialGraphOwner& Owner,
        const TSharedPtr<FJsonValue>& IdentifierJson,
        UMaterialExpression*& OutExpression,
        FString& OutCode,
        FString& OutField,
        FString& OutMessage,
        TArray<FString>& OutDidYouMean)
    {
        FMcpNodeValidationError E;
        const bool b = McpResolveUpdateIdentifier(Owner, IdentifierJson, OutExpression, E);
        OutCode = E.Code;
        OutField = E.Field;
        OutMessage = E.Message;
        OutDidYouMean = E.DidYouMean;
        return b;
    }

    bool ValidateUpdateFields(
        const TSharedPtr<FJsonObject>& Item,
        UClass* ResolvedClass,
        FString& OutCode,
        FString& OutField,
        FString& OutMessage)
    {
        FMcpNodeValidationError E;
        const bool b = McpValidateUpdateNodeSpec(0, Item, ResolvedClass, E);
        OutCode = E.Code;
        OutField = E.Field;
        OutMessage = E.Message;
        return b;
    }

    // Mirrors the RequestedFields collection in McpHandle_UpdateMaterialNodes
    // (Phase A loop, around line 1721). Skips meta keys that the apply pipeline
    // does not act on, so fieldsUpdated[] never reports them.
    void BuildRequestedFields(
        const TSharedPtr<FJsonObject>& Item,
        TArray<FString>& OutRequestedFields)
    {
        OutRequestedFields.Reset();
        if (!Item.IsValid()) return;
        for (const auto& Pair : Item->Values)
        {
            const FString& Key = Pair.Key;
            if (McpIsUpdateMetaKey(Key)) continue;
            OutRequestedFields.Add(Key);
        }
    }
#endif
}

// =============================================================================
// External entry: McpHandle_RemoveMaterialNodes (Task C.3)
//
// Payload: { assetPath, identifiers: [...], save?: bool }
// Phase A: resolve each identifier via McpResolveUpdateIdentifier (mixed-array
//          rules from C.2). All-or-nothing - any miss aborts the batch.
// Phase B: open transaction, Modify() asset and each expression, remove via
//          ExpressionCollection.RemoveExpression (UE 5.1+) plus
//          RemoveExpressionParameter for UMaterial owners. Single rebuild
//          after the loop. Optional save - failure rolls back.
// =============================================================================
extern bool McpHandle_RemoveMaterialNodes(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool McpHandle_RemoveMaterialNodes(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    if (!Sub || !Payload.IsValid())
    {
        if (Sub) Sub->SendAutomationError(Socket, RequestId,
            TEXT("Invalid payload"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("assetPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (AssetPath.StartsWith(TEXT("/Engine/")) || AssetPath.StartsWith(TEXT("/EnginePlugins/")))
    {
        Sub->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Asset path '%s' is under engine content. Copy to /Game first."), *AssetPath),
            TEXT("ENGINE_ASSET_BLOCKED"));
        return true;
    }

    FMcpMaterialGraphOwner Owner;
    FString OwnerErr;
    if (!McpResolveMaterialGraphOwner(AssetPath, Owner, OwnerErr))
    {
        const FString Code = OwnerErr.Contains(TEXT("not found"))
            ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE");
        Sub->SendAutomationError(Socket, RequestId,
            OwnerErr.IsEmpty() ? TEXT("Cannot mutate this asset") : OwnerErr, Code);
        return true;
    }
    if (Owner.bReadOnly)
    {
        // mirrors the legacy single-node remove handler
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("Cannot remove nodes from a MaterialFunctionInstance - edit the parent function instead"),
            TEXT("UNSUPPORTED_OPERATION"));
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* IdentifiersArr = nullptr;
    if (!Payload->TryGetArrayField(TEXT("identifiers"), IdentifiersArr) || !IdentifiersArr || IdentifiersArr->Num() == 0)
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("identifiers[] is required, minimum length 1"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Phase A: resolve each identifier; collect ALL errors and abort all-or-nothing.
    struct FResolvedRemove
    {
        UMaterialExpression* Expr = nullptr;
        FString              IdentifierDisplay;
    };
    TArray<FResolvedRemove>          Resolved;
    TArray<FMcpNodeValidationError>  NodeErrors;
    Resolved.Reserve(IdentifiersArr->Num());

    // dedupe expressions to avoid double-remove (both as protective measure
    // and so nodesRemoved reflects unique nodes)
    TSet<UMaterialExpression*> SeenExprs;

    for (int32 i = 0; i < IdentifiersArr->Num(); ++i)
    {
        const TSharedPtr<FJsonValue> IdentifierJson = (*IdentifiersArr)[i];

        FResolvedRemove R;
        R.IdentifierDisplay = McpFormatIdentifierForDisplay(IdentifierJson);

        FMcpNodeValidationError E;
        E.Index = i;
        UMaterialExpression* Expr = nullptr;
        if (!McpResolveUpdateIdentifier(Owner, IdentifierJson, Expr, E))
        {
            NodeErrors.Add(E);
            Resolved.Add(R);
            continue;
        }
        R.Expr = Expr;
        if (SeenExprs.Contains(Expr))
        {
            // skip duplicate; do not error - silent dedupe matches "remove once" semantics
            continue;
        }
        SeenExprs.Add(Expr);
        Resolved.Add(R);
    }

    if (NodeErrors.Num() > 0)
    {
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField  (TEXT("success"), false);
        Resp->SetStringField(TEXT("errorCode"), TEXT("VALIDATION_FAILED"));
        Resp->SetStringField(TEXT("message"),
            FString::Printf(TEXT("Batch rejected; 0 mutations applied. %d node errors."),
                NodeErrors.Num()));
        TArray<TSharedPtr<FJsonValue>> ErrorsJson;
        for (const auto& E : NodeErrors) ErrorsJson.Add(McpFormatNodeError(E));
        Resp->SetArrayField(TEXT("errors"), ErrorsJson);
        Sub->SendAutomationResponse(Socket, RequestId, false,
            TEXT("validation failed"), Resp, TEXT("VALIDATION_FAILED"));
        return true;
    }

    // Phase B: apply
    FScopedTransaction Tx(NSLOCTEXT("McpAutomationBridge",
        "McpRemoveMaterialNodes", "MCP remove_material_nodes"));
    if (Owner.Asset) Owner.Asset->Modify();

    TArray<TSharedPtr<FJsonObject>> ResultsJson;

    for (const FResolvedRemove& R : Resolved)
    {
        if (!R.Expr) continue;

        const FString RemovedName = R.Expr->GetName();
        const FString RemovedGuid = R.Expr->MaterialExpressionGuid.ToString();

        R.Expr->Modify();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        if (Owner.Kind == EMcpMaterialGraphOwnerKind::Material)
        {
            UMaterial* Mat = CastChecked<UMaterial>(Owner.GraphSource);
            Mat->GetEditorOnlyData()->ExpressionCollection.RemoveExpression(R.Expr);
            Mat->RemoveExpressionParameter(R.Expr);
        }
        else
        {
            UMaterialFunction* Func = CastChecked<UMaterialFunction>(Owner.GraphSource);
            Func->GetEditorOnlyData()->ExpressionCollection.RemoveExpression(R.Expr);
        }
#else
        // UE 5.0 fallback (project is 5.7 - dead branch but kept for parity)
        if (TArray<TObjectPtr<UMaterialExpression>>* ExprPtr = McpGetGraphExpressionsMutable(Owner))
        {
            ExprPtr->Remove(R.Expr);
        }
        if (Owner.Kind == EMcpMaterialGraphOwnerKind::Material)
        {
            CastChecked<UMaterial>(Owner.GraphSource)->RemoveExpressionParameter(R.Expr);
        }
#endif

        TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("identifier"),     R.IdentifierDisplay);
        Item->SetStringField(TEXT("expressionName"), RemovedName);
        Item->SetStringField(TEXT("expressionGuid"), RemovedGuid);
        ResultsJson.Add(Item);
    }

    // single rebuild after all removals
    FString RebuildErr;
    McpRebuildMaterialGraphOwner(Owner, RebuildErr);

    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    bool bSaved = false;
    if (bSave)
    {
        bSaved = McpSafeAssetSave(Owner.Asset);
        if (!bSaved)
        {
            Tx.Cancel();
            Sub->SendAutomationError(Socket, RequestId,
                TEXT("Save failed; transaction rolled back"), TEXT("APPLY_FAILED"));
            return true;
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField  (TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetNumberField(TEXT("nodesRemoved"), ResultsJson.Num());
    Resp->SetBoolField  (TEXT("saved"), bSaved);

    TArray<TSharedPtr<FJsonValue>> ResultsArr;
    for (const auto& R : ResultsJson) ResultsArr.Add(MakeShared<FJsonValueObject>(R));
    Resp->SetArrayField(TEXT("results"), ResultsArr);

    Sub->SendAutomationResponse(Socket, RequestId, true,
        TEXT("remove_material_nodes succeeded"), Resp, FString());
    return true;
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("remove_material_nodes requires editor build"), TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =============================================================================
// connect_material_pins (Task C.4)
//
// Standalone batch handler. Re-uses McpValidateConnectionsBatch (the same
// validator add_material_nodes uses for its inline connections[]). Passes an
// empty BatchLocalIdToClass since no nodes are being created here - every
// fromNode/toNode must resolve to an existing graph expression or the
// $material sentinel.
// =============================================================================
extern bool McpHandle_ConnectMaterialPins(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool McpHandle_ConnectMaterialPins(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    if (!Sub || !Payload.IsValid())
    {
        if (Sub) Sub->SendAutomationError(Socket, RequestId,
            TEXT("Invalid payload"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("assetPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (AssetPath.StartsWith(TEXT("/Engine/")) || AssetPath.StartsWith(TEXT("/EnginePlugins/")))
    {
        Sub->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Asset path '%s' is under engine content. Copy to /Game first."), *AssetPath),
            TEXT("ENGINE_ASSET_BLOCKED"));
        return true;
    }

    FMcpMaterialGraphOwner Owner;
    FString OwnerErr;
    if (!McpResolveMaterialGraphOwner(AssetPath, Owner, OwnerErr) || Owner.bReadOnly)
    {
        const FString Code = OwnerErr.Contains(TEXT("not found"))
            ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE");
        Sub->SendAutomationError(Socket, RequestId,
            OwnerErr.IsEmpty() ? TEXT("Cannot mutate this asset") : OwnerErr, Code);
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* ConnsArr = nullptr;
    if (!Payload->TryGetArrayField(TEXT("connections"), ConnsArr) || !ConnsArr || ConnsArr->Num() == 0)
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("connections[] is required, minimum length 1"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Phase A: validate connections against existing graph (no batch-localId pool)
    TMap<FString, UClass*>                EmptyBatch;
    TArray<FMcpResolvedConnection>        ResolvedConnections;
    TArray<FMcpConnectionValidationError> ConnErrors;
    TArray<FString>                       SentinelWarnings;
    McpValidateConnectionsBatch(*ConnsArr, EmptyBatch, Owner,
                                ResolvedConnections, ConnErrors, SentinelWarnings);

    if (ConnErrors.Num() > 0)
    {
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField  (TEXT("success"), false);
        Resp->SetStringField(TEXT("errorCode"), TEXT("VALIDATION_FAILED"));
        Resp->SetStringField(TEXT("message"),
            FString::Printf(TEXT("Batch rejected; 0 mutations applied. %d connection errors."),
                ConnErrors.Num()));
        TArray<TSharedPtr<FJsonValue>> ErrorsJson;
        for (const auto& E : ConnErrors) ErrorsJson.Add(McpFormatConnError(E));
        Resp->SetArrayField(TEXT("errors"), ErrorsJson);
        Sub->SendAutomationResponse(Socket, RequestId, false,
            TEXT("validation failed"), Resp, TEXT("VALIDATION_FAILED"));
        return true;
    }

    // Phase B: apply
    FScopedTransaction Tx(NSLOCTEXT("McpAutomationBridge",
        "McpConnectMaterialPins", "MCP connect_material_pins"));
    if (Owner.Asset) Owner.Asset->Modify();

    int32 ConnectionsApplied = 0;
    for (const FMcpResolvedConnection& C : ResolvedConnections)
    {
        // for connect_material_pins there are no batch-localIds; FromExpression
        // and (when not bToMaterialRoot) ToExpression must be already resolved
        UMaterialExpression* From = C.FromExpression;
        if (!From) continue;

        if (C.bToMaterialRoot)
        {
            const EMaterialProperty MP = McpMaterialPropertyFromName(C.ToPin);
            if (MP != MP_MAX)
            {
                if (UMaterialEditingLibrary::ConnectMaterialProperty(From, C.FromPin, MP))
                {
                    ++ConnectionsApplied;
                }
            }
        }
        else
        {
            UMaterialExpression* To = C.ToExpression;
            if (To)
            {
                if (UMaterialEditingLibrary::ConnectMaterialExpressions(From, C.FromPin, To, C.ToPin))
                {
                    ++ConnectionsApplied;
                }
            }
        }
    }

    // single rebuild after all connections
    FString RebuildErr;
    McpRebuildMaterialGraphOwner(Owner, RebuildErr);

    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    bool bSaved = false;
    if (bSave)
    {
        bSaved = McpSafeAssetSave(Owner.Asset);
        if (!bSaved)
        {
            Tx.Cancel();
            Sub->SendAutomationError(Socket, RequestId,
                TEXT("Save failed; transaction rolled back"), TEXT("APPLY_FAILED"));
            return true;
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField  (TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetNumberField(TEXT("connectionsApplied"), ConnectionsApplied);
    Resp->SetBoolField  (TEXT("saved"), bSaved);

    TArray<TSharedPtr<FJsonValue>> WarnJson;
    for (const FString& W : SentinelWarnings)
    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("code"),    TEXT("SENTINEL_RESOLVED"));
        O->SetStringField(TEXT("message"), W);
        WarnJson.Add(MakeShared<FJsonValueObject>(O));
    }
    if (WarnJson.Num() > 0) Resp->SetArrayField(TEXT("warnings"), WarnJson);

    Sub->SendAutomationResponse(Socket, RequestId, true,
        TEXT("connect_material_pins succeeded"), Resp, FString());
    return true;
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("connect_material_pins requires editor build"), TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =============================================================================
// break_material_connections (Task C.4)
//
// Per-item shape: { fromNode?, fromPin?, toNode, toPin }. The (toNode, toPin)
// pair identifies the input being disconnected.
//   - toNode == "$material" (or canonical aliases) => disconnect a root pin
//     on the material's editor-only data (BaseColor, Metallic, etc.).
//   - otherwise toNode resolves to an existing expression; toPin names one of
//     its FExpressionInput fields.
// fromNode (optional) lets the caller assert the current source; if provided
// and the resolved input is bound to a different source, the entry fails with
// CONNECTION_NOT_FOUND. If omitted, ALL connections to the named input are
// broken (regardless of source).
// =============================================================================
namespace
{
#if WITH_EDITOR

// Finds a (UStruct, FProperty) descriptor for an FExpressionInput-derived
// struct field whose name matches PinName (case-insensitive). Walks all struct
// properties of OwningClass; any UScriptStruct that is FExpressionInput or a
// subtype of it counts as a candidate. Returns the matched FStructProperty
// (or nullptr) and fills OutAllPinNames with every candidate seen.
static FStructProperty* McpFindExpressionInputProperty(
    UClass* OwningClass,
    const FString& PinName,
    TArray<FString>& OutAllPinNames)
{
    OutAllPinNames.Reset();
    if (!OwningClass) return nullptr;
    FStructProperty* Match = nullptr;
    const FName ExprInputName(TEXT("ExpressionInput"));
    for (TFieldIterator<FProperty> PropIt(OwningClass); PropIt; ++PropIt)
    {
        FStructProperty* SP = CastField<FStructProperty>(*PropIt);
        if (!SP || !SP->Struct) continue;

        // Walk the struct's super chain to detect FExpressionInput ancestry.
        // Plain FExpressionInput pins (on UMaterialExpression subclasses) match
        // directly; root pins on the material's editor-only data class are
        // FColorMaterialInput / FScalarMaterialInput / etc., which derive from
        // FMaterialInput<T> -> FExpressionInput.
        bool bIsExprInput = false;
        for (UStruct* S = SP->Struct; S; S = S->GetSuperStruct())
        {
            if (S->GetFName() == ExprInputName) { bIsExprInput = true; break; }
        }
        if (!bIsExprInput) continue;

        const FString Name = SP->GetName();
        OutAllPinNames.Add(Name);
        if (!Match && Name.Equals(PinName, ESearchCase::IgnoreCase))
        {
            Match = SP;
        }
    }
    return Match;
}

// Resolves the (Class, ContainerObj) pair for a break-side toNode.
//   - root sentinel => Class = MaterialEditorOnlyData class, Container = the
//     editor-only data UObject pointer.
//   - expression => Class = expression class, Container = the expression.
// Returns false on any resolution failure; OutCode/OutMessage describe it.
struct FMcpBreakTarget
{
    bool                 bRoot = false;
    UMaterialExpression* Expr = nullptr;     // when bRoot=false
    UObject*             Container = nullptr; // expression OR editor-only data
    UClass*              ContainerClass = nullptr;
    FString              ResolvedToNodeDisplay; // for results[]
};

static bool McpResolveBreakTarget(
    const FMcpMaterialGraphOwner& Owner,
    const FString& ToNodeRaw,
    FMcpBreakTarget& Out,
    FString& OutCode,
    FString& OutField,
    FString& OutMessage,
    bool& bOutSentinelAlias,
    FString& OutAliasUsed)
{
    bOutSentinelAlias = false;
    OutAliasUsed.Reset();

    if (ToNodeRaw.IsEmpty())
    {
        OutCode = TEXT("INVALID_ARGUMENT");
        OutField = TEXT("toNode");
        OutMessage = TEXT("toNode is required");
        return false;
    }

    // root sentinel + aliases (mirrors McpValidateConnectionsBatch)
    bool bSentinel = false;
    if (ToNodeRaw == TEXT("$material"))
    {
        bSentinel = true;
    }
    else if (ToNodeRaw.Equals(TEXT("$root"), ESearchCase::IgnoreCase) ||
             ToNodeRaw.Equals(TEXT("MaterialOutput"), ESearchCase::IgnoreCase) ||
             ToNodeRaw.Equals(TEXT("Material"), ESearchCase::IgnoreCase) ||
             ToNodeRaw.Equals(TEXT("Root"), ESearchCase::IgnoreCase))
    {
        bSentinel = true;
        bOutSentinelAlias = true;
        OutAliasUsed = ToNodeRaw;
    }

    if (bSentinel)
    {
        if (Owner.Kind != EMcpMaterialGraphOwnerKind::Material)
        {
            OutCode = TEXT("UNSUPPORTED_OPERATION");
            OutField = TEXT("toNode");
            OutMessage = TEXT("Root pin disconnect requires a UMaterial owner; this asset is a MaterialFunction");
            return false;
        }
        UMaterial* Mat = CastChecked<UMaterial>(Owner.GraphSource);
#if MCP_HAS_MATERIAL_EDITOR_ONLY_DATA
        UObject* EditorData = Mat->GetEditorOnlyData();
        if (!EditorData)
        {
            OutCode = TEXT("APPLY_FAILED");
            OutField = TEXT("toNode");
            OutMessage = TEXT("Material has no editor-only data");
            return false;
        }
        Out.bRoot = true;
        Out.Container = EditorData;
        Out.ContainerClass = EditorData->GetClass();
        Out.ResolvedToNodeDisplay = TEXT("$material");
        return true;
#else
        Out.bRoot = true;
        Out.Container = Mat;
        Out.ContainerClass = Mat->GetClass();
        Out.ResolvedToNodeDisplay = TEXT("$material");
        return true;
#endif
    }

    // expression resolution
    if (UMaterialExpression* Expr = McpFindGraphExpression(Owner, ToNodeRaw, -1))
    {
        Out.bRoot = false;
        Out.Expr = Expr;
        Out.Container = Expr;
        Out.ContainerClass = Expr->GetClass();
        Out.ResolvedToNodeDisplay = Expr->GetName();
        return true;
    }

    OutCode = TEXT("NODE_NOT_FOUND");
    OutField = TEXT("toNode");
    OutMessage = FString::Printf(TEXT("toNode '%s' not found in graph"), *ToNodeRaw);
    return false;
}

// Resolves the FExpressionInput* for a break target + pin name. For root
// targets (bRoot=true) this MUST go through UMaterial::GetExpressionInputForProperty
// because the concrete pin types (FColorMaterialInput, FScalarMaterialInput,
// FShadingModelMaterialInput, FSubstrateMaterialInput, FVectorMaterialInput,
// FVector2MaterialInput) derive from FMaterialInput<T>, which is declared
// noexport in UE 5.7's reflection - the FExpressionInput ancestor is not
// reachable via UStruct::GetSuperStruct walks.
//
// For non-root targets we fall back to the reflective McpFindExpressionInputProperty
// path (works because plain UMaterialExpression pins ARE FExpressionInput-typed,
// and FMaterialAttributesInput inherits FExpressionInput directly with full
// reflection metadata).
//
// On miss, OutCode is set to INPUT_NOT_FOUND with a helpful message.
static FExpressionInput* McpResolveBreakInputPointer(
    UMaterial* OwnerMaterialOrNull,
    const FMcpBreakTarget& Target,
    const FString& ToPin,
    EMaterialProperty& OutRootProperty,
    FStructProperty*& OutInputProp,
    FString& OutCode,
    FString& OutMessage)
{
    OutRootProperty = MP_MAX;
    OutInputProp = nullptr;
    OutCode.Reset();
    OutMessage.Reset();

    if (Target.bRoot)
    {
        const EMaterialProperty Prop = McpMaterialPropertyFromName(ToPin);
        if (Prop == MP_MAX)
        {
            static const TCHAR* const KnownRootPins =
                TEXT("BaseColor, Metallic, Roughness, Specular, Normal, EmissiveColor, ")
                TEXT("Opacity, OpacityMask, WorldPositionOffset, Refraction, ")
                TEXT("AmbientOcclusion, PixelDepthOffset, MaterialAttributes");
            OutCode = TEXT("INPUT_NOT_FOUND");
            OutMessage = FString::Printf(
                TEXT("toPin '%s' is not a valid root pin on $material. Valid pins: %s"),
                *ToPin, KnownRootPins);
            return nullptr;
        }
        if (!OwnerMaterialOrNull)
        {
            OutCode = TEXT("UNSUPPORTED_OPERATION");
            OutMessage = TEXT("Root pin requires UMaterial owner");
            return nullptr;
        }
        FExpressionInput* RootIn = OwnerMaterialOrNull->GetExpressionInputForProperty(Prop);
        if (!RootIn)
        {
            OutCode = TEXT("INPUT_NOT_FOUND");
            OutMessage = FString::Printf(
                TEXT("Root pin '%s' is not available on this material (UMaterial::GetExpressionInputForProperty returned null)"),
                *ToPin);
            return nullptr;
        }
        OutRootProperty = Prop;
        return RootIn;
    }

    TArray<FString> AvailablePins;
    FStructProperty* Prop = McpFindExpressionInputProperty(Target.ContainerClass, ToPin, AvailablePins);
    if (!Prop)
    {
        FString Joined;
        for (int32 j = 0; j < AvailablePins.Num(); ++j)
        {
            if (j > 0) Joined += TEXT(", ");
            Joined += AvailablePins[j];
        }
        OutCode = TEXT("INPUT_NOT_FOUND");
        OutMessage = FString::Printf(
            TEXT("toPin '%s' is not a valid input on %s. Valid pins: %s"),
            *ToPin, Target.ContainerClass ? *Target.ContainerClass->GetName() : TEXT("?"), *Joined);
        return nullptr;
    }
    OutInputProp = Prop;
    return Prop->ContainerPtrToValuePtr<FExpressionInput>(Target.Container);
}

#endif // WITH_EDITOR
} // namespace

// =============================================================================
// Test-only forwarder for break-side root-pin resolution. Mirrors the Phase A
// path that maps (sentinel target, toPin) -> live FExpressionInput*. Tests
// drive this with a transient UMaterial to verify that the new GetExpressionInputForProperty-
// based root-pin lookup actually finds wired inputs (the prior reflection-only
// path missed every root pin because FColorMaterialInput etc. derive from a
// noexport FMaterialInput<T>).
// =============================================================================
namespace McpBreakMaterialConnectionsForTests
{
#if WITH_EDITOR
    bool ResolveRootInput(
        UMaterial* Material,
        const FString& ToPin,
        FExpressionInput*& OutInput,
        EMaterialProperty& OutProperty,
        FString& OutCode,
        FString& OutMessage)
    {
        OutInput = nullptr;
        OutProperty = MP_MAX;
        OutCode.Reset();
        OutMessage.Reset();
        if (!Material)
        {
            OutCode = TEXT("UNSUPPORTED_OPERATION");
            OutMessage = TEXT("Material is null");
            return false;
        }
        FMcpBreakTarget T;
        T.bRoot = true;
#if MCP_HAS_MATERIAL_EDITOR_ONLY_DATA
        T.Container = Material->GetEditorOnlyData();
        T.ContainerClass = T.Container ? T.Container->GetClass() : nullptr;
#else
        T.Container = Material;
        T.ContainerClass = Material->GetClass();
#endif
        FStructProperty* DummyProp = nullptr;
        OutInput = McpResolveBreakInputPointer(Material, T, ToPin, OutProperty, DummyProp, OutCode, OutMessage);
        return OutInput != nullptr;
    }
#endif
}

extern bool McpHandle_BreakMaterialConnections(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool McpHandle_BreakMaterialConnections(
    UMcpAutomationBridgeSubsystem* Sub,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    if (!Sub || !Payload.IsValid())
    {
        if (Sub) Sub->SendAutomationError(Socket, RequestId,
            TEXT("Invalid payload"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("assetPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (AssetPath.StartsWith(TEXT("/Engine/")) || AssetPath.StartsWith(TEXT("/EnginePlugins/")))
    {
        Sub->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Asset path '%s' is under engine content. Copy to /Game first."), *AssetPath),
            TEXT("ENGINE_ASSET_BLOCKED"));
        return true;
    }

    FMcpMaterialGraphOwner Owner;
    FString OwnerErr;
    if (!McpResolveMaterialGraphOwner(AssetPath, Owner, OwnerErr) || Owner.bReadOnly)
    {
        const FString Code = OwnerErr.Contains(TEXT("not found"))
            ? TEXT("ASSET_NOT_FOUND") : TEXT("UNSUPPORTED_ASSET_TYPE");
        Sub->SendAutomationError(Socket, RequestId,
            OwnerErr.IsEmpty() ? TEXT("Cannot mutate this asset") : OwnerErr, Code);
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* ConnsArr = nullptr;
    if (!Payload->TryGetArrayField(TEXT("connections"), ConnsArr) || !ConnsArr || ConnsArr->Num() == 0)
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("connections[] is required, minimum length 1"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Phase A: resolve every entry; collect ALL errors and abort all-or-nothing.
    struct FResolvedBreak
    {
        int32                Index = INDEX_NONE;
        // resolved target
        FMcpBreakTarget      Target;
        FStructProperty*     InputProp = nullptr;     // for non-root: FExpressionInput-typed property on Target.ContainerClass
        FExpressionInput*    RootInput = nullptr;     // for root: pointer obtained via UMaterial::GetExpressionInputForProperty
        EMaterialProperty    RootProperty = MP_MAX;   // for root: resolved EMaterialProperty
        // optional source-match assertion
        bool                 bMatchSource = false;
        UMaterialExpression* MatchSource = nullptr;
        bool                 bMatchFromPin = false;
        FString              MatchFromPin;            // when fromPin specified, also assert OutputName/Mask
        // echo fields
        FString              FromNodeRaw;
        FString              FromPin;
        FString              ToNodeRaw;
        FString              ToPin;
        // pre-break source descriptor (filled at apply time)
    };

    TArray<FResolvedBreak>                Resolved;
    TArray<FMcpConnectionValidationError> Errors;
    TArray<FString>                       SentinelWarnings;
    Resolved.Reserve(ConnsArr->Num());

    for (int32 i = 0; i < ConnsArr->Num(); ++i)
    {
        const TSharedPtr<FJsonValue>& V = (*ConnsArr)[i];
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!V.IsValid() || !V->TryGetObject(ObjPtr) || !ObjPtr || !ObjPtr->IsValid())
        {
            FMcpConnectionValidationError E;
            E.Index = i;
            E.Field = TEXT("connections[]");
            E.Code = TEXT("INVALID_CONNECTION");
            E.Message = TEXT("Connection entry is not an object");
            Errors.Add(E);
            continue;
        }
        const TSharedPtr<FJsonObject>& Obj = *ObjPtr;

        FResolvedBreak R;
        R.Index = i;
        Obj->TryGetStringField(TEXT("fromNode"), R.FromNodeRaw);
        Obj->TryGetStringField(TEXT("fromPin"),  R.FromPin);
        Obj->TryGetStringField(TEXT("toNode"),   R.ToNodeRaw);
        Obj->TryGetStringField(TEXT("toPin"),    R.ToPin);

        if (R.ToNodeRaw.IsEmpty())
        {
            FMcpConnectionValidationError E;
            E.Index = i;
            E.Field = TEXT("toNode");
            E.Code = TEXT("NODE_NOT_FOUND");
            E.Message = TEXT("toNode is required");
            Errors.Add(E);
            continue;
        }
        if (R.ToPin.IsEmpty())
        {
            FMcpConnectionValidationError E;
            E.Index = i;
            E.Field = TEXT("toPin");
            E.Code = TEXT("INVALID_ARGUMENT");
            E.Message = TEXT("toPin is required");
            Errors.Add(E);
            continue;
        }

        // resolve toNode (sentinel or expression)
        FString TgtCode, TgtField, TgtMsg, AliasUsed;
        bool bAlias = false;
        if (!McpResolveBreakTarget(Owner, R.ToNodeRaw, R.Target, TgtCode, TgtField, TgtMsg, bAlias, AliasUsed))
        {
            FMcpConnectionValidationError E;
            E.Index = i;
            E.Field = TgtField;
            E.Code = TgtCode;
            E.Message = TgtMsg;
            Errors.Add(E);
            continue;
        }
        if (bAlias)
        {
            SentinelWarnings.Add(FString::Printf(
                TEXT("toNode '%s' resolved to canonical '$material'; future calls should use '$material'"),
                *AliasUsed));
        }

        // resolve toPin: root pins use UMaterial::GetExpressionInputForProperty
        // (concrete types like FColorMaterialInput derive from FMaterialInput<T>
        // which is noexport in 5.7's reflection - super-walk never finds the
        // FExpressionInput ancestor). Expression-to-expression breaks still use
        // the reflective path.
        UMaterial* OwnerMatOrNull = (Owner.Kind == EMcpMaterialGraphOwnerKind::Material)
            ? Cast<UMaterial>(Owner.GraphSource) : nullptr;

        FString PinCode, PinMsg;
        FExpressionInput* CurrentInput = McpResolveBreakInputPointer(
            OwnerMatOrNull, R.Target, R.ToPin,
            R.RootProperty, R.InputProp, PinCode, PinMsg);
        if (!CurrentInput)
        {
            FMcpConnectionValidationError E;
            E.Index = i;
            E.Field = TEXT("toPin");
            E.Code = PinCode.IsEmpty() ? TEXT("INPUT_NOT_FOUND") : PinCode;
            E.Message = PinMsg;
            Errors.Add(E);
            continue;
        }
        if (R.Target.bRoot) R.RootInput = CurrentInput;

        if (CurrentInput->Expression == nullptr)
        {
            FMcpConnectionValidationError E;
            E.Index = i;
            E.Field = TEXT("toPin");
            E.Code = TEXT("CONNECTION_NOT_FOUND");
            E.Message = FString::Printf(
                TEXT("Input '%s' on '%s' is not currently connected"),
                *R.ToPin, *R.Target.ResolvedToNodeDisplay);
            Errors.Add(E);
            continue;
        }

        // optional fromNode assertion: input must be bound to that source
        if (!R.FromNodeRaw.IsEmpty())
        {
            UMaterialExpression* Asserted = McpFindGraphExpression(Owner, R.FromNodeRaw, -1);
            if (!Asserted)
            {
                FMcpConnectionValidationError E;
                E.Index = i;
                E.Field = TEXT("fromNode");
                E.Code = TEXT("NODE_NOT_FOUND");
                E.Message = FString::Printf(TEXT("fromNode '%s' not found in graph"), *R.FromNodeRaw);
                Errors.Add(E);
                continue;
            }
            if (CurrentInput->Expression != Asserted)
            {
                FMcpConnectionValidationError E;
                E.Index = i;
                E.Field = TEXT("fromNode");
                E.Code = TEXT("CONNECTION_NOT_FOUND");
                E.Message = FString::Printf(
                    TEXT("the input is connected, but to a different source than fromNode='%s'"),
                    *R.FromNodeRaw);
                Errors.Add(E);
                continue;
            }
            R.bMatchSource = true;
            R.MatchSource = Asserted;
        }

        Resolved.Add(R);
    }

    if (Errors.Num() > 0)
    {
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField  (TEXT("success"), false);
        Resp->SetStringField(TEXT("errorCode"), TEXT("VALIDATION_FAILED"));
        Resp->SetStringField(TEXT("message"),
            FString::Printf(TEXT("Batch rejected; 0 mutations applied. %d connection errors."),
                Errors.Num()));
        TArray<TSharedPtr<FJsonValue>> ErrorsJson;
        for (const auto& E : Errors) ErrorsJson.Add(McpFormatConnError(E));
        Resp->SetArrayField(TEXT("errors"), ErrorsJson);
        Sub->SendAutomationResponse(Socket, RequestId, false,
            TEXT("validation failed"), Resp, TEXT("VALIDATION_FAILED"));
        return true;
    }

    // Phase B: apply
    FScopedTransaction Tx(NSLOCTEXT("McpAutomationBridge",
        "McpBreakMaterialConnections", "MCP break_material_connections"));
    if (Owner.Asset) Owner.Asset->Modify();

    TArray<TSharedPtr<FJsonObject>> ResultsJson;
    int32 ConnectionsBroken = 0;

    for (const FResolvedBreak& R : Resolved)
    {
        if (!R.Target.Container) continue;

        // Resolve the live FExpressionInput pointer the same way Phase A did.
        // For root pins: re-fetch via GetExpressionInputForProperty (handles
        // FColorMaterialInput etc. without reflection). For non-root: reflective
        // ContainerPtrToValuePtr through the cached FStructProperty.
        FExpressionInput* In = nullptr;
        if (R.Target.bRoot)
        {
            UMaterial* Mat = Cast<UMaterial>(Owner.GraphSource);
            if (Mat) In = Mat->GetExpressionInputForProperty(R.RootProperty);
        }
        else if (R.InputProp)
        {
            In = R.InputProp->ContainerPtrToValuePtr<FExpressionInput>(R.Target.Container);
        }
        if (!In) continue;

        // capture pre-break source for the response
        FString BrokenSourceName;
        FString BrokenSourcePin;
        if (In->Expression)
        {
            BrokenSourceName = In->Expression->GetName();
            // Derive the source pin name from the connected expression's
            // Outputs[OutputIndex] (FExpressionInput stores only the index;
            // the name lives on the source's output array).
            const TArray<FExpressionOutput>& Outs = In->Expression->GetOutputs();
            if (Outs.IsValidIndex(In->OutputIndex))
            {
                const FExpressionOutput& Out = Outs[In->OutputIndex];
                if (!Out.OutputName.IsNone())
                {
                    BrokenSourcePin = Out.OutputName.ToString();
                }
            }
        }

        // Modify the holder so transaction snapshots both sides
        if (R.Target.Container) R.Target.Container->Modify();
        if (R.Target.Expr) R.Target.Expr->Modify();

        In->Expression = nullptr;
        In->OutputIndex = 0;
        ++ConnectionsBroken;

        TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
        if (!R.FromNodeRaw.IsEmpty()) Item->SetStringField(TEXT("fromNode"), R.FromNodeRaw);
        if (!R.FromPin.IsEmpty())     Item->SetStringField(TEXT("fromPin"),  R.FromPin);
        Item->SetStringField(TEXT("toNode"), R.Target.ResolvedToNodeDisplay);
        Item->SetStringField(TEXT("toPin"),  R.ToPin);
        Item->SetStringField(TEXT("brokenSourceName"), BrokenSourceName);
        Item->SetStringField(TEXT("brokenSourcePin"),  BrokenSourcePin);
        ResultsJson.Add(Item);
    }

    // single rebuild after all breaks
    FString RebuildErr;
    McpRebuildMaterialGraphOwner(Owner, RebuildErr);

    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    bool bSaved = false;
    if (bSave)
    {
        bSaved = McpSafeAssetSave(Owner.Asset);
        if (!bSaved)
        {
            Tx.Cancel();
            Sub->SendAutomationError(Socket, RequestId,
                TEXT("Save failed; transaction rolled back"), TEXT("APPLY_FAILED"));
            return true;
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField  (TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetNumberField(TEXT("connectionsBroken"), ConnectionsBroken);
    Resp->SetBoolField  (TEXT("saved"), bSaved);

    TArray<TSharedPtr<FJsonValue>> ResultsArr;
    for (const auto& R : ResultsJson) ResultsArr.Add(MakeShared<FJsonValueObject>(R));
    Resp->SetArrayField(TEXT("results"), ResultsArr);

    TArray<TSharedPtr<FJsonValue>> WarnJson;
    for (const FString& W : SentinelWarnings)
    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("code"),    TEXT("SENTINEL_RESOLVED"));
        O->SetStringField(TEXT("message"), W);
        WarnJson.Add(MakeShared<FJsonValueObject>(O));
    }
    if (WarnJson.Num() > 0) Resp->SetArrayField(TEXT("warnings"), WarnJson);

    Sub->SendAutomationResponse(Socket, RequestId, true,
        TEXT("break_material_connections succeeded"), Resp, FString());
    return true;
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("break_material_connections requires editor build"), TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
