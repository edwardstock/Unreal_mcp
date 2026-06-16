#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "../McpMaterialExpressionCatalog.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpMaterialExpressionCatalogTest,
    "LHGame.Mcp.Material.Catalog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpMaterialExpressionCatalogTest::RunTest(const FString& Parameters)
{
    const FMcpMaterialExpressionCatalog& Cat = FMcpMaterialExpressionCatalog::Get();

    // Catalog has many classes
    TestTrue(TEXT("catalog non-empty (>50 classes)"),
             Cat.GetAllClasses().Num() > 50);

    // Exact match
    UClass* Mul = Cat.ResolveClassByExactName(TEXT("MaterialExpressionMultiply"));
    TestNotNull(TEXT("Multiply by exact name"), Mul);

    // Auto-prefix
    bool bAuto = false;
    UClass* Mul2 = Cat.ResolveClassWithAutoPrefix(TEXT("Multiply"), bAuto);
    TestNotNull(TEXT("auto-prefix Multiply"), Mul2);
    TestTrue(TEXT("auto-prefix flag set"), bAuto);
    TestEqual(TEXT("auto-prefix gives same class"), Mul, Mul2);

    // Already-prefixed should not double-prefix
    bool bAuto2 = false;
    UClass* Mul3 = Cat.ResolveClassWithAutoPrefix(TEXT("MaterialExpressionMultiply"), bAuto2);
    TestNotNull(TEXT("exact-via-autoprefix works"), Mul3);
    TestFalse(TEXT("not flagged as auto-prefixed when already canonical"), bAuto2);

    // Unknown name -> nullptr
    bool bAuto3 = false;
    UClass* Bogus = Cat.ResolveClassWithAutoPrefix(TEXT("MaterialExpressionMul"), bAuto3);
    TestNull(TEXT("unknown name nullptr"), Bogus);

    // Did-you-mean
    TArray<FString> Sugg = Cat.SuggestNames(TEXT("MaterialExpressionMul"), 3);
    TestTrue(TEXT("at least one suggestion"), Sugg.Num() >= 1);
    TestTrue(TEXT("Multiply among suggestions"),
        Sugg.ContainsByPredicate([](const FString& S) { return S == TEXT("MaterialExpressionMultiply"); }));

    // Applicable fields for parameter sampler
    const TArray<FString>* Fields = Cat.GetApplicableFields(TEXT("MaterialExpressionTextureSampleParameter2D"));
    TestNotNull(TEXT("fields present"), Fields);
    if (Fields)
    {
        TestTrue(TEXT("includes parameterName"),
            Fields->ContainsByPredicate([](const FString& F) { return F == TEXT("parameterName"); }));
        TestTrue(TEXT("includes texturePath"),
            Fields->ContainsByPredicate([](const FString& F) { return F == TEXT("texturePath"); }));
        TestTrue(TEXT("includes samplerType"),
            Fields->ContainsByPredicate([](const FString& F) { return F == TEXT("samplerType"); }));
    }

    // Reflection-discovered class-specific enum field
    const TArray<FString>* WPFields = Cat.GetApplicableFields(TEXT("MaterialExpressionWorldPosition"));
    TestNotNull(TEXT("WP fields present"), WPFields);
    if (WPFields)
    {
        TestTrue(TEXT("WP includes worldPositionShaderOffset"),
            WPFields->ContainsByPredicate([](const FString& F) { return F == TEXT("worldPositionShaderOffset"); }));
    }
    const TArray<FString>* WPEnum = Cat.GetFieldEnumValues(
        TEXT("MaterialExpressionWorldPosition"), TEXT("worldPositionShaderOffset"));
    TestNotNull(TEXT("WP enum values present"), WPEnum);
    if (WPEnum)
    {
        TestTrue(TEXT("Default present"),
            WPEnum->ContainsByPredicate([](const FString& V) { return V == TEXT("Default"); }));
        TestTrue(TEXT("CameraRelative present"),
            WPEnum->ContainsByPredicate([](const FString& V) { return V == TEXT("CameraRelative"); }));
        TestTrue(TEXT("ExcludeAllShaderOffsets present"),
            WPEnum->ContainsByPredicate([](const FString& V) { return V == TEXT("ExcludeAllShaderOffsets"); }));
    }

    return true;
}

#endif
