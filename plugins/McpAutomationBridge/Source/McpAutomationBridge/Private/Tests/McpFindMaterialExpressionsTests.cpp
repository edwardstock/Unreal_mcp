// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Tests/McpFindMaterialExpressionsTests.cpp
// Tests for find_material_expressions (Task E.1).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "MaterialEditingLibrary.h"

#include "../McpAutomationBridgeHelpers.h"

namespace McpFindMaterialExpressionsForTests
{
    TSet<UMaterialExpression*> ComputeOrphanSet(const FMcpMaterialGraphOwner& Owner);
    bool LooksLikeGuid(const FString& S);
    UMaterialExpression* ResolveIdentifier(
        const FMcpMaterialGraphOwner& Owner,
        const TSharedPtr<FJsonValue>& Id,
        FString& OutKind,
        FString& OutMessage);
    bool ExpressionMatchesFilters(
        const FMcpMaterialGraphOwner& Owner,
        UMaterialExpression* Expr,
        int32 Index,
        const TSharedPtr<FJsonObject>& Payload,
        const TSet<UMaterialExpression*>* OrphanSet);
}


// ---- LooksLikeGuid ---------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpFindExpressionsLooksLikeGuid,
    "LHGame.Mcp.Material.FindExpressions.LooksLikeGuid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpFindExpressionsLooksLikeGuid::RunTest(const FString&)
{
    TestTrue(TEXT("braced guid"),
        McpFindMaterialExpressionsForTests::LooksLikeGuid(
            TEXT("{12345678-1234-1234-1234-1234567890AB}")));
    TestTrue(TEXT("hex32"),
        McpFindMaterialExpressionsForTests::LooksLikeGuid(
            TEXT("12345678123412341234123456789ABC")));
    TestTrue(TEXT("hyphenated"),
        McpFindMaterialExpressionsForTests::LooksLikeGuid(
            TEXT("12345678-1234-1234-1234-123456789ABC")));
    TestFalse(TEXT("plain name"),
        McpFindMaterialExpressionsForTests::LooksLikeGuid(TEXT("MaterialExpressionMultiply_3")));
    TestFalse(TEXT("numeric string"),
        McpFindMaterialExpressionsForTests::LooksLikeGuid(TEXT("3")));
    TestFalse(TEXT("empty"),
        McpFindMaterialExpressionsForTests::LooksLikeGuid(TEXT("")));
    return true;
}

// ---- ResolveIdentifier - notFound for numeric-string ----------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpFindExpressionsResolveNumericString,
    "LHGame.Mcp.Material.FindExpressions.ResolveIdentifierNumericString",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpFindExpressionsResolveNumericString::RunTest(const FString&)
{
    UMaterial* Mat = NewObject<UMaterial>(GetTransientPackage(),
        TEXT("Fixture_Mat_NumStr"), RF_Transient);
    FMcpMaterialGraphOwner Owner;
    Owner.Asset = Mat;
    Owner.GraphSource = Mat;
    Owner.Kind = EMcpMaterialGraphOwnerKind::Material;

    FString Kind, Msg;
    UMaterialExpression* R = McpFindMaterialExpressionsForTests::ResolveIdentifier(
        Owner, MakeShared<FJsonValueString>(TEXT("3")), Kind, Msg);

    TestNull(TEXT("not resolved"), R);
    TestEqual(TEXT("kind invalid_type"), Kind, FString(TEXT("invalid_type")));
    TestTrue(TEXT("message mentions ambiguous"), Msg.Contains(TEXT("ambiguous")));
    return true;
}

// ---- ResolveIdentifier - index out of range -------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpFindExpressionsResolveIndexOutOfRange,
    "LHGame.Mcp.Material.FindExpressions.ResolveIdentifierIndexOutOfRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpFindExpressionsResolveIndexOutOfRange::RunTest(const FString&)
{
    UMaterial* Mat = NewObject<UMaterial>(GetTransientPackage(),
        TEXT("Fixture_Mat_IdxOOR"), RF_Transient);
    FMcpMaterialGraphOwner Owner;
    Owner.Asset = Mat;
    Owner.GraphSource = Mat;
    Owner.Kind = EMcpMaterialGraphOwnerKind::Material;

    FString Kind, Msg;
    UMaterialExpression* R = McpFindMaterialExpressionsForTests::ResolveIdentifier(
        Owner, MakeShared<FJsonValueNumber>(99), Kind, Msg);

    TestNull(TEXT("not resolved"), R);
    TestEqual(TEXT("kind index"), Kind, FString(TEXT("index")));
    TestTrue(TEXT("message mentions out of range"), Msg.Contains(TEXT("out of range")));
    return true;
}

// ---- ResolveIdentifier - guid miss ----------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpFindExpressionsResolveUnknownGuid,
    "LHGame.Mcp.Material.FindExpressions.ResolveIdentifierUnknownGuid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpFindExpressionsResolveUnknownGuid::RunTest(const FString&)
{
    UMaterial* Mat = NewObject<UMaterial>(GetTransientPackage(),
        TEXT("Fixture_Mat_GuidMiss"), RF_Transient);
    FMcpMaterialGraphOwner Owner;
    Owner.Asset = Mat;
    Owner.GraphSource = Mat;
    Owner.Kind = EMcpMaterialGraphOwnerKind::Material;

    FString Kind, Msg;
    UMaterialExpression* R = McpFindMaterialExpressionsForTests::ResolveIdentifier(
        Owner, MakeShared<FJsonValueString>(TEXT("00000000-0000-0000-0000-000000000000")),
        Kind, Msg);

    TestNull(TEXT("not resolved"), R);
    TestEqual(TEXT("kind guid"), Kind, FString(TEXT("guid")));
    return true;
}

// ---- Filters: parameterName / parameterGroup ------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpFindExpressionsParameterGroupFilter,
    "LHGame.Mcp.Material.FindExpressions.ParameterGroupFilter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpFindExpressionsParameterGroupFilter::RunTest(const FString&)
{
    UMaterial* Mat = NewObject<UMaterial>(GetTransientPackage(),
        TEXT("Fixture_Mat_ParamGroup"), RF_Transient);
    UMaterialExpressionScalarParameter* P =
        NewObject<UMaterialExpressionScalarParameter>(Mat);
    P->ParameterName = TEXT("Brightness");
    P->Group = TEXT("Lighting");

    FMcpMaterialGraphOwner Owner;
    Owner.Asset = Mat;
    Owner.GraphSource = Mat;
    Owner.Kind = EMcpMaterialGraphOwnerKind::Material;

    TSharedPtr<FJsonObject> Filter = MakeShared<FJsonObject>();
    Filter->SetStringField(TEXT("parameterGroup"), TEXT("Lighting"));
    TestTrue(TEXT("group match accepts"),
        McpFindMaterialExpressionsForTests::ExpressionMatchesFilters(Owner, P, 0, Filter, nullptr));

    Filter->SetStringField(TEXT("parameterGroup"), TEXT("Geometry"));
    TestFalse(TEXT("group mismatch rejects"),
        McpFindMaterialExpressionsForTests::ExpressionMatchesFilters(Owner, P, 0, Filter, nullptr));

    // empty group on a non-parameter expression -> rejects with non-empty filter
    UMaterialExpressionMultiply* M = NewObject<UMaterialExpressionMultiply>(Mat);
    Filter->SetStringField(TEXT("parameterGroup"), TEXT("Anything"));
    TestFalse(TEXT("non-parameter rejected by group filter"),
        McpFindMaterialExpressionsForTests::ExpressionMatchesFilters(Owner, M, 0, Filter, nullptr));
    return true;
}

// ---- Filters: samplerType -------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpFindExpressionsSamplerTypeFilter,
    "LHGame.Mcp.Material.FindExpressions.SamplerTypeFilter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpFindExpressionsSamplerTypeFilter::RunTest(const FString&)
{
    UMaterial* Mat = NewObject<UMaterial>(GetTransientPackage(),
        TEXT("Fixture_Mat_SamplerType"), RF_Transient);
    UMaterialExpressionTextureSample* TS =
        NewObject<UMaterialExpressionTextureSample>(Mat);
    TS->SamplerType = SAMPLERTYPE_LinearColor;

    FMcpMaterialGraphOwner Owner;
    Owner.Asset = Mat;
    Owner.GraphSource = Mat;
    Owner.Kind = EMcpMaterialGraphOwnerKind::Material;

    TSharedPtr<FJsonObject> Filter = MakeShared<FJsonObject>();
    Filter->SetStringField(TEXT("samplerType"), TEXT("LinearColor"));
    TestTrue(TEXT("samplerType LinearColor matches"),
        McpFindMaterialExpressionsForTests::ExpressionMatchesFilters(Owner, TS, 0, Filter, nullptr));

    Filter->SetStringField(TEXT("samplerType"), TEXT("Normal"));
    TestFalse(TEXT("samplerType Normal does not match"),
        McpFindMaterialExpressionsForTests::ExpressionMatchesFilters(Owner, TS, 0, Filter, nullptr));

    // Non-texture expression rejects samplerType filter.
    UMaterialExpressionMultiply* M = NewObject<UMaterialExpressionMultiply>(Mat);
    Filter->SetStringField(TEXT("samplerType"), TEXT("LinearColor"));
    TestFalse(TEXT("non-texture rejected by samplerType filter"),
        McpFindMaterialExpressionsForTests::ExpressionMatchesFilters(Owner, M, 0, Filter, nullptr));
    return true;
}

// ---- Filters: referencesTexture (path mismatch path) ----------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpFindExpressionsReferencesTextureNullCase,
    "LHGame.Mcp.Material.FindExpressions.ReferencesTextureNullCase",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpFindExpressionsReferencesTextureNullCase::RunTest(const FString&)
{
    UMaterial* Mat = NewObject<UMaterial>(GetTransientPackage(),
        TEXT("Fixture_Mat_RefTex"), RF_Transient);
    UMaterialExpressionTextureSample* TS =
        NewObject<UMaterialExpressionTextureSample>(Mat);
    TS->Texture = nullptr;

    FMcpMaterialGraphOwner Owner;
    Owner.Asset = Mat;
    Owner.GraphSource = Mat;
    Owner.Kind = EMcpMaterialGraphOwnerKind::Material;

    TSharedPtr<FJsonObject> Filter = MakeShared<FJsonObject>();
    Filter->SetStringField(TEXT("referencesTexture"), TEXT("/Game/Some/T_Foo"));
    TestFalse(TEXT("unbound texture rejects path filter"),
        McpFindMaterialExpressionsForTests::ExpressionMatchesFilters(Owner, TS, 0, Filter, nullptr));

    // Non-texture-base node also rejects a non-empty referencesTexture filter.
    UMaterialExpressionMultiply* M = NewObject<UMaterialExpressionMultiply>(Mat);
    TestFalse(TEXT("non-texture rejected by referencesTexture filter"),
        McpFindMaterialExpressionsForTests::ExpressionMatchesFilters(Owner, M, 0, Filter, nullptr));
    return true;
}

// ---- Orphan BFS (3-node hand-built fixture) -------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpFindExpressionsOrphanBfs,
    "LHGame.Mcp.Material.FindExpressions.OrphanBfs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpFindExpressionsOrphanBfs::RunTest(const FString&)
{
    UMaterial* Mat = NewObject<UMaterial>(GetTransientPackage(),
        TEXT("Fixture_Mat_OrphanBfs"), RF_Transient);

    // A wired to root BaseColor; B wired into A's input; C unwired.
    UMaterialExpressionMultiply* A = Cast<UMaterialExpressionMultiply>(
        UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass()));
    UMaterialExpressionAdd* B = Cast<UMaterialExpressionAdd>(
        UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionAdd::StaticClass()));
    UMaterialExpressionConstant* C = Cast<UMaterialExpressionConstant>(
        UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionConstant::StaticClass()));

    if (!A || !B || !C)
    {
        AddInfo(TEXT("expression creation returned null; skipping"));
        return true;
    }

    // Wire B -> A's input, A -> root BaseColor.
    A->A.Expression = B;
    A->A.OutputIndex = 0;

#if MCP_HAS_MATERIAL_EDITOR_ONLY_DATA
    if (UObject* EditorData = Mat->GetEditorOnlyData())
    {
        FExpressionInput* Root = Mat->GetExpressionInputForProperty(MP_BaseColor);
        if (Root)
        {
            Root->Expression = A;
            Root->OutputIndex = 0;
        }
    }
#else
    FExpressionInput* Root = Mat->GetExpressionInputForProperty(MP_BaseColor);
    if (Root)
    {
        Root->Expression = A;
        Root->OutputIndex = 0;
    }
#endif

    FMcpMaterialGraphOwner Owner;
    Owner.Asset = Mat;
    Owner.GraphSource = Mat;
    Owner.Kind = EMcpMaterialGraphOwnerKind::Material;

    TSet<UMaterialExpression*> Orphans =
        McpFindMaterialExpressionsForTests::ComputeOrphanSet(Owner);

    TestFalse(TEXT("A is reachable"), Orphans.Contains(A));
    TestFalse(TEXT("B is reachable via A"), Orphans.Contains(B));
    TestTrue (TEXT("C is orphan"),         Orphans.Contains(C));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
