using System.IO;
using UnrealBuildTool;

public class McpAutomationBridgeTests : ModuleRules
{
    public McpAutomationBridgeTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "McpAutomationBridge",
            "Json",
            "AssetTools",
            "AssetRegistry",
            "EditorScriptingUtilities",
            "Slate",
            "SlateCore",
        });

        // Allow tests to include private headers from the McpAutomationBridge
        // module (e.g. MCP/McpToolRegistry.h) without changing the bridge module's
        // own public/private layout.
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "McpAutomationBridge", "Private"));
    }
}
