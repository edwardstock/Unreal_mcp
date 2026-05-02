#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpMaterialDiagnosticsToolRegistrationTest,
    "LHGame.McpAutomationBridge.MaterialDiagnostics.ToolIsRegistered",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpMaterialDiagnosticsToolRegistrationTest::RunTest(const FString& Parameters)
{
    FMcpToolDefinition* Tool = FMcpToolRegistry::Get().FindTool(TEXT("manage_material_diagnostics"));
    TestNotNull(TEXT("manage_material_diagnostics native tool is registered"), Tool);
    if (!Tool)
    {
        return false;
    }

    const TSharedPtr<FJsonObject> Schema = Tool->BuildInputSchema();
    TestTrue(TEXT("diagnostics tool exposes an input schema"), Schema.IsValid());
    if (!Schema.IsValid())
    {
        return false;
    }

    const TSharedPtr<FJsonObject>* Properties = nullptr;
    TestTrue(TEXT("schema has properties"), Schema->TryGetObjectField(TEXT("properties"), Properties) && Properties && Properties->IsValid());
    if (!Properties || !Properties->IsValid())
    {
        return false;
    }

    TestTrue(TEXT("schema advertises subAction"), (*Properties)->HasField(TEXT("subAction")));
    TestTrue(TEXT("schema advertises assetPath"), (*Properties)->HasField(TEXT("assetPath")));
    return true;
}
#endif
