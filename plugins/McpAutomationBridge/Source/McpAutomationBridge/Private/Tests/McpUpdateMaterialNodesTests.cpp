// Tests for the update_material_nodes (Task C.2) validators:
//   - identifier resolution (number index, GUID, numeric-string rejection,
//     unknown name with suggestions)
//   - applicable-field validation against the resolved class
//
// Pure-function validator tests - mirror McpAddMaterialNodesValidationTests.cpp.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionMultiply.h"

#include "../McpAutomationBridgeHelpers.h"

namespace McpUpdateMaterialNodesValidationForTests
{
    bool ResolveIdentifier(
        const FMcpMaterialGraphOwner& Owner,
        const TSharedPtr<FJsonValue>& IdentifierJson,
        UMaterialExpression*& OutExpression,
        FString& OutCode,
        FString& OutField,
        FString& OutMessage,
        TArray<FString>& OutDidYouMean);

    bool ValidateUpdateFields(
        const TSharedPtr<FJsonObject>& Item,
        UClass* ResolvedClass,
        FString& OutCode,
        FString& OutField,
        FString& OutMessage);

    void BuildRequestedFields(
        const TSharedPtr<FJsonObject>& Item,
        TArray<FString>& OutRequestedFields);
}

namespace
{
    // Build a transient UMaterialFunction with two named expressions.
    // A: ScalarParameter (named "Alpha"), B: Multiply.
    struct FUpdateFixture
    {
        UMaterialFunction* MF = nullptr;
        UMaterialExpressionScalarParameter* A = nullptr;
        UMaterialExpressionMultiply*        B = nullptr;
        FMcpMaterialGraphOwner Owner;

        void Build(const TCHAR* FixtureSuffix)
        {
            const FString MFName = FString::Printf(TEXT("Fixture_UpdateNodes_%s"), FixtureSuffix);
            MF = NewObject<UMaterialFunction>(GetTransientPackage(), *MFName, RF_Transient);
            A = NewObject<UMaterialExpressionScalarParameter>(MF);
            A->ParameterName = TEXT("Alpha");
            A->MaterialExpressionGuid = FGuid::NewGuid();
            MF->GetExpressionCollection().AddExpression(A);

            B = NewObject<UMaterialExpressionMultiply>(MF);
            B->MaterialExpressionGuid = FGuid::NewGuid();
            MF->GetExpressionCollection().AddExpression(B);

            Owner.Asset       = MF;
            Owner.GraphSource = MF;
            Owner.Kind        = EMcpMaterialGraphOwnerKind::MaterialFunction;
            Owner.bReadOnly   = false;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpUpdateMaterialNodes_NumberPicksByIndex,
    "LHGame.Mcp.Material.UpdateNodes.Identifier.NumberIndex",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpUpdateMaterialNodes_NumberPicksByIndex::RunTest(const FString&)
{
    FUpdateFixture F; F.Build(TEXT("NumIdx"));

    UMaterialExpression* Out = nullptr;
    FString Code, Field, Msg; TArray<FString> DYM;

    TSharedPtr<FJsonValue> Id = MakeShared<FJsonValueNumber>(0);
    const bool b = McpUpdateMaterialNodesValidationForTests::ResolveIdentifier(
        F.Owner, Id, Out, Code, Field, Msg, DYM);

    TestTrue (TEXT("number resolves"), b);
    TestEqual(TEXT("index 0 -> A"), Out, (UMaterialExpression*)F.A);

    Id = MakeShared<FJsonValueNumber>(1);
    DYM.Reset();
    const bool b2 = McpUpdateMaterialNodesValidationForTests::ResolveIdentifier(
        F.Owner, Id, Out, Code, Field, Msg, DYM);
    TestTrue (TEXT("index 1 resolves"), b2);
    TestEqual(TEXT("index 1 -> B"), Out, (UMaterialExpression*)F.B);

    // Out-of-range -> NODE_NOT_FOUND
    Id = MakeShared<FJsonValueNumber>(99);
    DYM.Reset();
    const bool b3 = McpUpdateMaterialNodesValidationForTests::ResolveIdentifier(
        F.Owner, Id, Out, Code, Field, Msg, DYM);
    TestFalse(TEXT("out-of-range fails"), b3);
    TestEqual(TEXT("code NODE_NOT_FOUND"), Code, FString(TEXT("NODE_NOT_FOUND")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpUpdateMaterialNodes_GuidResolves,
    "LHGame.Mcp.Material.UpdateNodes.Identifier.Guid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpUpdateMaterialNodes_GuidResolves::RunTest(const FString&)
{
    FUpdateFixture F; F.Build(TEXT("Guid"));

    UMaterialExpression* Out = nullptr;
    FString Code, Field, Msg; TArray<FString> DYM;

    TSharedPtr<FJsonValue> Id = MakeShared<FJsonValueString>(F.B->MaterialExpressionGuid.ToString());
    const bool b = McpUpdateMaterialNodesValidationForTests::ResolveIdentifier(
        F.Owner, Id, Out, Code, Field, Msg, DYM);

    TestTrue (TEXT("guid resolves"), b);
    TestEqual(TEXT("guid -> B"), Out, (UMaterialExpression*)F.B);

    // unknown guid -> NODE_NOT_FOUND
    Id = MakeShared<FJsonValueString>(FGuid::NewGuid().ToString());
    DYM.Reset();
    const bool b2 = McpUpdateMaterialNodesValidationForTests::ResolveIdentifier(
        F.Owner, Id, Out, Code, Field, Msg, DYM);
    TestFalse(TEXT("unknown guid fails"), b2);
    TestEqual(TEXT("code NODE_NOT_FOUND"), Code, FString(TEXT("NODE_NOT_FOUND")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpUpdateMaterialNodes_NumericStringRejected,
    "LHGame.Mcp.Material.UpdateNodes.Identifier.NumericStringRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpUpdateMaterialNodes_NumericStringRejected::RunTest(const FString&)
{
    FUpdateFixture F; F.Build(TEXT("NumStr"));

    UMaterialExpression* Out = nullptr;
    FString Code, Field, Msg; TArray<FString> DYM;

    TSharedPtr<FJsonValue> Id = MakeShared<FJsonValueString>(TEXT("3"));
    const bool b = McpUpdateMaterialNodesValidationForTests::ResolveIdentifier(
        F.Owner, Id, Out, Code, Field, Msg, DYM);

    TestFalse(TEXT("numeric-string rejected"), b);
    TestEqual(TEXT("code INVALID_IDENTIFIER_TYPE"), Code, FString(TEXT("INVALID_IDENTIFIER_TYPE")));
    TestEqual(TEXT("field identifier"), Field, FString(TEXT("identifier")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpUpdateMaterialNodes_UnknownName,
    "LHGame.Mcp.Material.UpdateNodes.Identifier.UnknownName",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpUpdateMaterialNodes_UnknownName::RunTest(const FString&)
{
    FUpdateFixture F; F.Build(TEXT("Unknown"));

    UMaterialExpression* Out = nullptr;
    FString Code, Field, Msg; TArray<FString> DYM;

    // A name guaranteed not to match any expression
    TSharedPtr<FJsonValue> Id = MakeShared<FJsonValueString>(TEXT("ZZZ_NoSuchExpression"));
    const bool b = McpUpdateMaterialNodesValidationForTests::ResolveIdentifier(
        F.Owner, Id, Out, Code, Field, Msg, DYM);

    TestFalse(TEXT("unknown name fails"), b);
    TestEqual(TEXT("code NODE_NOT_FOUND"), Code, FString(TEXT("NODE_NOT_FOUND")));
    TestTrue (TEXT("didYouMean populated"), DYM.Num() > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpUpdateMaterialNodes_FieldNotApplicable,
    "LHGame.Mcp.Material.UpdateNodes.Validate.FieldNotApplicable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpUpdateMaterialNodes_FieldNotApplicable::RunTest(const FString&)
{
    // Multiply does not accept defaultValue; same payload that would fail in C.1.
    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    Item->SetNumberField(TEXT("defaultValue"), 1.0);

    FString Code, Field, Msg;
    const bool b = McpUpdateMaterialNodesValidationForTests::ValidateUpdateFields(
        Item, UMaterialExpressionMultiply::StaticClass(), Code, Field, Msg);

    TestFalse(TEXT("non-applicable rejected"), b);
    TestEqual(TEXT("code FIELD_NOT_APPLICABLE"), Code, FString(TEXT("FIELD_NOT_APPLICABLE")));
    TestEqual(TEXT("field defaultValue"), Field, FString(TEXT("defaultValue")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpUpdateMaterialNodes_FieldApplicable,
    "LHGame.Mcp.Material.UpdateNodes.Validate.FieldApplicable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpUpdateMaterialNodes_FieldApplicable::RunTest(const FString&)
{
    // ScalarParameter accepts defaultValue + parameterName; identifier is ignored.
    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    Item->SetStringField(TEXT("identifier"), TEXT("Alpha"));
    Item->SetNumberField(TEXT("defaultValue"), 0.5);
    Item->SetStringField(TEXT("parameterName"), TEXT("Alpha2"));

    FString Code, Field, Msg;
    const bool b = McpUpdateMaterialNodesValidationForTests::ValidateUpdateFields(
        Item, UMaterialExpressionScalarParameter::StaticClass(), Code, Field, Msg);

    TestTrue(TEXT("applicable fields accepted"), b);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpUpdateMaterialNodes_LocalIdSilentlySkipped,
    "LHGame.Mcp.Material.UpdateNodes.LocalIdSilentlySkipped",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpUpdateMaterialNodes_LocalIdSilentlySkipped::RunTest(const FString&)
{
    // Caller passes a localId in the update payload alongside a real field.
    // localId is an add-only meta key; on update it must be silently skipped:
    //   - validation passes
    //   - RequestedFields excludes "localId"
    //   - RequestedFields includes "defaultValue"
    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    Item->SetNumberField(TEXT("identifier"),   0);
    Item->SetStringField(TEXT("localId"),      TEXT("foo"));
    Item->SetNumberField(TEXT("defaultValue"), 0.5);

    FString Code, Field, Msg;
    const bool bValid = McpUpdateMaterialNodesValidationForTests::ValidateUpdateFields(
        Item, UMaterialExpressionScalarParameter::StaticClass(), Code, Field, Msg);
    TestTrue(TEXT("validation passes with localId present"), bValid);

    TArray<FString> Requested;
    McpUpdateMaterialNodesValidationForTests::BuildRequestedFields(Item, Requested);

    TestFalse(TEXT("localId excluded from RequestedFields"),
        Requested.Contains(FString(TEXT("localId"))));
    TestTrue (TEXT("defaultValue present in RequestedFields"),
        Requested.Contains(FString(TEXT("defaultValue"))));
    TestFalse(TEXT("identifier excluded from RequestedFields"),
        Requested.Contains(FString(TEXT("identifier"))));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpUpdateMaterialNodes_NodeTypeSilentlySkipped,
    "LHGame.Mcp.Material.UpdateNodes.NodeTypeSilentlySkipped",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpUpdateMaterialNodes_NodeTypeSilentlySkipped::RunTest(const FString&)
{
    // nodeType, like localId, is an add-only concept and must be silently
    // skipped on update.
    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    Item->SetNumberField(TEXT("identifier"),   0);
    Item->SetStringField(TEXT("nodeType"),     TEXT("ScalarParameter"));
    Item->SetNumberField(TEXT("defaultValue"), 0.25);

    FString Code, Field, Msg;
    const bool bValid = McpUpdateMaterialNodesValidationForTests::ValidateUpdateFields(
        Item, UMaterialExpressionScalarParameter::StaticClass(), Code, Field, Msg);
    TestTrue(TEXT("validation passes with nodeType present"), bValid);

    TArray<FString> Requested;
    McpUpdateMaterialNodesValidationForTests::BuildRequestedFields(Item, Requested);

    TestFalse(TEXT("nodeType excluded from RequestedFields"),
        Requested.Contains(FString(TEXT("nodeType"))));
    TestTrue (TEXT("defaultValue present in RequestedFields"),
        Requested.Contains(FString(TEXT("defaultValue"))));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
