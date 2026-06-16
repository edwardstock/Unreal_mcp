// DumpMcpManifestCommandlet.h
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DumpMcpManifestCommandlet.generated.h"

/**
 * Dumps FMcpToolRegistry's full tool manifest to a JSON file.
 *
 * Usage:
 *   UnrealEditor-Cmd.exe LHGame.uproject -run=DumpMcpManifest -Output=<path>
 *
 * If -Output= is omitted, writes to Plugins/Unreal_mcp/generated/tool-manifest.json
 * resolved from this plugin's base directory.
 */
UCLASS()
class UDumpMcpManifestCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UDumpMcpManifestCommandlet();
    virtual int32 Main(const FString& Params) override;
};
