// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Tests/McpListMaterialExpressionClassesTests.cpp
// Tests for list_material_expression_classes (Task E.2).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "../McpMaterialExpressionCatalog.h"

namespace McpListMaterialExpressionClassesForTests
{
    TArray<FString> CollectAllCategories();
    int32 MaterialRootPinCount();
}

// ---- catalog reuse: aggregate categories accessor works -------------------
// Categories are read from the CDO's UPROPERTY(config) TArray<FText> MenuCategories,
// populated from Engine/Config/BaseMaterialExpressions.ini. Stock UE ships a non-empty
// set including at least Parameters / Math / Texture / Utility.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpListClassesCategoriesSortedUnique,
    "LHGame.Mcp.Material.ListClasses.CategoriesSortedUnique",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpListClassesCategoriesSortedUnique::RunTest(const FString&)
{
    const TArray<FString> Cats = McpListMaterialExpressionClassesForTests::CollectAllCategories();
    // Sorted ascending and unique.
    for (int32 i = 1; i < Cats.Num(); ++i)
    {
        TestTrue(TEXT("strictly ascending (unique + sorted)"), Cats[i - 1] < Cats[i]);
    }
    // No empty entries.
    for (const FString& C : Cats)
    {
        TestFalse(TEXT("no empty category"), C.IsEmpty());
    }
    // Stock UE always registers these via BaseMaterialExpressions.ini.
    TestTrue(TEXT("contains Parameters"), Cats.Contains(TEXT("Parameters")));
    TestTrue(TEXT("contains Math"), Cats.Contains(TEXT("Math")));
    TestTrue(TEXT("contains Texture"), Cats.Contains(TEXT("Texture")));
    return true;
}

// ---- materialRootPins[] count matches spec --------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpListClassesRootPinCount,
    "LHGame.Mcp.Material.ListClasses.RootPinCount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpListClassesRootPinCount::RunTest(const FString&)
{
    // Spec sec 8a lists exactly 13 root pins.
    TestEqual(TEXT("root pin count == 13"),
        McpListMaterialExpressionClassesForTests::MaterialRootPinCount(), 13);
    return true;
}

// ---- catalog filter: Contains is case-insensitive on class name ----------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpListClassesFilterReducesSet,
    "LHGame.Mcp.Material.ListClasses.FilterReducesSet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpListClassesFilterReducesSet::RunTest(const FString&)
{
    const FMcpMaterialExpressionCatalog& Cat = FMcpMaterialExpressionCatalog::Get();
    const TArray<FString>& All = Cat.GetAllClasses();
    const int32 Total = All.Num();
    TestTrue(TEXT("catalog non-empty"), Total > 0);

    int32 TextureMatches = 0;
    for (const FString& Name : All)
    {
        if (Name.Contains(TEXT("Texture"), ESearchCase::IgnoreCase))
        {
            ++TextureMatches;
        }
    }
    TestTrue(TEXT("Texture filter reduces set"), TextureMatches > 0 && TextureMatches < Total);

    // Lowercase should match equally (case-insensitive).
    int32 LowerMatches = 0;
    for (const FString& Name : All)
    {
        if (Name.Contains(TEXT("texture"), ESearchCase::IgnoreCase))
        {
            ++LowerMatches;
        }
    }
    TestEqual(TEXT("case-insensitive Contains"), LowerMatches, TextureMatches);
    return true;
}

// ---- category filter reduces the result set ------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpListClassesCategoryReducesSet,
    "LHGame.Mcp.Material.ListClasses.CategoryReducesSet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpListClassesCategoryReducesSet::RunTest(const FString&)
{
    const FMcpMaterialExpressionCatalog& Cat = FMcpMaterialExpressionCatalog::Get();
    const TArray<FString>& All = Cat.GetAllClasses();

    const TArray<FString> AllCats = McpListMaterialExpressionClassesForTests::CollectAllCategories();
    TestTrue(TEXT("catalog has at least one category"), AllCats.Num() > 0);
    if (AllCats.Num() == 0)
    {
        return false;
    }

    const FString& Pick = AllCats[0];
    int32 InCategory = 0;
    for (const FString& Name : All)
    {
        const TArray<FString>* Cs = Cat.GetCategories(Name);
        if (!Cs) continue;
        for (const FString& C : *Cs)
        {
            if (C.Equals(Pick, ESearchCase::IgnoreCase)) { ++InCategory; break; }
        }
    }
    TestTrue(TEXT("at least one class in picked category"), InCategory > 0);
    TestTrue(TEXT("category reduces set"), InCategory < All.Num());
    return true;
}

// ---- StaticSwitchParameter: categories + applicable fields populated -----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpListClassesStaticSwitchParameterEntry,
    "LHGame.Mcp.Material.ListClasses.StaticSwitchParameterEntry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpListClassesStaticSwitchParameterEntry::RunTest(const FString&)
{
    const FMcpMaterialExpressionCatalog& Cat = FMcpMaterialExpressionCatalog::Get();
    const FString ClassName = TEXT("MaterialExpressionStaticSwitchParameter");

    const TArray<FString>* Cats = Cat.GetCategories(ClassName);
    TestNotNull(TEXT("class registered in catalog"), Cats);
    if (Cats)
    {
        TestTrue(TEXT("StaticSwitchParameter category is 'Parameters'"),
            Cats->Contains(TEXT("Parameters")));
    }

    const TArray<FString>* Fields = Cat.GetApplicableFields(ClassName);
    TestNotNull(TEXT("applicableFields registered"), Fields);
    if (Fields)
    {
        TestTrue(TEXT("has parameterName"), Fields->Contains(TEXT("parameterName")));
        TestTrue(TEXT("has defaultValue"), Fields->Contains(TEXT("defaultValue")));
    }

    // The whole point of pin discovery: StaticSwitchParameter's UE-level pins are A/B,
    // but GetInputName overrides them to True/False. Both must be exposed so an agent
    // can pick the canonical name.
    const TArray<FMcpPinInfo>* InPins = Cat.GetInputPins(ClassName);
    TestNotNull(TEXT("inputPins registered"), InPins);
    if (InPins)
    {
        TestEqual(TEXT("two inputs"), InPins->Num(), 2);
        if (InPins->Num() >= 2)
        {
            TestEqual(TEXT("pin 0 name"), (*InPins)[0].Name, FString(TEXT("True")));
            TestEqual(TEXT("pin 1 name"), (*InPins)[1].Name, FString(TEXT("False")));
            TestEqual(TEXT("pin 0 propertyName"), (*InPins)[0].PropertyName, FString(TEXT("A")));
            TestEqual(TEXT("pin 1 propertyName"), (*InPins)[1].PropertyName, FString(TEXT("B")));
        }
    }
    return true;
}

// ---- INVALID_CATEGORY: unknown category not in registered set ------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpListClassesInvalidCategoryUnknown,
    "LHGame.Mcp.Material.ListClasses.InvalidCategoryUnknown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpListClassesInvalidCategoryUnknown::RunTest(const FString&)
{
    const TArray<FString> Cats = McpListMaterialExpressionClassesForTests::CollectAllCategories();
    const FString Bogus(TEXT("ThisCategoryShouldNeverExistMcpE2"));
    bool bMatched = false;
    for (const FString& C : Cats)
    {
        if (C.Equals(Bogus, ESearchCase::IgnoreCase)) { bMatched = true; break; }
    }
    TestFalse(TEXT("bogus category not in registered set"), bMatched);
    return true;
}

#endif
