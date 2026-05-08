// Tests for break_material_connections (Task C.4) root-pin resolution.
//
// The reflective McpFindExpressionInputProperty path cannot find root pins on
// UMaterialEditorOnlyData because their concrete types (FColorMaterialInput,
// FScalarMaterialInput, etc.) derive from FMaterialInput<T> which is declared
// noexport in UE 5.7's reflection - the FExpressionInput ancestor is not
// reachable via UStruct::GetSuperStruct walks. Fix routes root-pin lookup
// through UMaterial::GetExpressionInputForProperty.
//
// These tests cover the new logic only (per plan ground rule "tests only for
// new logic"); the expression-to-expression reflective path is unchanged.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "SceneTypes.h"
#include "../McpAutomationBridgeHelpers.h"

namespace McpBreakMaterialConnectionsForTests
{
    bool ResolveRootInput(
        UMaterial* Material,
        const FString& ToPin,
        FExpressionInput*& OutInput,
        EMaterialProperty& OutProperty,
        FString& OutCode,
        FString& OutMessage);
}

namespace
{
    // Build a transient UMaterial with a single expression wired into BaseColor.
    struct FBreakFixture
    {
        UMaterial* Mat = nullptr;
        UMaterialExpressionConstant3Vector* ExprColor = nullptr;
        UMaterialExpressionConstant*        ExprScalar = nullptr;

        void Build(const TCHAR* Suffix)
        {
            const FString MatName = FString::Printf(TEXT("Fixture_BreakConnections_%s"), Suffix);
            Mat = NewObject<UMaterial>(GetTransientPackage(), *MatName, RF_Transient);

            ExprColor = NewObject<UMaterialExpressionConstant3Vector>(Mat);
            ExprColor->MaterialExpressionGuid = FGuid::NewGuid();
            ExprColor->Constant = FLinearColor(1.f, 0.5f, 0.25f, 1.f);

            ExprScalar = NewObject<UMaterialExpressionConstant>(Mat);
            ExprScalar->MaterialExpressionGuid = FGuid::NewGuid();
            ExprScalar->R = 0.7f;

            Mat->GetExpressionCollection().AddExpression(ExprColor);
            Mat->GetExpressionCollection().AddExpression(ExprScalar);

            // Wire ExprColor -> BaseColor via the same path the production
            // code uses (UMaterialEditorOnlyData on UE 5.1+).
#if MCP_HAS_MATERIAL_EDITOR_ONLY_DATA
            UMaterialEditorOnlyData* ED = Mat->GetEditorOnlyData();
            check(ED);
            ED->BaseColor.Expression = ExprColor;
            ED->BaseColor.OutputIndex = 0;
            ED->Metallic.Expression = ExprScalar;
            ED->Metallic.OutputIndex = 0;
#else
            Mat->BaseColor.Expression = ExprColor;
            Mat->BaseColor.OutputIndex = 0;
            Mat->Metallic.Expression = ExprScalar;
            Mat->Metallic.OutputIndex = 0;
#endif
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpBreakConnections_RootPinBaseColorResolves,
    "LHGame.Mcp.Material.BreakConnections.RootPin.BaseColorResolves",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpBreakConnections_RootPinBaseColorResolves::RunTest(const FString&)
{
    FBreakFixture F; F.Build(TEXT("BaseColor"));

    FExpressionInput* In = nullptr;
    EMaterialProperty Prop = MP_MAX;
    FString Code, Msg;

    const bool b = McpBreakMaterialConnectionsForTests::ResolveRootInput(
        F.Mat, TEXT("BaseColor"), In, Prop, Code, Msg);

    TestTrue (TEXT("BaseColor resolves to a non-null FExpressionInput"), b);
    TestNotNull(TEXT("OutInput non-null"), In);
    TestEqual(TEXT("OutProperty == MP_BaseColor"), (int32)Prop, (int32)MP_BaseColor);
    if (In)
    {
        TestEqual(TEXT("BaseColor.Expression == ExprColor"),
            (UMaterialExpression*)In->Expression, (UMaterialExpression*)F.ExprColor);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpBreakConnections_RootPinMetallicResolves,
    "LHGame.Mcp.Material.BreakConnections.RootPin.MetallicResolves",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpBreakConnections_RootPinMetallicResolves::RunTest(const FString&)
{
    FBreakFixture F; F.Build(TEXT("Metallic"));

    FExpressionInput* In = nullptr;
    EMaterialProperty Prop = MP_MAX;
    FString Code, Msg;

    const bool b = McpBreakMaterialConnectionsForTests::ResolveRootInput(
        F.Mat, TEXT("Metallic"), In, Prop, Code, Msg);

    TestTrue (TEXT("Metallic resolves"), b);
    TestNotNull(TEXT("OutInput non-null"), In);
    TestEqual(TEXT("OutProperty == MP_Metallic"), (int32)Prop, (int32)MP_Metallic);
    if (In)
    {
        TestEqual(TEXT("Metallic.Expression == ExprScalar"),
            (UMaterialExpression*)In->Expression, (UMaterialExpression*)F.ExprScalar);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpBreakConnections_RootPinUnknownReturnsInputNotFound,
    "LHGame.Mcp.Material.BreakConnections.RootPin.UnknownReturnsInputNotFound",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpBreakConnections_RootPinUnknownReturnsInputNotFound::RunTest(const FString&)
{
    FBreakFixture F; F.Build(TEXT("Unknown"));

    FExpressionInput* In = nullptr;
    EMaterialProperty Prop = MP_MAX;
    FString Code, Msg;

    const bool b = McpBreakMaterialConnectionsForTests::ResolveRootInput(
        F.Mat, TEXT("NotARealRootPin"), In, Prop, Code, Msg);

    TestFalse(TEXT("unknown root pin fails"), b);
    TestEqual(TEXT("code INPUT_NOT_FOUND"), Code, FString(TEXT("INPUT_NOT_FOUND")));
    TestNull (TEXT("OutInput stays null"), In);
    TestEqual(TEXT("OutProperty stays MP_MAX"), (int32)Prop, (int32)MP_MAX);
    // Message must list the canonical valid root pins so users can self-correct.
    TestTrue (TEXT("message mentions BaseColor as a valid pin"), Msg.Contains(TEXT("BaseColor")));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
