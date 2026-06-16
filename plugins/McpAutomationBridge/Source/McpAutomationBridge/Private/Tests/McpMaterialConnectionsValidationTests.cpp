// Tests for canonical material connection batch validation.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionStaticSwitch.h"

#include "../McpAutomationBridgeHelpers.h"

namespace McpMaterialConnectionsValidationForTests
{
    bool ValidateConnectionsBatch(
        const TArray<TSharedPtr<FJsonValue>>& ConnectionsJson,
        const TMap<FString, UClass*>& BatchLocalIdToClass,
        const FMcpMaterialGraphOwner& Owner,
        FString& OutCode,
        FString& OutField,
        FString& OutMessage);
}

namespace
{
    struct FConnectionFixture
    {
        UMaterialFunction* MF = nullptr;
        UMaterialExpressionStaticSwitch* Switch = nullptr;
        UMaterialExpressionMultiply* Multiply = nullptr;
        FMcpMaterialGraphOwner Owner;

        void Build(const TCHAR* Suffix)
        {
            const FString MFName = FString::Printf(TEXT("Fixture_MaterialConnections_%s"), Suffix);
            MF = NewObject<UMaterialFunction>(GetTransientPackage(), *MFName, RF_Transient);

            Switch = NewObject<UMaterialExpressionStaticSwitch>(MF);
            Switch->MaterialExpressionGuid = FGuid::NewGuid();
            MF->GetExpressionCollection().AddExpression(Switch);

            Multiply = NewObject<UMaterialExpressionMultiply>(MF);
            Multiply->MaterialExpressionGuid = FGuid::NewGuid();
            MF->GetExpressionCollection().AddExpression(Multiply);

            Owner.Asset = MF;
            Owner.GraphSource = MF;
            Owner.Kind = EMcpMaterialGraphOwnerKind::MaterialFunction;
            Owner.bReadOnly = false;
        }
    };

    TSharedPtr<FJsonObject> MakeConnection(
        const FString& FromNode,
        int32 FromOutputIndex,
        const FString& ToNode,
        const FString& ToPin)
    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("fromNode"), FromNode);
        O->SetNumberField(TEXT("fromOutputIndex"), FromOutputIndex);
        O->SetStringField(TEXT("toNode"), ToNode);
        O->SetStringField(TEXT("toPin"), ToPin);
        return O;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpMaterialConnectionsRejectsNonZeroStaticSwitchOutput,
    "LHGame.Mcp.Material.Connections.Validate.RejectsNonZeroStaticSwitchOutput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpMaterialConnectionsRejectsNonZeroStaticSwitchOutput::RunTest(const FString&)
{
    FConnectionFixture F;
    F.Build(TEXT("StaticSwitchOutput"));

    TArray<TSharedPtr<FJsonValue>> Connections;
    Connections.Add(MakeShared<FJsonValueObject>(
        MakeConnection(F.Switch->GetName(), 2, F.Multiply->GetName(), TEXT("A"))));

    FString Code, Field, Msg;
    const bool bOk = McpMaterialConnectionsValidationForTests::ValidateConnectionsBatch(
        Connections, TMap<FString, UClass*>(), F.Owner, Code, Field, Msg);

    TestFalse(TEXT("non-zero StaticSwitch output is rejected"), bOk);
    TestEqual(TEXT("error code"), Code, FString(TEXT("INVALID_SOURCE_OUTPUT_INDEX")));
    TestEqual(TEXT("field"), Field, FString(TEXT("fromOutputIndex")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpMaterialConnectionsRejectsNonZeroLocalStaticSwitchOutput,
    "LHGame.Mcp.Material.Connections.Validate.RejectsNonZeroLocalStaticSwitchOutput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpMaterialConnectionsRejectsNonZeroLocalStaticSwitchOutput::RunTest(const FString&)
{
    FConnectionFixture F;
    F.Build(TEXT("LocalStaticSwitchOutput"));

    TArray<TSharedPtr<FJsonValue>> Connections;
    Connections.Add(MakeShared<FJsonValueObject>(
        MakeConnection(TEXT("sw_orm_g"), 2, F.Multiply->GetName(), TEXT("A"))));

    TMap<FString, UClass*> BatchLocalIdToClass;
    BatchLocalIdToClass.Add(TEXT("sw_orm_g"), UMaterialExpressionStaticSwitch::StaticClass());

    FString Code, Field, Msg;
    const bool bOk = McpMaterialConnectionsValidationForTests::ValidateConnectionsBatch(
        Connections, BatchLocalIdToClass, F.Owner, Code, Field, Msg);

    TestFalse(TEXT("non-zero local StaticSwitch output is rejected"), bOk);
    TestEqual(TEXT("error code"), Code, FString(TEXT("INVALID_SOURCE_OUTPUT_INDEX")));
    TestEqual(TEXT("field"), Field, FString(TEXT("fromOutputIndex")));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
