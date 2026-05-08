// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_Asset_Validation.cpp
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

// ============================================================================
// 1. FIXUP REDIRECTORS
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleFixupRedirectors(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("fixup_redirectors"), ESearchCase::IgnoreCase)) {
    // Not our action — allow other handlers to try
    return false;
  }

  // Implementation of redirector fixup functionality
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("fixup_redirectors payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Get directory path - REQUIRED for proper error reporting
  FString DirectoryPath;
  Payload->TryGetStringField(TEXT("directoryPath"), DirectoryPath);
  
  // Also check for "path" as alias
  if (DirectoryPath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("path"), DirectoryPath);
  }

  bool bCheckoutFiles = false;
  Payload->TryGetBoolField(TEXT("checkoutFiles"), bCheckoutFiles);

  // Validate path is provided
  if (DirectoryPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("directoryPath or path is required for fixup_redirectors"),
                        TEXT("MISSING_ARGUMENT"));
    return true;
  }

  // SECURITY: Sanitize path to prevent traversal attacks
  FString SanitizedPath = SanitizeProjectRelativePath(DirectoryPath);
  if (SanitizedPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Invalid path (traversal/security violation): %s"), *DirectoryPath),
        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Normalize path
  FString NormalizedPath = SanitizedPath;
  if (NormalizedPath.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
    NormalizedPath = FString::Printf(TEXT("/Game%s"), *NormalizedPath.RightChop(8));
  }

  AsyncTask(ENamedThreads::GameThread, [this, RequestId, NormalizedPath,
                                        bCheckoutFiles, RequestingSocket]() {
    // CRITICAL FIX: Use DoesAssetDirectoryExistOnDisk for strict validation
    // UEditorAssetLibrary::DoesDirectoryExist() uses AssetRegistry cache which may
    // contain stale entries. We need to check if the directory ACTUALLY exists on disk.
    if (!DoesAssetDirectoryExistOnDisk(NormalizedPath)) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Directory not found: %s"), *NormalizedPath),
                          TEXT("PATH_NOT_FOUND"));
      return;
    }

    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

    // Find all redirectors
    FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CoreUObject"),
                                             TEXT("ObjectRedirector")));
#else
    Filter.ClassNames.Add(FName(TEXT("ObjectRedirector")));
#endif

    Filter.PackagePaths.Add(FName(*NormalizedPath));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> RedirectorAssets;
    AssetRegistry.GetAssets(Filter, RedirectorAssets);

    if (RedirectorAssets.Num() == 0) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetBoolField(TEXT("success"), true);
      Result->SetNumberField(TEXT("redirectorsFound"), 0);
      Result->SetNumberField(TEXT("redirectorsFixed"), 0);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("No redirectors found"), Result, FString());
      return;
    }

    // Convert to string paths for AssetTools
    TArray<FString> RedirectorPaths;
    for (const FAssetData &Asset : RedirectorAssets) {
      RedirectorPaths.Add(Asset.ToSoftObjectPath().ToString());
    }

    // Checkout files if source control is enabled
    if (bCheckoutFiles && ISourceControlModule::Get().IsEnabled()) {
      ISourceControlProvider &SourceControlProvider =
          ISourceControlModule::Get().GetProvider();
      TArray<FString> PackageNames;
      for (const FAssetData &Asset : RedirectorAssets) {
        PackageNames.Add(Asset.PackageName.ToString());
      }
      SourceControlHelpers::CheckOutFiles(PackageNames, true);
    }

    // Convert FAssetData to UObjectRedirector* for AssetTools
    TArray<UObjectRedirector *> Redirectors;
    for (const FAssetData &Asset : RedirectorAssets) {
      if (UObjectRedirector *Redirector =
              Cast<UObjectRedirector>(Asset.GetAsset())) {
        Redirectors.Add(Redirector);
      }
    }

    // Fixup redirectors using AssetTools
    if (Redirectors.Num() > 0) {
      IAssetTools &AssetTools =
          FModuleManager::LoadModuleChecked<FAssetToolsModule>(
              TEXT("AssetTools"))
              .Get();
      AssetTools.FixupReferencers(Redirectors);
    }

    // Delete the now-unused redirectors
    int32 DeletedCount = 0;
    TArray<UObject *> ObjectsToDelete;
    for (const FAssetData &Asset : RedirectorAssets) {
      if (UObject *Obj = Asset.GetAsset()) {
        ObjectsToDelete.Add(Obj);
      }
    }

    if (ObjectsToDelete.Num() > 0) {
      DeletedCount = ObjectTools::DeleteObjects(ObjectsToDelete, false);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("redirectorsFound"), RedirectorAssets.Num());
    Result->SetNumberField(TEXT("redirectorsFixed"), DeletedCount);

    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Fixed %d redirectors"), DeletedCount), Result,
        FString());
  });

  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("fixup_redirectors requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleValidateAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("validate payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AsyncTask(ENamedThreads::GameThread, [this, RequestId, Socket, AssetPath]() {
    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
      SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                             nullptr, TEXT("ASSET_NOT_FOUND"));
      return;
    }

    UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset) {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Failed to load asset"), nullptr,
                             TEXT("LOAD_FAILED"));
      return;
    }

    bool bIsValid = true;
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), bIsValid);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetBoolField(TEXT("isValid"), bIsValid);

    SendAutomationResponse(Socket, RequestId, true, TEXT("Asset validated"),
                           Resp, FString());
  });
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}
