// Tests for instance-based pin resolution on UMaterialExpressionMaterialFunctionCall.
//
// Prior write-side path validated toPin against static UPROPERTY fields only, so
// MFC FunctionInputs (a dynamic TArray) were invisible and any name-based connect
// or break would return VALIDATION_FAILED/INPUT_NOT_FOUND. After the fix the
// resolver walks FExpressionInputIterator (GetInput(i)) and accepts canonical,
// decorated, UPROPERTY-name, instance-name, and numeric-index handle forms.
//
// Uses /Engine/Functions/Engine_MaterialFunctions02/Utility/AppendMany as the
// MFC target - 4 scalar inputs named R / G / B / A.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialFunction.h"

extern FString McpGetUndecoratedInputName(UMaterialExpression* Expr, int32 InputIndex);

namespace McpInputPinResolutionForTests
{
    FExpressionInput* ResolveInputByPinName(
        UMaterialExpression* Expr,
        const FString& PinName,
        TArray<FString>& OutCanonicalHandles,
        int32& OutInputIndex);
}

namespace
{
    // Build a transient material with an MFC pointing at AppendMany.
    struct FMfcFixture
    {
        UMaterial*                                  Mat = nullptr;
        UMaterialExpressionMaterialFunctionCall*    MFC = nullptr;
        UMaterialFunction*                          Func = nullptr;

        bool Build(const TCHAR* Suffix)
        {
            const FString MatName = FString::Printf(TEXT("Fixture_MfcPinResolve_%s"), Suffix);
            Mat = NewObject<UMaterial>(GetTransientPackage(), *MatName, RF_Transient);
            if (!Mat) return false;

            Func = LoadObject<UMaterialFunction>(nullptr,
                TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/AppendMany.AppendMany"));
            if (!Func) return false;

            MFC = NewObject<UMaterialExpressionMaterialFunctionCall>(Mat);
            MFC->MaterialExpressionGuid = FGuid::NewGuid();
            MFC->SetMaterialFunction(Func);
            Mat->GetExpressionCollection().AddExpression(MFC);
            return MFC->FunctionInputs.Num() == 4; // AppendMany expects R/G/B/A
        }
    };
}

// ---- helper: McpGetUndecoratedInputName strips type suffix on MFC ---------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpMfcUndecoratedName_StripsTypeSuffix,
    "LHGame.Mcp.Material.MfcPinResolve.UndecoratedNameStripsType",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpMfcUndecoratedName_StripsTypeSuffix::RunTest(const FString&)
{
    FMfcFixture F;
    if (!F.Build(TEXT("UndecoratedName")))
    {
        AddWarning(TEXT("AppendMany asset unavailable in this environment; skipping"));
        return true;
    }

    // Decorated form is what UE shows in the editor.
    TestEqual(TEXT("decorated input 0 == 'R (S)'"),
        F.MFC->GetInputName(0).ToString(), FString(TEXT("R (S)")));

    // Canonical form is the round-trip handle we emit and accept on the write side.
    TestEqual(TEXT("canonical input 0 == 'R'"),
        McpGetUndecoratedInputName(F.MFC, 0), FString(TEXT("R")));
    TestEqual(TEXT("canonical input 1 == 'G'"),
        McpGetUndecoratedInputName(F.MFC, 1), FString(TEXT("G")));
    TestEqual(TEXT("canonical input 2 == 'B'"),
        McpGetUndecoratedInputName(F.MFC, 2), FString(TEXT("B")));
    TestEqual(TEXT("canonical input 3 == 'A'"),
        McpGetUndecoratedInputName(F.MFC, 3), FString(TEXT("A")));
    return true;
}

// ---- resolver accepts all four handle forms on MFC ------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpMfcResolve_AcceptsAllHandleForms,
    "LHGame.Mcp.Material.MfcPinResolve.AcceptsAllHandleForms",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpMfcResolve_AcceptsAllHandleForms::RunTest(const FString&)
{
    FMfcFixture F;
    if (!F.Build(TEXT("AllForms")))
    {
        AddWarning(TEXT("AppendMany asset unavailable; skipping"));
        return true;
    }

    auto CheckMatches = [&](const FString& PinName, int32 ExpectedIndex)
    {
        TArray<FString> Handles;
        int32 ResolvedIndex = INDEX_NONE;
        FExpressionInput* In = McpInputPinResolutionForTests::ResolveInputByPinName(
            F.MFC, PinName, Handles, ResolvedIndex);
        TestNotNull(*FString::Printf(TEXT("'%s' resolves"), *PinName), In);
        TestEqual(*FString::Printf(TEXT("'%s' index"), *PinName), ResolvedIndex, ExpectedIndex);
    };

    CheckMatches(TEXT("R"),       0); // canonical
    CheckMatches(TEXT("r"),       0); // case-insensitive canonical
    CheckMatches(TEXT("R (S)"),   0); // decorated (editor form)
    CheckMatches(TEXT("0"),       0); // numeric index

    CheckMatches(TEXT("B"),       2);
    CheckMatches(TEXT("B (S)"),   2);
    CheckMatches(TEXT("2"),       2);
    return true;
}

// ---- resolver miss surfaces canonical handles in diagnostics ------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpMfcResolve_GibberishMissYieldsCanonicalList,
    "LHGame.Mcp.Material.MfcPinResolve.GibberishMissYieldsCanonicalList",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpMfcResolve_GibberishMissYieldsCanonicalList::RunTest(const FString&)
{
    FMfcFixture F;
    if (!F.Build(TEXT("GibberishMiss")))
    {
        AddWarning(TEXT("AppendMany asset unavailable; skipping"));
        return true;
    }

    TArray<FString> Handles;
    int32 ResolvedIndex = INDEX_NONE;
    FExpressionInput* In = McpInputPinResolutionForTests::ResolveInputByPinName(
        F.MFC, TEXT("NotAValidPinName"), Handles, ResolvedIndex);

    TestNull(TEXT("miss returns nullptr"), In);
    TestEqual(TEXT("miss leaves index INDEX_NONE"), ResolvedIndex, INDEX_NONE);

    // Diagnostic list must contain the four canonical handles so error messages
    // can guide the caller toward a valid name.
    TestEqual(TEXT("4 canonical handles"), Handles.Num(), 4);
    TestTrue (TEXT("handles contains 'R'"), Handles.Contains(FString(TEXT("R"))));
    TestTrue (TEXT("handles contains 'G'"), Handles.Contains(FString(TEXT("G"))));
    TestTrue (TEXT("handles contains 'B'"), Handles.Contains(FString(TEXT("B"))));
    TestTrue (TEXT("handles contains 'A'"), Handles.Contains(FString(TEXT("A"))));
    return true;
}

// ---- regression: StaticSwitchParameter accepts UPROPERTY name + synthetic name + index ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpStaticSwitch_AllHandleForms,
    "LHGame.Mcp.Material.MfcPinResolve.StaticSwitchAllHandleForms",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpStaticSwitch_AllHandleForms::RunTest(const FString&)
{
    UMaterial* Mat = NewObject<UMaterial>(GetTransientPackage(),
        TEXT("Fixture_MfcPinResolve_StaticSwitch"), RF_Transient);
    UMaterialExpressionStaticSwitchParameter* SSP =
        NewObject<UMaterialExpressionStaticSwitchParameter>(Mat);
    SSP->MaterialExpressionGuid = FGuid::NewGuid();
    Mat->GetExpressionCollection().AddExpression(SSP);

    auto CheckMatches = [&](const FString& PinName, int32 ExpectedIndex)
    {
        TArray<FString> Handles;
        int32 ResolvedIndex = INDEX_NONE;
        FExpressionInput* In = McpInputPinResolutionForTests::ResolveInputByPinName(
            SSP, PinName, Handles, ResolvedIndex);
        TestNotNull(*FString::Printf(TEXT("'%s' resolves"), *PinName), In);
        TestEqual(*FString::Printf(TEXT("'%s' index"), *PinName), ResolvedIndex, ExpectedIndex);
    };

    CheckMatches(TEXT("A"),     0); // UPROPERTY
    CheckMatches(TEXT("True"),  0); // GetInputName synthetic
    CheckMatches(TEXT("true"),  0); // case-insensitive
    CheckMatches(TEXT("0"),     0); // numeric

    CheckMatches(TEXT("B"),     1);
    CheckMatches(TEXT("False"), 1);
    CheckMatches(TEXT("1"),     1);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
