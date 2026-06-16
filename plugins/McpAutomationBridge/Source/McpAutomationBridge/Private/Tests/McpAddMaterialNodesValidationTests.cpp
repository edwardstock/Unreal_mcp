// Tests for McpValidateNodeSpec (Task C.1).
// We test only the validator (not the full handler — handler is exercised via smoke test).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "../McpMaterialExpressionCatalog.h"

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
        TArray<FString>& OutDidYouMean);
}

namespace
{
    TSharedPtr<FJsonObject> MakeNode(const FString& NodeType)
    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("nodeType"), NodeType);
        return O;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpAddMaterialNodesValidate_ExactType,
    "LHGame.Mcp.Material.AddNodes.Validate.ExactType",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpAddMaterialNodesValidate_ExactType::RunTest(const FString&)
{
    TSet<FString> Seen;
    UClass* Cls = nullptr; bool bAuto = false;
    FString Code, Field, Msg; TArray<FString> DYM;

    auto Item = MakeNode(TEXT("MaterialExpressionMultiply"));
    const bool bOk = McpAddMaterialNodesValidationForTests::ValidateNodeSpec(
        0, Item, Seen, Cls, bAuto, Code, Field, Msg, DYM);

    TestTrue(TEXT("exact type ok"), bOk);
    TestNotNull(TEXT("class resolved"), Cls);
    TestFalse(TEXT("not auto-prefixed"), bAuto);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpAddMaterialNodesValidate_AutoPrefix,
    "LHGame.Mcp.Material.AddNodes.Validate.AutoPrefix",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpAddMaterialNodesValidate_AutoPrefix::RunTest(const FString&)
{
    TSet<FString> Seen;
    UClass* Cls = nullptr; bool bAuto = false;
    FString Code, Field, Msg; TArray<FString> DYM;

    auto Item = MakeNode(TEXT("Multiply"));
    const bool bOk = McpAddMaterialNodesValidationForTests::ValidateNodeSpec(
        0, Item, Seen, Cls, bAuto, Code, Field, Msg, DYM);

    TestTrue(TEXT("auto-prefix ok"), bOk);
    TestNotNull(TEXT("class resolved"), Cls);
    TestTrue(TEXT("flagged auto-prefixed"), bAuto);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpAddMaterialNodesValidate_UnknownType,
    "LHGame.Mcp.Material.AddNodes.Validate.UnknownType",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpAddMaterialNodesValidate_UnknownType::RunTest(const FString&)
{
    TSet<FString> Seen;
    UClass* Cls = nullptr; bool bAuto = false;
    FString Code, Field, Msg; TArray<FString> DYM;

    auto Item = MakeNode(TEXT("MaterialExpressionMul"));
    const bool bOk = McpAddMaterialNodesValidationForTests::ValidateNodeSpec(
        0, Item, Seen, Cls, bAuto, Code, Field, Msg, DYM);

    TestFalse(TEXT("unknown type fails"), bOk);
    TestEqual(TEXT("error code INVALID_NODE_TYPE"), Code, FString(TEXT("INVALID_NODE_TYPE")));
    TestTrue(TEXT("didYouMean populated"), DYM.Num() > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpAddMaterialNodesValidate_LocalIdDuplicate,
    "LHGame.Mcp.Material.AddNodes.Validate.LocalIdDuplicate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpAddMaterialNodesValidate_LocalIdDuplicate::RunTest(const FString&)
{
    TSet<FString> Seen;
    UClass* Cls = nullptr; bool bAuto = false;
    FString Code, Field, Msg; TArray<FString> DYM;

    // First node — set localId
    {
        auto Item = MakeNode(TEXT("MaterialExpressionMultiply"));
        Item->SetStringField(TEXT("localId"), TEXT("a"));
        const bool bOk = McpAddMaterialNodesValidationForTests::ValidateNodeSpec(
            0, Item, Seen, Cls, bAuto, Code, Field, Msg, DYM);
        TestTrue(TEXT("first ok"), bOk);
    }
    // Second node — same localId
    {
        auto Item = MakeNode(TEXT("MaterialExpressionAdd"));
        Item->SetStringField(TEXT("localId"), TEXT("a"));
        const bool bOk = McpAddMaterialNodesValidationForTests::ValidateNodeSpec(
            1, Item, Seen, Cls, bAuto, Code, Field, Msg, DYM);
        TestFalse(TEXT("duplicate fails"), bOk);
        TestEqual(TEXT("code LOCAL_ID_DUPLICATE"), Code, FString(TEXT("LOCAL_ID_DUPLICATE")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpAddMaterialNodesValidate_FieldNotApplicable,
    "LHGame.Mcp.Material.AddNodes.Validate.FieldNotApplicable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpAddMaterialNodesValidate_FieldNotApplicable::RunTest(const FString&)
{
    TSet<FString> Seen;
    UClass* Cls = nullptr; bool bAuto = false;
    FString Code, Field, Msg; TArray<FString> DYM;

    auto Item = MakeNode(TEXT("MaterialExpressionMultiply"));
    Item->SetNumberField(TEXT("defaultValue"), 1.0);
    const bool bOk = McpAddMaterialNodesValidationForTests::ValidateNodeSpec(
        0, Item, Seen, Cls, bAuto, Code, Field, Msg, DYM);

    TestFalse(TEXT("non-applicable field rejected"), bOk);
    TestEqual(TEXT("code FIELD_NOT_APPLICABLE"), Code, FString(TEXT("FIELD_NOT_APPLICABLE")));
    TestEqual(TEXT("field defaultValue"), Field, FString(TEXT("defaultValue")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpAddMaterialNodesValidate_EnumValid,
    "LHGame.Mcp.Material.AddNodes.Validate.EnumValid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpAddMaterialNodesValidate_EnumValid::RunTest(const FString&)
{
    // worldPositionShaderOffset is a reflected enum field on MaterialExpressionWorldPosition.
    // Catalog strips prefix WPT_, so "CameraRelative" should match WPT_CameraRelative.
    const FMcpMaterialExpressionCatalog& Cat = FMcpMaterialExpressionCatalog::Get();
    const TArray<FString>* Values = Cat.GetFieldEnumValues(
        TEXT("MaterialExpressionWorldPosition"), TEXT("worldPositionShaderOffset"));
    if (!Values || Values->Num() == 0)
    {
        // Skip — catalog entry not present in this build (defensive)
        AddInfo(TEXT("worldPositionShaderOffset not enum-typed in catalog; skipping"));
        return true;
    }

    // Pick an existing valid value
    const FString Sample = (*Values)[0];

    TSet<FString> Seen;
    UClass* Cls = nullptr; bool bAuto = false;
    FString Code, Field, Msg; TArray<FString> DYM;

    auto Item = MakeNode(TEXT("MaterialExpressionWorldPosition"));
    Item->SetStringField(TEXT("worldPositionShaderOffset"), Sample);
    const bool bOk = McpAddMaterialNodesValidationForTests::ValidateNodeSpec(
        0, Item, Seen, Cls, bAuto, Code, Field, Msg, DYM);

    TestTrue(TEXT("valid enum value accepted"), bOk);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpAddMaterialNodesValidate_EnumInvalid,
    "LHGame.Mcp.Material.AddNodes.Validate.EnumInvalid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpAddMaterialNodesValidate_EnumInvalid::RunTest(const FString&)
{
    const FMcpMaterialExpressionCatalog& Cat = FMcpMaterialExpressionCatalog::Get();
    const TArray<FString>* Values = Cat.GetFieldEnumValues(
        TEXT("MaterialExpressionWorldPosition"), TEXT("worldPositionShaderOffset"));
    if (!Values || Values->Num() == 0)
    {
        AddInfo(TEXT("worldPositionShaderOffset not enum-typed in catalog; skipping"));
        return true;
    }

    TSet<FString> Seen;
    UClass* Cls = nullptr; bool bAuto = false;
    FString Code, Field, Msg; TArray<FString> DYM;

    auto Item = MakeNode(TEXT("MaterialExpressionWorldPosition"));
    Item->SetStringField(TEXT("worldPositionShaderOffset"), TEXT("BogusValueDoesNotExist"));
    const bool bOk = McpAddMaterialNodesValidationForTests::ValidateNodeSpec(
        0, Item, Seen, Cls, bAuto, Code, Field, Msg, DYM);

    TestFalse(TEXT("invalid enum rejected"), bOk);
    TestEqual(TEXT("code INVALID_ENUM_VALUE"), Code, FString(TEXT("INVALID_ENUM_VALUE")));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
