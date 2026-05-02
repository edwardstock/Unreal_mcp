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
    }
}
