#include "McpMaterialExpressionCatalog.h"

#include "CoreMinimal.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"

#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionStaticBool.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionRotator.h"

// External: canonical undecorated pin name (defined in McpAutomationBridge_Material_GraphWrites.cpp).
extern FString McpGetUndecoratedInputName(UMaterialExpression* Expr, int32 InputIndex);

namespace
{
    // Convert PascalCase to camelCase by lowercasing first character.
    FString PascalToCamel(const FString& Pascal)
    {
        if (Pascal.IsEmpty())
        {
            return Pascal;
        }
        FString Out = Pascal;
        Out[0] = FChar::ToLower(Out[0]);
        return Out;
    }

    // Strip per-enum prefix at first underscore - "WPT_Default" -> "Default".
    // If no underscore or underscore is at position 0 or there's nothing after, keep full name.
    FString StripEnumPrefix(const FString& Name)
    {
        int32 UnderscoreIdx = INDEX_NONE;
        if (!Name.FindChar(TEXT('_'), UnderscoreIdx))
        {
            return Name;
        }
        if (UnderscoreIdx <= 0 || UnderscoreIdx + 1 >= Name.Len())
        {
            return Name;
        }
        return Name.Mid(UnderscoreIdx + 1);
    }

    // Two-row Levenshtein distance.
    int32 LevenshteinDistance(const FString& A, const FString& B)
    {
        const int32 M = A.Len();
        const int32 N = B.Len();
        if (M == 0) return N;
        if (N == 0) return M;

        TArray<int32> Prev;
        TArray<int32> Curr;
        Prev.SetNum(N + 1);
        Curr.SetNum(N + 1);

        for (int32 j = 0; j <= N; ++j)
        {
            Prev[j] = j;
        }

        for (int32 i = 1; i <= M; ++i)
        {
            Curr[0] = i;
            const TCHAR Ai = A[i - 1];
            for (int32 j = 1; j <= N; ++j)
            {
                const TCHAR Bj = B[j - 1];
                const int32 Cost = (Ai == Bj) ? 0 : 1;
                const int32 Del = Prev[j] + 1;
                const int32 Ins = Curr[j - 1] + 1;
                const int32 Sub = Prev[j - 1] + Cost;
                int32 Best = Del < Ins ? Del : Ins;
                if (Sub < Best) Best = Sub;
                Curr[j] = Best;
            }
            Swap(Prev, Curr);
        }

        return Prev[N];
    }

    void AddUnique(TArray<FString>& Arr, const FString& Item)
    {
        if (!Arr.Contains(Item))
        {
            Arr.Add(Item);
        }
    }
}

const FMcpMaterialExpressionCatalog& FMcpMaterialExpressionCatalog::Get()
{
    static FMcpMaterialExpressionCatalog Instance;
    return Instance;
}

FMcpMaterialExpressionCatalog::FMcpMaterialExpressionCatalog()
{
    Build();
}

void FMcpMaterialExpressionCatalog::Build()
{
    // Reserved set of UE property names already covered by MCP-semantic hardcoded fields.
    // Reflection walk skips these to avoid double-listing under camelCase reflected names.
    static const TSet<FString> SemanticReserved = {
        TEXT("ParameterName"), TEXT("Group"), TEXT("SortPriority"),
        TEXT("DefaultValue"), TEXT("Texture"), TEXT("SamplerType"),
        TEXT("ConstCoordinate"), TEXT("MipValueMode"), TEXT("ChannelNames"),
        TEXT("UTiling"), TEXT("VTiling"), TEXT("Speed"),
        TEXT("MaterialExpressionEditorX"), TEXT("MaterialExpressionEditorY"),
        TEXT("Desc"), TEXT("MaterialExpressionGuid")
    };

    UClass* BaseClass = UMaterialExpression::StaticClass();
    if (!BaseClass)
    {
        return;
    }

    for (TObjectIterator<UClass> It; It; ++It)
    {
        UClass* Cls = *It;
        if (!Cls || Cls == BaseClass)
        {
            continue;
        }
        if (!Cls->IsChildOf(BaseClass))
        {
            continue;
        }
        if (Cls->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
        {
            continue;
        }

        const FString ClassName = Cls->GetName();
        NameToClass.Add(ClassName, Cls);
        AllClassNames.Add(ClassName);

        // MenuCategories - UPROPERTY(config) on UMaterialExpression, populated onto the CDO from
        // BaseMaterialExpressions.ini. NOT UCLASS metadata - reading via Cls->GetMetaData yields
        // nothing. Description - GetCreationDescription() virtual is UE's canonical short blurb
        // (used by the material editor palette). ToolTip UCLASS metadata is empty on stock UE.
        TArray<FString>& Categories = NameToCategories.Add(ClassName);
        const UMaterialExpression* CDO = Cast<UMaterialExpression>(
            Cls->GetDefaultObject(/*bCreateIfNeeded*/ true));
        if (CDO)
        {
            for (const FText& Cat : CDO->MenuCategories)
            {
                FString S = Cat.ToString();
                if (!S.IsEmpty()) Categories.Add(MoveTemp(S));
            }
        }

#if WITH_EDITOR
        // Description fallback chain (CDO-safe — Desc is empty on the CDO so base virtuals
        // write nothing; overrides typically write static type-level strings):
        //   1. GetCreationDescription() - canonical short blurb, overridden by ~3 classes
        //   2. GetExpressionToolTip()   - overridden by ~49 classes with hardcoded help text
        //   3. GetCaption()[0]          - overridden by nearly all classes; first caption line
        //                                 is the node-type display name (subsequent lines are
        //                                 instance-specific, e.g. parameter name)
        FString Description;
        if (CDO)
        {
            Description = CDO->GetCreationDescription().ToString();
            if (Description.IsEmpty())
            {
                UMaterialExpression* MutableCDO = CastChecked<UMaterialExpression>(
                    Cls->GetDefaultObject(true));
                TArray<FString> ToolTipLines;
                MutableCDO->GetExpressionToolTip(ToolTipLines);
                Description = FString::Join(ToolTipLines, TEXT(" "));
                Description.TrimStartAndEndInline();
            }
            if (Description.IsEmpty())
            {
                TArray<FString> Captions;
                CDO->GetCaption(Captions);
                if (Captions.Num() > 0)
                {
                    Description = Captions[0];
                    Description.TrimStartAndEndInline();
                }
            }
        }
        NameToDescription.Add(ClassName, Description);
#else
        NameToDescription.Add(ClassName, FString());
#endif

        // Build applicable fields - common base set.
        TArray<FString>& Fields = NameToApplicableFields.Add(ClassName);
        Fields.Add(TEXT("localId"));
        Fields.Add(TEXT("x"));
        Fields.Add(TEXT("y"));
        Fields.Add(TEXT("desc"));

        // MCP-semantic fields per spec section 7.2.
        // Note: Texture sample parameters (UMaterialExpressionTextureSampleParameter and
        // descendants) are a separate hierarchy from UMaterialExpressionParameter but expose
        // the same ParameterName / Group / SortPriority semantics, so include them too.
        const bool bHasParameterSemantics =
            Cls->IsChildOf(UMaterialExpressionParameter::StaticClass()) ||
            Cls->IsChildOf(UMaterialExpressionTextureSampleParameter::StaticClass());
        if (bHasParameterSemantics)
        {
            AddUnique(Fields, TEXT("parameterName"));
            AddUnique(Fields, TEXT("group"));
            AddUnique(Fields, TEXT("sortPriority"));
        }
        if (Cls->IsChildOf(UMaterialExpressionTextureBase::StaticClass()))
        {
            AddUnique(Fields, TEXT("texturePath"));
            AddUnique(Fields, TEXT("samplerType"));
            AddUnique(Fields, TEXT("coordinateIndex"));
            AddUnique(Fields, TEXT("mipValueMode"));
        }
        if (Cls == UMaterialExpressionConstant::StaticClass()
            || Cls == UMaterialExpressionScalarParameter::StaticClass())
        {
            AddUnique(Fields, TEXT("defaultValue"));
        }
        if (Cls == UMaterialExpressionConstant3Vector::StaticClass()
            || Cls == UMaterialExpressionConstant4Vector::StaticClass()
            || Cls == UMaterialExpressionVectorParameter::StaticClass())
        {
            AddUnique(Fields, TEXT("defaultValue"));
        }
        if (Cls == UMaterialExpressionStaticBool::StaticClass()
            || Cls == UMaterialExpressionStaticBoolParameter::StaticClass()
            || Cls == UMaterialExpressionStaticSwitchParameter::StaticClass())
        {
            AddUnique(Fields, TEXT("defaultValue"));
        }
        if (Cls == UMaterialExpressionVectorParameter::StaticClass()
            || Cls == UMaterialExpressionTextureSampleParameter2D::StaticClass())
        {
            AddUnique(Fields, TEXT("channelNames"));
        }
        if (Cls == UMaterialExpressionTextureCoordinate::StaticClass()
            || Cls == UMaterialExpressionPanner::StaticClass()
            || Cls == UMaterialExpressionRotator::StaticClass())
        {
            AddUnique(Fields, TEXT("uTiling"));
            AddUnique(Fields, TEXT("vTiling"));
        }
        if (Cls == UMaterialExpressionPanner::StaticClass()
            || Cls == UMaterialExpressionRotator::StaticClass())
        {
            AddUnique(Fields, TEXT("speed"));
        }

        // Reflection-driven discovery of class-specific fields.
        TMap<FString, FString>& UePropMap = NameToFieldUeProp.Add(ClassName);
        TMap<FString, TArray<FString>>& EnumMap = NameToFieldEnums.Add(ClassName);

        for (TFieldIterator<FProperty> PropIt(Cls); PropIt; ++PropIt)
        {
            FProperty* Prop = *PropIt;
            if (!Prop)
            {
                continue;
            }

            // Skip pin properties: FExpressionInput / FExpressionOutput structs.
            if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
            {
                if (StructProp->Struct)
                {
                    const FName StructName = StructProp->Struct->GetFName();
                    if (StructName == TEXT("ExpressionInput") || StructName == TEXT("ExpressionOutput"))
                    {
                        continue;
                    }
                }
            }

            // Skip properties not editable / not blueprint-visible.
            const bool bEdit = Prop->HasAnyPropertyFlags(CPF_Edit);
            const bool bBpVisible = Prop->HasAnyPropertyFlags(CPF_BlueprintVisible);
            if (!bEdit && !bBpVisible)
            {
                continue;
            }

            // Skip transient properties.
            if (Prop->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_NonTransactional))
            {
                continue;
            }

            const FString UePropName = Prop->GetName();

            // Skip MCP-semantic-reserved property names.
            if (SemanticReserved.Contains(UePropName))
            {
                continue;
            }

            const FString CamelName = PascalToCamel(UePropName);
            AddUnique(Fields, CamelName);
            UePropMap.Add(CamelName, UePropName);

            // Enum value capture: FEnumProperty and FByteProperty with non-null Enum.
            UEnum* EnumPtr = nullptr;
            if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
            {
                EnumPtr = EnumProp->GetEnum();
            }
            else if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
            {
                EnumPtr = ByteProp->Enum;
            }

            if (EnumPtr)
            {
                TArray<FString>& EnumValues = EnumMap.Add(CamelName);
                const int32 NumEntries = EnumPtr->NumEnums();
                for (int32 i = 0; i < NumEntries; ++i)
                {
                    const FString EntryName = EnumPtr->GetNameStringByIndex(i);
                    if (EntryName.EndsWith(TEXT("_MAX")))
                    {
                        continue;
                    }
#if WITH_EDITOR
                    if (EnumPtr->HasMetaData(TEXT("Hidden"), i))
                    {
                        continue;
                    }
#endif
                    EnumValues.Add(StripEnumPrefix(EntryName));
                }
            }
        }

        // Pin enumeration from CDO.
        // Inputs: walk FExpressionInputIterator (declaration order via UMaterialExpression::GetInput).
        // PropertyName comes from a parallel reflection pass; set only when it differs from Name.
        // Outputs: iterate the CDO's Outputs UPROPERTY (FExpressionOutput::OutputName is the handle).
        TArray<FMcpPinInfo>& InputPins = NameToInputPins.Add(ClassName);
        TArray<FMcpPinInfo>& OutputPins = NameToOutputPins.Add(ClassName);
        if (UMaterialExpression* MutableCDO = Cast<UMaterialExpression>(Cls->GetDefaultObject(true)))
        {
            TMap<const FExpressionInput*, FString> PtrToPropName;
            for (TFieldIterator<FProperty> PinPropIt(Cls); PinPropIt; ++PinPropIt)
            {
                if (FStructProperty* StructProp = CastField<FStructProperty>(*PinPropIt))
                {
                    if (StructProp->Struct
                        && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput")))
                    {
                        const FExpressionInput* P = StructProp->ContainerPtrToValuePtr<FExpressionInput>(MutableCDO);
                        PtrToPropName.Add(P, StructProp->GetName());
                    }
                }
            }

            for (FExpressionInputIterator PinIt{ MutableCDO }; PinIt; ++PinIt)
            {
                FMcpPinInfo Pin;
                Pin.Index = PinIt.Index;
                // Use the canonical undecorated form for invariant naming across
                // catalog, consumers JSON, and connect/break resolver.
                Pin.Name  = McpGetUndecoratedInputName(MutableCDO, PinIt.Index);
                if (const FString* PName = PtrToPropName.Find(PinIt.Input))
                {
                    if (!PName->Equals(Pin.Name))
                    {
                        Pin.PropertyName = *PName;
                    }
                }
                InputPins.Add(MoveTemp(Pin));
            }

            for (int32 i = 0; i < MutableCDO->Outputs.Num(); ++i)
            {
                FMcpPinInfo Pin;
                Pin.Index = i;
                Pin.Name  = MutableCDO->Outputs[i].OutputName.ToString();
                OutputPins.Add(MoveTemp(Pin));
            }
        }
    }

    AllClassNames.Sort();
}

UClass* FMcpMaterialExpressionCatalog::ResolveClassByExactName(const FString& Name) const
{
    UClass* const* Found = NameToClass.Find(Name);
    return Found ? *Found : nullptr;
}

UClass* FMcpMaterialExpressionCatalog::ResolveClassWithAutoPrefix(const FString& Name, bool& bOutAutoPrefixed) const
{
    bOutAutoPrefixed = false;
    if (UClass* Direct = ResolveClassByExactName(Name))
    {
        return Direct;
    }
    const FString Prefixed = FString(TEXT("MaterialExpression")) + Name;
    if (UClass* Pref = ResolveClassByExactName(Prefixed))
    {
        bOutAutoPrefixed = true;
        return Pref;
    }
    return nullptr;
}

TArray<FString> FMcpMaterialExpressionCatalog::SuggestNames(const FString& Query, int32 MaxCount) const
{
    TArray<FString> Result;
    if (MaxCount <= 0 || AllClassNames.Num() == 0)
    {
        return Result;
    }

    struct FScored
    {
        const FString* Name;
        int32 Distance;
        bool bPrefixMatch;
    };

    TArray<FScored> Scored;
    Scored.Reserve(AllClassNames.Num());
    for (const FString& Name : AllClassNames)
    {
        const bool bPrefix = Name.StartsWith(Query, ESearchCase::CaseSensitive);
        Scored.Add({ &Name, LevenshteinDistance(Query, Name), bPrefix });
    }

    // Prefix matches rank ahead of pure-distance matches, because "MaterialExpressionMul"
    // most likely intends "MaterialExpressionMultiply" rather than "MaterialExpressionMin".
    Scored.Sort([](const FScored& A, const FScored& B)
    {
        if (A.bPrefixMatch != B.bPrefixMatch) return A.bPrefixMatch && !B.bPrefixMatch;
        if (A.Distance != B.Distance) return A.Distance < B.Distance;
        return *A.Name < *B.Name;
    });

    const int32 Take = FMath::Min(MaxCount, Scored.Num());
    Result.Reserve(Take);
    for (int32 i = 0; i < Take; ++i)
    {
        Result.Add(*Scored[i].Name);
    }
    return Result;
}

const TArray<FString>* FMcpMaterialExpressionCatalog::GetApplicableFields(const FString& ClassName) const
{
    return NameToApplicableFields.Find(ClassName);
}

const TArray<FString>* FMcpMaterialExpressionCatalog::GetFieldEnumValues(const FString& ClassName, const FString& FieldName) const
{
    const TMap<FString, TArray<FString>>* PerClass = NameToFieldEnums.Find(ClassName);
    if (!PerClass)
    {
        return nullptr;
    }
    return PerClass->Find(FieldName);
}

FString FMcpMaterialExpressionCatalog::GetUePropertyName(const FString& ClassName, const FString& FieldName) const
{
    const TMap<FString, FString>* PerClass = NameToFieldUeProp.Find(ClassName);
    if (!PerClass)
    {
        return FString();
    }
    const FString* Found = PerClass->Find(FieldName);
    return Found ? *Found : FString();
}

const TArray<FString>* FMcpMaterialExpressionCatalog::GetCategories(const FString& ClassName) const
{
    return NameToCategories.Find(ClassName);
}

FString FMcpMaterialExpressionCatalog::GetDescription(const FString& ClassName) const
{
    const FString* Found = NameToDescription.Find(ClassName);
    return Found ? *Found : FString();
}

const TArray<FMcpPinInfo>* FMcpMaterialExpressionCatalog::GetInputPins(const FString& ClassName) const
{
    return NameToInputPins.Find(ClassName);
}

const TArray<FMcpPinInfo>* FMcpMaterialExpressionCatalog::GetOutputPins(const FString& ClassName) const
{
    return NameToOutputPins.Find(ClassName);
}
