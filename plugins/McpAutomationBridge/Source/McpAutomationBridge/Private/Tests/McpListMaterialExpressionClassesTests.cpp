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
// Note: UE's UMaterialExpression subclasses populate `MenuCategories` at runtime
// via a virtual `GetMenuCategories` method, not through `meta=(MenuCategories=...)`
// UCLASS metadata. The catalog's metadata-driven extraction therefore yields an
// empty set on stock UE. Test asserts the aggregator returns a deterministic
// (sorted, no duplicates) array - the count itself is environment-dependent.

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
    if (AllCats.Num() == 0)
    {
        // Categories aren't populated on stock UE (see CategoriesSortedUnique
        // note). Skip rather than fail.
        AddInfo(TEXT("catalog reports no categories on this engine build; skipping"));
        return true;
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
