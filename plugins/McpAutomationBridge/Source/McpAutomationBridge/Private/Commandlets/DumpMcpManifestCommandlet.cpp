// DumpMcpManifestCommandlet.cpp
#include "Commandlets/DumpMcpManifestCommandlet.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "MCP/McpToolRegistry.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

UDumpMcpManifestCommandlet::UDumpMcpManifestCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UDumpMcpManifestCommandlet::Main(const FString& Params)
{
    // The McpAutomationBridge module hosts FMcpToolRegistry. Static MCP_REGISTER_TOOL
    // initializers run on module load, so by the time we reach this line all 37 tools
    // are already in the registry.
    FModuleManager::Get().LoadModuleChecked(TEXT("McpAutomationBridge"));

    FString OutputPath;
    FParse::Value(*Params, TEXT("Output="), OutputPath);
    if (OutputPath.IsEmpty())
    {
        const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("McpAutomationBridge"));
        if (!Plugin.IsValid())
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
                TEXT("Cannot resolve McpAutomationBridge plugin base dir; pass -Output=<path>."));
            return 1;
        }
        // Plugin BaseDir is .../Plugins/Unreal_mcp/plugins/McpAutomationBridge.
        // The generated artifact lives two levels up at Plugins/Unreal_mcp/generated/.
        OutputPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT(".."), TEXT(".."),
            TEXT("generated"), TEXT("tool-manifest.json"));
        FPaths::CollapseRelativeDirectories(OutputPath);
    }

    const FString OutputDir = FPaths::GetPath(OutputPath);
    if (!FPaths::DirectoryExists(OutputDir))
    {
        IFileManager::Get().MakeDirectory(*OutputDir, /*Tree=*/true);
    }

    const TArray<TSharedPtr<FJsonObject>> Tools = FMcpToolRegistry::Get().BuildToolManifest();

    TArray<TSharedPtr<FJsonValue>> Values;
    Values.Reserve(Tools.Num());
    for (const TSharedPtr<FJsonObject>& Obj : Tools)
    {
        Values.Add(MakeShared<FJsonValueObject>(Obj));
    }

    FString Json;
    TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);
    if (!FJsonSerializer::Serialize(Values, Writer))
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("FJsonSerializer::Serialize failed"));
        return 1;
    }

    if (!FFileHelper::SaveStringToFile(Json, *OutputPath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("Failed to write %s"), *OutputPath);
        return 1;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Display, TEXT("DumpMcpManifest wrote %d tools to %s"),
        Tools.Num(), *OutputPath);
    return 0;
}
