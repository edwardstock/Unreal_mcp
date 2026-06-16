#pragma once

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "MCP/McpToolRegistry.h"

// Locks in the public API the codegen pipeline depends on:
// FMcpToolRegistry::BuildToolManifest() must return one JSON object per
// registered tool, with name + inputSchema present.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpToolRegistryBuildManifestReturnsAllToolsTest,
    "LHGame.Mcp.ToolRegistry.BuildManifestReturnsAllTools",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpToolRegistryBuildManifestReturnsAllToolsTest::RunTest(const FString& Parameters)
{
    FModuleManager::Get().LoadModuleChecked(TEXT("McpAutomationBridge"));

    FMcpToolRegistry& Registry = FMcpToolRegistry::Get();
    const int32 ExpectedCount = Registry.GetToolCount();

    TestTrue(TEXT("Registry has at least one tool registered"), ExpectedCount > 0);

    const TArray<TSharedPtr<FJsonObject>> Manifest = Registry.BuildToolManifest();
    TestEqual(TEXT("Manifest size matches registered tool count"),
        Manifest.Num(), ExpectedCount);

    for (const TSharedPtr<FJsonObject>& Entry : Manifest)
    {
        if (!Entry.IsValid())
        {
            AddError(TEXT("Manifest contains a null entry"));
            continue;
        }
        FString Name;
        TestTrue(TEXT("Each entry has a non-empty 'name' field"),
            Entry->TryGetStringField(TEXT("name"), Name) && !Name.IsEmpty());
        TestTrue(TEXT("Each entry has an 'inputSchema' object"),
            Entry->HasTypedField<EJson::Object>(TEXT("inputSchema")));
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
