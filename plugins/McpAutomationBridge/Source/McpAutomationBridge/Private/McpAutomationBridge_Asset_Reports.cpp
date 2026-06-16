// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_Asset_Reports.cpp
//
// G.2: per-domain split of manage_asset handler bodies. The dispatcher in
// McpAutomationBridge_AssetWorkflowHandlers.cpp routes to these handlers by
// canonical plural action name.

#include "McpVersionCompatibility.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/ScopeExit.h"
#include "UObject/MetaData.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"
#include "McpSafeOperations.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "AssetViewUtils.h"
#include "EditorAssetLibrary.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "IAssetTools.h"
#include "Engine/StaticMesh.h"
#include "ObjectTools.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/FileHelper.h"

#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlHelpers.h"
#include "SourceControlOperations.h"

#include "ImageUtils.h"
#endif // WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleGenerateReport(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("generate_report payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Directory;
  Payload->TryGetStringField(TEXT("directory"), Directory);
  if (Directory.IsEmpty()) {
    Directory = TEXT("/Game");
  }

  // Normalize /Content prefix to /Game for convenience
  if (Directory.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
    Directory = FString::Printf(TEXT("/Game%s"), *Directory.RightChop(8));
  }

  FString ReportType;
  Payload->TryGetStringField(TEXT("reportType"), ReportType);
  if (ReportType.IsEmpty()) {
    ReportType = TEXT("Summary");
  }

  FString OutputPath;
  Payload->TryGetStringField(TEXT("outputPath"), OutputPath);

  AsyncTask(ENamedThreads::GameThread, [this, RequestId, Socket, Directory,
                                        ReportType, OutputPath]() {
    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    FARFilter Filter;
    Filter.bRecursivePaths = true;
    if (!Directory.IsEmpty()) {
      Filter.PackagePaths.Add(FName(*Directory));
    }

    // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
    // Asset listing uses cached AssetRegistry data exclusively.
    // LIMITATION: Assets not yet indexed by the editor's background scanner
    // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
    TArray<FAssetData> AssetList;
    AssetRegistryModule.Get().GetAssets(Filter, AssetList);

    TArray<TSharedPtr<FJsonValue>> AssetsArray;
    for (const FAssetData &Asset : AssetList) {
      TSharedPtr<FJsonObject> AssetObj = McpHandlerUtils::CreateResultObject();
      AssetObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
      AssetObj->SetStringField(TEXT("path"),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                               Asset.GetSoftObjectPath().ToString());
      AssetObj->SetStringField(TEXT("class"), Asset.AssetClassPath.ToString());
#else
                               Asset.ToSoftObjectPath().ToString());
      AssetObj->SetStringField(TEXT("class"), Asset.AssetClass.ToString());
#endif
      AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
    }

    bool bFileWritten = false;
    if (!OutputPath.IsEmpty()) {
      // SECURITY: Sanitize and validate the output path to prevent path traversal
      FString SafeOutputPath = SanitizeProjectFilePath(OutputPath);
      if (SafeOutputPath.IsEmpty()) {
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Invalid or unsafe output path: %s"), *OutputPath),
                            TEXT("SECURITY_VIOLATION"));
        return;
      }
      
      FString AbsoluteOutput = FPaths::ProjectDir() / SafeOutputPath;
      AbsoluteOutput = FPaths::ConvertRelativePathToFull(AbsoluteOutput);
      FPaths::NormalizeFilename(AbsoluteOutput);
      
      FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
      FPaths::NormalizeDirectoryName(NormalizedProjectDir);
      if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
        NormalizedProjectDir += TEXT("/");
      }
      
      if (!AbsoluteOutput.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase)) {
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Output path escapes project directory: %s"), *OutputPath),
                            TEXT("SECURITY_VIOLATION"));
        return;
      }

      const FString DirPath = FPaths::GetPath(AbsoluteOutput);
      IPlatformFile &PlatformFile =
          FPlatformFileManager::Get().GetPlatformFile();
      PlatformFile.CreateDirectoryTree(*DirPath);

      const FString FileContents = TEXT(
          "{\"report\":\"Asset report generated by MCP Automation Bridge\"}");
      bFileWritten =
          FFileHelper::SaveStringToFile(FileContents, *AbsoluteOutput);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("directory"), Directory);
    Resp->SetStringField(TEXT("reportType"), ReportType);
    Resp->SetNumberField(TEXT("assetCount"), AssetList.Num());
    Resp->SetArrayField(TEXT("assets"), AssetsArray);
    if (!OutputPath.IsEmpty()) {
      Resp->SetStringField(TEXT("outputPath"), OutputPath);
      Resp->SetBoolField(TEXT("fileWritten"), bFileWritten);
    }

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Asset report generated"), Resp, FString());
  });
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}
