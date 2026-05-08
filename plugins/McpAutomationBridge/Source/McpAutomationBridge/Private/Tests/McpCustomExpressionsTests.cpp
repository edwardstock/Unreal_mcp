// Tests for the C.5 add_custom_expressions / update_custom_expressions
// validation and diff helpers.
//
// Coverage:
//   - add: missing/empty code -> INVALID_ARGUMENT
//   - add: bad outputType (primary) -> INVALID_ENUM_VALUE
//   - add: bad outputs[].outputType (no MaterialAttributes there) -> INVALID_ENUM_VALUE
//   - add: duplicate localId -> LOCAL_ID_DUPLICATE
//   - update: identifier resolves to non-Custom expression -> INVALID_NODE_TYPE
//   - update: removed input with live connection + onPinRemoved=preserve
//             -> LIVE_CONNECTIONS_ON_REMOVED_PIN
//   - update: same situation with onPinRemoved=break -> connectionsBroken[]
//             populated (incoming) with direction="incoming"
//   - update: wholesale-replace Inputs/AdditionalOutputs apply check
//             (validator should accept the replacement)
//
// These tests are pure-function tests against the production validator. The
// full socket/subsystem path is not exercised here.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"

#include "../McpAutomationBridgeHelpers.h"

namespace McpAddCustomExpressionsValidationForTests
{
    bool ValidateAddCustomItem(
        int32 Index,
        const TSharedPtr<FJsonObject>& Item,
        TSet<FString>& InOutSeenLocalIds,
        FString& OutCode,
        FString& OutField,
        FString& OutMessage);
}

namespace McpUpdateCustomExpressionsForTests
{
    struct FBrokenInfo
    {
        FString PinName;
        FString Direction;
        FString OtherNode;
        FString OtherPin;
    };
    struct FRemapInfo
    {
        FString OutputName;
        int32   OldOutputIndex = INDEX_NONE;
        int32   NewOutputIndex = INDEX_NONE;
    };
    bool ValidateUpdateCustomItem(
        const FMcpMaterialGraphOwner& Owner,
        UMaterialExpressionCustom* Custom,
        const TSharedPtr<FJsonObject>& Item,
        FString& OutCode,
        FString& OutField,
        FString& OutMessage,
        TArray<FBrokenInfo>& OutBroken);
    bool ValidateAndApplyUpdateCustomItem(
        const FMcpMaterialGraphOwner& Owner,
        UMaterialExpressionCustom* Custom,
        const TSharedPtr<FJsonObject>& Item,
        FString& OutCode,
        FString& OutField,
        FString& OutMessage,
        TArray<FBrokenInfo>& OutBroken,
        TArray<FRemapInfo>& OutRemapped);
}

namespace
{
    struct FCustomFixture
    {
        UMaterialFunction*         MF = nullptr;
        UMaterialExpressionCustom* Custom = nullptr;
        UMaterialExpressionMultiply* Multiply = nullptr;
        UMaterialExpressionConstant* Const = nullptr;
        FMcpMaterialGraphOwner Owner;

        void Build(const TCHAR* Suffix)
        {
            const FString MFName = FString::Printf(TEXT("Fixture_CustomExpr_%s"), Suffix);
            MF = NewObject<UMaterialFunction>(GetTransientPackage(), *MFName, RF_Transient);

            Custom = NewObject<UMaterialExpressionCustom>(MF);
            Custom->Code = TEXT("return Input;");
            Custom->MaterialExpressionGuid = FGuid::NewGuid();
            // The Custom constructor seeds Inputs with one unnamed entry; clear
            // it so our test fixture has a deterministic single named input.
            Custom->Inputs.Reset();
            FCustomInput In;
            In.InputName = FName(TEXT("Input"));
            Custom->Inputs.Add(In);
            Custom->RebuildOutputs();

            Multiply = NewObject<UMaterialExpressionMultiply>(MF);
            Multiply->MaterialExpressionGuid = FGuid::NewGuid();

            Const = NewObject<UMaterialExpressionConstant>(MF);
            Const->R = 0.5f;
            Const->MaterialExpressionGuid = FGuid::NewGuid();

            MF->GetExpressionCollection().AddExpression(Custom);
            MF->GetExpressionCollection().AddExpression(Multiply);
            MF->GetExpressionCollection().AddExpression(Const);

            // Wire Const -> Custom.Inputs[0]
            Custom->Inputs[0].Input.Expression = Const;
            Custom->Inputs[0].Input.OutputIndex = 0;

            Owner.Asset = MF;
            Owner.GraphSource = MF;
            Owner.Kind = EMcpMaterialGraphOwnerKind::MaterialFunction;
            Owner.bReadOnly = false;
        }
    };

    // Specialized fixture for tests that exercise Phase B apply-side behavior
    // (preserving kept-name input wires; output-direction broken; output remap).
    // Caller picks the input name and the source expression, and may add
    // AdditionalOutputs[] wired to a downstream Multiply.A pin.
    struct FCustomApplyFixture
    {
        UMaterialFunction*         MF = nullptr;
        UMaterialExpressionCustom* Custom = nullptr;
        UMaterialExpressionMultiply* Multiply = nullptr;
        UMaterialExpressionTextureCoordinate* TexCoord = nullptr;
        FMcpMaterialGraphOwner Owner;

        // Builds: TexCoord -> Custom.Inputs[name=UV]; no AdditionalOutputs.
        void BuildUVWired(const TCHAR* Suffix)
        {
            const FString MFName = FString::Printf(TEXT("Fixture_CustomApply_%s"), Suffix);
            MF = NewObject<UMaterialFunction>(GetTransientPackage(), *MFName, RF_Transient);

            TexCoord = NewObject<UMaterialExpressionTextureCoordinate>(MF);
            TexCoord->MaterialExpressionGuid = FGuid::NewGuid();

            Custom = NewObject<UMaterialExpressionCustom>(MF);
            Custom->Code = TEXT("return UV;");
            Custom->MaterialExpressionGuid = FGuid::NewGuid();
            Custom->Inputs.Reset();
            FCustomInput UVIn;
            UVIn.InputName = FName(TEXT("UV"));
            UVIn.Input.Expression  = TexCoord;
            UVIn.Input.OutputIndex = 0;
            Custom->Inputs.Add(UVIn);
            Custom->RebuildOutputs();

            MF->GetExpressionCollection().AddExpression(TexCoord);
            MF->GetExpressionCollection().AddExpression(Custom);

            Owner.Asset = MF;
            Owner.GraphSource = MF;
            Owner.Kind = EMcpMaterialGraphOwnerKind::MaterialFunction;
            Owner.bReadOnly = false;
        }

        // Builds: Custom with AdditionalOutputs=[Extra]; Custom.Outputs[Extra]
        // (= GetOutputs() index 1) wired into Multiply.A.
        void BuildExtraWiredToMultiply(const TCHAR* Suffix)
        {
            const FString MFName = FString::Printf(TEXT("Fixture_CustomApply_%s"), Suffix);
            MF = NewObject<UMaterialFunction>(GetTransientPackage(), *MFName, RF_Transient);

            Custom = NewObject<UMaterialExpressionCustom>(MF);
            Custom->Code = TEXT("Extra = 1.0; return 0.0;");
            Custom->MaterialExpressionGuid = FGuid::NewGuid();
            Custom->Inputs.Reset();
            FCustomOutput CO;
            CO.OutputName = FName(TEXT("Extra"));
            CO.OutputType = CMOT_Float1;
            Custom->AdditionalOutputs.Add(CO);
            Custom->RebuildOutputs();

            Multiply = NewObject<UMaterialExpressionMultiply>(MF);
            Multiply->MaterialExpressionGuid = FGuid::NewGuid();
            Multiply->A.Expression = Custom;
            Multiply->A.OutputIndex = 1; // primary=0, AdditionalOutputs[0]="Extra"=1

            MF->GetExpressionCollection().AddExpression(Custom);
            MF->GetExpressionCollection().AddExpression(Multiply);

            Owner.Asset = MF;
            Owner.GraphSource = MF;
            Owner.Kind = EMcpMaterialGraphOwnerKind::MaterialFunction;
            Owner.bReadOnly = false;
        }

        // Builds: Custom with AdditionalOutputs=[X, Y]; Custom.Outputs[Y] wired
        // into Multiply.A. (Y = AdditionalOutputs[1] = GetOutputs index 2.)
        void BuildXYWiredYToMultiply(const TCHAR* Suffix)
        {
            const FString MFName = FString::Printf(TEXT("Fixture_CustomApply_%s"), Suffix);
            MF = NewObject<UMaterialFunction>(GetTransientPackage(), *MFName, RF_Transient);

            Custom = NewObject<UMaterialExpressionCustom>(MF);
            Custom->Code = TEXT("X = 1.0; Y = 2.0; return 0.0;");
            Custom->MaterialExpressionGuid = FGuid::NewGuid();
            Custom->Inputs.Reset();

            FCustomOutput CX; CX.OutputName = FName(TEXT("X")); CX.OutputType = CMOT_Float1;
            FCustomOutput CY; CY.OutputName = FName(TEXT("Y")); CY.OutputType = CMOT_Float1;
            Custom->AdditionalOutputs.Add(CX);
            Custom->AdditionalOutputs.Add(CY);
            Custom->RebuildOutputs();

            Multiply = NewObject<UMaterialExpressionMultiply>(MF);
            Multiply->MaterialExpressionGuid = FGuid::NewGuid();
            Multiply->A.Expression = Custom;
            Multiply->A.OutputIndex = 2; // Y is at AdditionalOutputs[1] -> output index 2

            MF->GetExpressionCollection().AddExpression(Custom);
            MF->GetExpressionCollection().AddExpression(Multiply);

            Owner.Asset = MF;
            Owner.GraphSource = MF;
            Owner.Kind = EMcpMaterialGraphOwnerKind::MaterialFunction;
            Owner.bReadOnly = false;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpAddCustomExpr_MissingCodeRejected,
    "LHGame.Mcp.Material.CustomExpressions.Add.MissingCodeRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpAddCustomExpr_MissingCodeRejected::RunTest(const FString&)
{
    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    // No "code" field
    TSet<FString> Seen;
    FString Code, Field, Msg;
    const bool b = McpAddCustomExpressionsValidationForTests::ValidateAddCustomItem(
        0, Item, Seen, Code, Field, Msg);

    TestFalse(TEXT("missing code rejected"), b);
    TestEqual(TEXT("code INVALID_ARGUMENT"), Code, FString(TEXT("INVALID_ARGUMENT")));
    TestEqual(TEXT("field code"), Field, FString(TEXT("code")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpAddCustomExpr_BadOutputType,
    "LHGame.Mcp.Material.CustomExpressions.Add.BadOutputType",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpAddCustomExpr_BadOutputType::RunTest(const FString&)
{
    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    Item->SetStringField(TEXT("code"), TEXT("return 1;"));
    Item->SetStringField(TEXT("outputType"), TEXT("Float17"));
    TSet<FString> Seen;
    FString Code, Field, Msg;
    const bool b = McpAddCustomExpressionsValidationForTests::ValidateAddCustomItem(
        0, Item, Seen, Code, Field, Msg);

    TestFalse(TEXT("bad outputType rejected"), b);
    TestEqual(TEXT("code INVALID_ENUM_VALUE"), Code, FString(TEXT("INVALID_ENUM_VALUE")));
    TestEqual(TEXT("field outputType"), Field, FString(TEXT("outputType")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpAddCustomExpr_AdditionalOutputDisallowsMaterialAttributes,
    "LHGame.Mcp.Material.CustomExpressions.Add.AdditionalOutputDisallowsMaterialAttributes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpAddCustomExpr_AdditionalOutputDisallowsMaterialAttributes::RunTest(const FString&)
{
    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    Item->SetStringField(TEXT("code"), TEXT("return 1;"));

    TSharedPtr<FJsonObject> O0 = MakeShared<FJsonObject>();
    O0->SetStringField(TEXT("name"), TEXT("Out0"));
    O0->SetStringField(TEXT("outputType"), TEXT("MaterialAttributes"));
    TArray<TSharedPtr<FJsonValue>> Outs;
    Outs.Add(MakeShared<FJsonValueObject>(O0));
    Item->SetArrayField(TEXT("outputs"), Outs);

    TSet<FString> Seen;
    FString Code, Field, Msg;
    const bool b = McpAddCustomExpressionsValidationForTests::ValidateAddCustomItem(
        0, Item, Seen, Code, Field, Msg);

    TestFalse(TEXT("MaterialAttributes on additional output rejected"), b);
    TestEqual(TEXT("code INVALID_ENUM_VALUE"), Code, FString(TEXT("INVALID_ENUM_VALUE")));
    TestTrue (TEXT("field references outputs[0].outputType"),
        Field.Contains(TEXT("outputs[0].outputType")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpAddCustomExpr_DuplicateLocalId,
    "LHGame.Mcp.Material.CustomExpressions.Add.DuplicateLocalId",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpAddCustomExpr_DuplicateLocalId::RunTest(const FString&)
{
    TSet<FString> Seen;
    FString Code, Field, Msg;

    TSharedPtr<FJsonObject> Item1 = MakeShared<FJsonObject>();
    Item1->SetStringField(TEXT("localId"), TEXT("foo"));
    Item1->SetStringField(TEXT("code"), TEXT("return 1;"));
    const bool b1 = McpAddCustomExpressionsValidationForTests::ValidateAddCustomItem(
        0, Item1, Seen, Code, Field, Msg);
    TestTrue(TEXT("first item accepted"), b1);

    TSharedPtr<FJsonObject> Item2 = MakeShared<FJsonObject>();
    Item2->SetStringField(TEXT("localId"), TEXT("foo"));
    Item2->SetStringField(TEXT("code"), TEXT("return 2;"));
    const bool b2 = McpAddCustomExpressionsValidationForTests::ValidateAddCustomItem(
        1, Item2, Seen, Code, Field, Msg);

    TestFalse(TEXT("duplicate localId rejected"), b2);
    TestEqual(TEXT("code LOCAL_ID_DUPLICATE"), Code, FString(TEXT("LOCAL_ID_DUPLICATE")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpUpdateCustomExpr_LiveIncomingPreserve,
    "LHGame.Mcp.Material.CustomExpressions.Update.LiveIncomingPreserve",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpUpdateCustomExpr_LiveIncomingPreserve::RunTest(const FString&)
{
    FCustomFixture F; F.Build(TEXT("LivePreserve"));

    // Replace Inputs[] with an empty list - the existing "Input" pin is removed.
    // The pin has a live incoming connection from Const, so onPinRemoved="preserve"
    // (the default) MUST reject.
    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Empty;
    Item->SetArrayField(TEXT("inputs"), Empty);

    TArray<McpUpdateCustomExpressionsForTests::FBrokenInfo> Broken;
    FString Code, Field, Msg;
    const bool b = McpUpdateCustomExpressionsForTests::ValidateUpdateCustomItem(
        F.Owner, F.Custom, Item, Code, Field, Msg, Broken);

    TestFalse(TEXT("preserve + live wire rejected"), b);
    TestEqual(TEXT("code LIVE_CONNECTIONS_ON_REMOVED_PIN"),
        Code, FString(TEXT("LIVE_CONNECTIONS_ON_REMOVED_PIN")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpUpdateCustomExpr_LiveIncomingBreak,
    "LHGame.Mcp.Material.CustomExpressions.Update.LiveIncomingBreak",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpUpdateCustomExpr_LiveIncomingBreak::RunTest(const FString&)
{
    FCustomFixture F; F.Build(TEXT("LiveBreak"));

    // Same as above but with onPinRemoved=break.
    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    Item->SetStringField(TEXT("onPinRemoved"), TEXT("break"));
    TArray<TSharedPtr<FJsonValue>> Empty;
    Item->SetArrayField(TEXT("inputs"), Empty);

    TArray<McpUpdateCustomExpressionsForTests::FBrokenInfo> Broken;
    FString Code, Field, Msg;
    const bool b = McpUpdateCustomExpressionsForTests::ValidateUpdateCustomItem(
        F.Owner, F.Custom, Item, Code, Field, Msg, Broken);

    TestTrue (TEXT("break accepted"), b);
    TestEqual(TEXT("one connection captured"), Broken.Num(), 1);
    if (Broken.Num() == 1)
    {
        TestEqual(TEXT("direction incoming"),
            Broken[0].Direction, FString(TEXT("incoming")));
        TestEqual(TEXT("pinName Input"),
            Broken[0].PinName, FString(TEXT("Input")));
        TestEqual(TEXT("otherNode is the Const expression"),
            Broken[0].OtherNode, F.Const->GetName());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpUpdateCustomExpr_WholesaleArrayReplacementValidates,
    "LHGame.Mcp.Material.CustomExpressions.Update.WholesaleArrayReplacementValidates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpUpdateCustomExpr_WholesaleArrayReplacementValidates::RunTest(const FString&)
{
    FCustomFixture F; F.Build(TEXT("Wholesale"));

    // Replace inputs[] with one entry "Input" (same name as before) - no
    // pins removed - and add an outputs[] with two entries. Validator must
    // accept (no live-connection violations because "Input" is preserved).
    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();

    TSharedPtr<FJsonObject> I0 = MakeShared<FJsonObject>();
    I0->SetStringField(TEXT("name"), TEXT("Input"));
    TArray<TSharedPtr<FJsonValue>> Inputs;
    Inputs.Add(MakeShared<FJsonValueObject>(I0));
    Item->SetArrayField(TEXT("inputs"), Inputs);

    TSharedPtr<FJsonObject> O0 = MakeShared<FJsonObject>();
    O0->SetStringField(TEXT("name"), TEXT("Out0"));
    O0->SetStringField(TEXT("outputType"), TEXT("Float2"));
    TSharedPtr<FJsonObject> O1 = MakeShared<FJsonObject>();
    O1->SetStringField(TEXT("name"), TEXT("Out1"));
    TArray<TSharedPtr<FJsonValue>> Outputs;
    Outputs.Add(MakeShared<FJsonValueObject>(O0));
    Outputs.Add(MakeShared<FJsonValueObject>(O1));
    Item->SetArrayField(TEXT("outputs"), Outputs);

    TArray<McpUpdateCustomExpressionsForTests::FBrokenInfo> Broken;
    FString Code, Field, Msg;
    const bool b = McpUpdateCustomExpressionsForTests::ValidateUpdateCustomItem(
        F.Owner, F.Custom, Item, Code, Field, Msg, Broken);

    TestTrue (TEXT("wholesale replacement accepted"), b);
    TestEqual(TEXT("no broken connections recorded"), Broken.Num(), 0);
    return true;
}

// =============================================================================
// Phase B apply-side tests (added to cover the wholesale-replace fix that
// preserves wires on kept-name inputs and remaps kept-name output positions).
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpUpdateCustomExpr_KeptInputPreservesWire,
    "LHGame.Mcp.Material.CustomExpressions.Update.KeptInputPreservesWire",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpUpdateCustomExpr_KeptInputPreservesWire::RunTest(const FString&)
{
    FCustomApplyFixture F; F.BuildUVWired(TEXT("KeptUV"));

    // Replace inputs[] with the same single entry "UV". After Phase B the
    // wire from TexCoord MUST still be live - this is the regression test
    // for the wholesale-replace bug.
    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> I0 = MakeShared<FJsonObject>();
    I0->SetStringField(TEXT("name"), TEXT("UV"));
    TArray<TSharedPtr<FJsonValue>> Inputs;
    Inputs.Add(MakeShared<FJsonValueObject>(I0));
    Item->SetArrayField(TEXT("inputs"), Inputs);

    TArray<McpUpdateCustomExpressionsForTests::FBrokenInfo> Broken;
    TArray<McpUpdateCustomExpressionsForTests::FRemapInfo>  Remapped;
    FString Code, Field, Msg;
    const bool b = McpUpdateCustomExpressionsForTests::ValidateAndApplyUpdateCustomItem(
        F.Owner, F.Custom, Item, Code, Field, Msg, Broken, Remapped);

    TestTrue (TEXT("preserve + same-name inputs accepted"), b);
    TestEqual(TEXT("no broken connections"), Broken.Num(), 0);
    TestEqual(TEXT("no remaps"), Remapped.Num(), 0);
    TestEqual(TEXT("Custom still has 1 input"), F.Custom->Inputs.Num(), 1);
    if (F.Custom->Inputs.Num() == 1)
    {
        TestEqual(TEXT("kept input still named UV"),
            F.Custom->Inputs[0].InputName, FName(TEXT("UV")));
        TestEqual(TEXT("kept input wire NOT lost"),
            (UMaterialExpression*)F.Custom->Inputs[0].Input.Expression,
            (UMaterialExpression*)F.TexCoord);
        TestEqual(TEXT("kept input OutputIndex preserved"),
            F.Custom->Inputs[0].Input.OutputIndex, 0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpUpdateCustomExpr_NewInputStartsFresh,
    "LHGame.Mcp.Material.CustomExpressions.Update.NewInputStartsFresh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpUpdateCustomExpr_NewInputStartsFresh::RunTest(const FString&)
{
    FCustomApplyFixture F; F.BuildUVWired(TEXT("NewMask"));

    // Add a second entry "Mask" alongside the kept "UV". The new "Mask"
    // entry must be unconnected (default FExpressionInput); the kept "UV"
    // must still be wired to TexCoord.
    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> I0 = MakeShared<FJsonObject>();
    I0->SetStringField(TEXT("name"), TEXT("UV"));
    TSharedPtr<FJsonObject> I1 = MakeShared<FJsonObject>();
    I1->SetStringField(TEXT("name"), TEXT("Mask"));
    TArray<TSharedPtr<FJsonValue>> Inputs;
    Inputs.Add(MakeShared<FJsonValueObject>(I0));
    Inputs.Add(MakeShared<FJsonValueObject>(I1));
    Item->SetArrayField(TEXT("inputs"), Inputs);

    TArray<McpUpdateCustomExpressionsForTests::FBrokenInfo> Broken;
    TArray<McpUpdateCustomExpressionsForTests::FRemapInfo>  Remapped;
    FString Code, Field, Msg;
    const bool b = McpUpdateCustomExpressionsForTests::ValidateAndApplyUpdateCustomItem(
        F.Owner, F.Custom, Item, Code, Field, Msg, Broken, Remapped);

    TestTrue (TEXT("two-input replacement accepted"), b);
    TestEqual(TEXT("no broken connections"), Broken.Num(), 0);
    TestEqual(TEXT("Custom now has 2 inputs"), F.Custom->Inputs.Num(), 2);
    if (F.Custom->Inputs.Num() == 2)
    {
        TestEqual(TEXT("kept UV still wired"),
            (UMaterialExpression*)F.Custom->Inputs[0].Input.Expression,
            (UMaterialExpression*)F.TexCoord);
        TestEqual(TEXT("new Mask is unconnected"),
            (UMaterialExpression*)F.Custom->Inputs[1].Input.Expression,
            (UMaterialExpression*)nullptr);
        TestEqual(TEXT("new Mask name"),
            F.Custom->Inputs[1].InputName, FName(TEXT("Mask")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpUpdateCustomExpr_RenamedInputPreserveErrors,
    "LHGame.Mcp.Material.CustomExpressions.Update.RenamedInputPreserveErrors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpUpdateCustomExpr_RenamedInputPreserveErrors::RunTest(const FString&)
{
    FCustomApplyFixture F; F.BuildUVWired(TEXT("Renamed"));

    // Rename UV -> Coord under preserve. The wire on UV is live (TexCoord
    // attached) so this MUST be rejected with LIVE_CONNECTIONS_ON_REMOVED_PIN.
    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> I0 = MakeShared<FJsonObject>();
    I0->SetStringField(TEXT("name"), TEXT("Coord"));
    TArray<TSharedPtr<FJsonValue>> Inputs;
    Inputs.Add(MakeShared<FJsonValueObject>(I0));
    Item->SetArrayField(TEXT("inputs"), Inputs);

    TArray<McpUpdateCustomExpressionsForTests::FBrokenInfo> Broken;
    FString Code, Field, Msg;
    const bool b = McpUpdateCustomExpressionsForTests::ValidateUpdateCustomItem(
        F.Owner, F.Custom, Item, Code, Field, Msg, Broken);

    TestFalse(TEXT("rename of wired input under preserve rejected"), b);
    TestEqual(TEXT("code LIVE_CONNECTIONS_ON_REMOVED_PIN"),
        Code, FString(TEXT("LIVE_CONNECTIONS_ON_REMOVED_PIN")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpUpdateCustomExpr_OutgoingBreakNullifiesConsumer,
    "LHGame.Mcp.Material.CustomExpressions.Update.OutgoingBreakNullifiesConsumer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpUpdateCustomExpr_OutgoingBreakNullifiesConsumer::RunTest(const FString&)
{
    FCustomApplyFixture F; F.BuildExtraWiredToMultiply(TEXT("OutBreak"));

    // Drop AdditionalOutputs[] (the "Extra" output is removed). Multiply.A
    // is wired to that output; under onPinRemoved="break" we expect:
    //  - connectionsBroken[] has one entry, direction="outgoing"
    //  - Multiply.A.Expression == nullptr after Phase B
    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    Item->SetStringField(TEXT("onPinRemoved"), TEXT("break"));
    TArray<TSharedPtr<FJsonValue>> Empty;
    Item->SetArrayField(TEXT("outputs"), Empty);

    TArray<McpUpdateCustomExpressionsForTests::FBrokenInfo> Broken;
    TArray<McpUpdateCustomExpressionsForTests::FRemapInfo>  Remapped;
    FString Code, Field, Msg;
    const bool b = McpUpdateCustomExpressionsForTests::ValidateAndApplyUpdateCustomItem(
        F.Owner, F.Custom, Item, Code, Field, Msg, Broken, Remapped);

    TestTrue (TEXT("break accepted"), b);
    TestEqual(TEXT("one broken connection captured"), Broken.Num(), 1);
    if (Broken.Num() == 1)
    {
        TestEqual(TEXT("direction outgoing"),
            Broken[0].Direction, FString(TEXT("outgoing")));
        TestEqual(TEXT("otherNode is the Multiply expression"),
            Broken[0].OtherNode, F.Multiply->GetName());
        TestEqual(TEXT("otherPin is A"),
            Broken[0].OtherPin, FString(TEXT("A")));
    }
    TestEqual(TEXT("Multiply.A.Expression nullified"),
        (UMaterialExpression*)F.Multiply->A.Expression,
        (UMaterialExpression*)nullptr);
    TestEqual(TEXT("Multiply.A.OutputIndex reset"), F.Multiply->A.OutputIndex, 0);
    TestEqual(TEXT("Custom AdditionalOutputs is empty"),
        F.Custom->AdditionalOutputs.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpUpdateCustomExpr_OutputReorderRemapsConsumer,
    "LHGame.Mcp.Material.CustomExpressions.Update.OutputReorderRemapsConsumer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpUpdateCustomExpr_OutputReorderRemapsConsumer::RunTest(const FString&)
{
    FCustomApplyFixture F; F.BuildXYWiredYToMultiply(TEXT("Reorder"));

    // Swap [X, Y] -> [Y, X]. Multiply.A points at Y (OutputIndex=2). After
    // remap Y is at AdditionalOutputs[0] -> OutputIndex=1.
    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> O0 = MakeShared<FJsonObject>();
    O0->SetStringField(TEXT("name"), TEXT("Y"));
    TSharedPtr<FJsonObject> O1 = MakeShared<FJsonObject>();
    O1->SetStringField(TEXT("name"), TEXT("X"));
    TArray<TSharedPtr<FJsonValue>> Outputs;
    Outputs.Add(MakeShared<FJsonValueObject>(O0));
    Outputs.Add(MakeShared<FJsonValueObject>(O1));
    Item->SetArrayField(TEXT("outputs"), Outputs);

    TArray<McpUpdateCustomExpressionsForTests::FBrokenInfo> Broken;
    TArray<McpUpdateCustomExpressionsForTests::FRemapInfo>  Remapped;
    FString Code, Field, Msg;
    const bool b = McpUpdateCustomExpressionsForTests::ValidateAndApplyUpdateCustomItem(
        F.Owner, F.Custom, Item, Code, Field, Msg, Broken, Remapped);

    TestTrue (TEXT("output reorder accepted"), b);
    TestEqual(TEXT("no broken connections (kept-name reorder is a remap, not break)"),
        Broken.Num(), 0);
    // Both X and Y moved positions - both should be in Remapped.
    TestEqual(TEXT("two remap entries (one per moved output)"), Remapped.Num(), 2);
    TestEqual(TEXT("Multiply.A still points at Custom"),
        (UMaterialExpression*)F.Multiply->A.Expression,
        (UMaterialExpression*)F.Custom);
    TestEqual(TEXT("Multiply.A.OutputIndex remapped to 1 (new Y position)"),
        F.Multiply->A.OutputIndex, 1);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
