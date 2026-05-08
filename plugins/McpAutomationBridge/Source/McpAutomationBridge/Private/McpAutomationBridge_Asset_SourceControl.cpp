// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_Asset_SourceControl.cpp
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
// 2. SOURCE CONTROL CHECKOUT
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleSourceControlCheckout(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("source_control_checkout"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("checkout"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("source_control_checkout payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPaths (array) and assetPath (single string)
  TArray<FString> AssetPaths;
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) &&
      AssetPathsArray && AssetPathsArray->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }
  } else {
    // Try single assetPath
    FString SinglePath;
    if (Payload->TryGetStringField(TEXT("assetPath"), SinglePath) && !SinglePath.IsEmpty()) {
      AssetPaths.Add(SinglePath);
    }
  }

  if (AssetPaths.Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("assetPath (string) or assetPaths (array) required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!ISourceControlModule::Get().IsEnabled()) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"),
                           TEXT("Source control is not enabled"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Source control disabled"), Result,
                           TEXT("SOURCE_CONTROL_DISABLED"));
    return true;
  }

  ISourceControlProvider &SourceControlProvider =
      ISourceControlModule::Get().GetProvider();

  TArray<FString> PackageNames;
  TArray<FString> ValidPaths;
  for (const FString &Path : AssetPaths) {
    if (UEditorAssetLibrary::DoesAssetExist(Path)) {
      ValidPaths.Add(Path);
      FString PackageName = FPackageName::ObjectPathToPackageName(Path);
      PackageNames.Add(PackageName);
    }
  }

  if (PackageNames.Num() == 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("No valid assets found"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("No valid assets"), Result,
                           TEXT("NO_VALID_ASSETS"));
    return true;
  }

  bool bSuccess = SourceControlHelpers::CheckOutFiles(PackageNames, true);

  TArray<TSharedPtr<FJsonValue>> CheckedOutPaths;
  for (const FString &Path : ValidPaths) {
    CheckedOutPaths.Add(MakeShared<FJsonValueString>(Path));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), bSuccess);
  Result->SetNumberField(TEXT("checkedOut"), PackageNames.Num());
  Result->SetArrayField(TEXT("assets"), CheckedOutPaths);

  SendAutomationResponse(RequestingSocket, RequestId, bSuccess,
                         bSuccess ? TEXT("Assets checked out successfully")
                                  : TEXT("Checkout failed"),
                         Result,
                         bSuccess ? FString() : TEXT("CHECKOUT_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("source_control_checkout requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 3. SOURCE CONTROL SUBMIT
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleSourceControlSubmit(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("source_control_submit"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("submit"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("source_control_submit payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPaths (array) and assetPath (single string)
  TArray<FString> AssetPaths;
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) &&
      AssetPathsArray && AssetPathsArray->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }
  } else {
    // Try single assetPath
    FString SinglePath;
    if (Payload->TryGetStringField(TEXT("assetPath"), SinglePath) && !SinglePath.IsEmpty()) {
      AssetPaths.Add(SinglePath);
    }
  }

  if (AssetPaths.Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("assetPath (string) or assetPaths (array) required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString Description;
  if (!Payload->TryGetStringField(TEXT("description"), Description) ||
      Description.IsEmpty()) {
    Description = TEXT("Automated submission via MCP Automation Bridge");
  }

  if (!ISourceControlModule::Get().IsEnabled()) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"),
                           TEXT("Source control is not enabled"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Source control disabled"), Result,
                           TEXT("SOURCE_CONTROL_DISABLED"));
    return true;
  }

  ISourceControlProvider &SourceControlProvider =
      ISourceControlModule::Get().GetProvider();

  TArray<FString> PackageNames;
  for (const FString &Path : AssetPaths) {
    if (UEditorAssetLibrary::DoesAssetExist(Path)) {
      FString PackageName = FPackageName::ObjectPathToPackageName(Path);
      PackageNames.Add(PackageName);
    }
  }

  if (PackageNames.Num() == 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("No valid assets found"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("No valid assets"), Result,
                           TEXT("NO_VALID_ASSETS"));
    return true;
  }

  TArray<FString> FilePaths;
  for (const FString &PackageName : PackageNames) {
    FString FilePath;
    if (FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, FilePath, FPackageName::GetAssetPackageExtension())) {
      FilePaths.Add(FilePath);
    }
  }

  TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation =
      ISourceControlOperation::Create<FCheckIn>();
  CheckInOperation->SetDescription(FText::FromString(Description));

  ECommandResult::Type Result =
      SourceControlProvider.Execute(CheckInOperation, FilePaths);
  bool bSuccess = (Result == ECommandResult::Succeeded);

  TSharedPtr<FJsonObject> ResultObj = McpHandlerUtils::CreateResultObject();
  ResultObj->SetBoolField(TEXT("success"), bSuccess);
  ResultObj->SetNumberField(TEXT("submitted"),
                            bSuccess ? PackageNames.Num() : 0);
  ResultObj->SetStringField(TEXT("description"), Description);

  SendAutomationResponse(
      RequestingSocket, RequestId, bSuccess,
      bSuccess ? TEXT("Assets submitted successfully") : TEXT("Submit failed"),
      ResultObj, bSuccess ? FString() : TEXT("SUBMIT_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("source_control_submit requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 4A. SOURCE CONTROL ENABLE
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleSourceControlEnable(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("source_control_enable"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  FString Provider = TEXT("None");
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("provider"), Provider);
  }

  ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
  
  // Check if already enabled
  if (SourceControlModule.IsEnabled()) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("provider"), SourceControlModule.GetProvider().GetName().ToString());
    Result->SetStringField(TEXT("message"), TEXT("Source control already enabled"));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Source control already enabled"), Result, FString());
    return true;
  }

  // Try to set the provider by name
  if (!Provider.IsEmpty() && !Provider.Equals(TEXT("None"), ESearchCase::IgnoreCase)) {
    SourceControlModule.SetProvider(FName(*Provider));
  }
  
  bool bEnabled = SourceControlModule.IsEnabled();
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), bEnabled);
  Result->SetStringField(TEXT("provider"), SourceControlModule.GetProvider().GetName().ToString());
  
  if (bEnabled) {
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Source control enabled"), Result, FString());
  } else {
    Result->SetStringField(TEXT("error"), TEXT("Failed to enable source control. Please configure provider in Editor preferences."));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Source control enable failed"), Result,
                           TEXT("SOURCE_CONTROL_ENABLE_FAILED"));
  }
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("source_control_enable requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// SOURCE CONTROL STATE
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleGetSourceControlState(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("get_source_control_state"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("get_source_control_state payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPath and assetPaths
  TArray<FString> AssetPaths;
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) &&
      AssetPathsArray && AssetPathsArray->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }
  } else {
    FString SinglePath;
    if (Payload->TryGetStringField(TEXT("assetPath"), SinglePath) && !SinglePath.IsEmpty()) {
      AssetPaths.Add(SinglePath);
    }
  }

  if (AssetPaths.Num() == 0) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath (string) or assetPaths (array) required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!ISourceControlModule::Get().IsEnabled()) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("sourceControlEnabled"), false);
    Result->SetStringField(TEXT("message"), TEXT("Source control is not enabled"));
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Source control disabled"), Result, FString());
    return true;
  }

  ISourceControlProvider &SourceControlProvider =
      ISourceControlModule::Get().GetProvider();

  TArray<TSharedPtr<FJsonValue>> StatesArray;

  for (const FString &AssetPath : AssetPaths) {
    TSharedPtr<FJsonObject> StateObj = McpHandlerUtils::CreateResultObject();
    StateObj->SetStringField(TEXT("assetPath"), AssetPath);

    // Check if asset exists
    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
      StateObj->SetBoolField(TEXT("exists"), false);
      StateObj->SetStringField(TEXT("state"), TEXT("not_found"));
      StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
      continue;
    }

    StateObj->SetBoolField(TEXT("exists"), true);

    // Convert asset path to file path
    FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
    FString FilePath;
    if (!FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, FilePath, FPackageName::GetAssetPackageExtension())) {
      StateObj->SetStringField(TEXT("state"), TEXT("path_conversion_failed"));
      StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
      continue;
    }

    // Get source control state
    FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(
        FilePath, EStateCacheUsage::Use);

    if (!SourceControlState.IsValid()) {
      StateObj->SetStringField(TEXT("state"), TEXT("unknown"));
      StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
      continue;
    }

    // Populate state info
    StateObj->SetBoolField(TEXT("isSourceControlled"), SourceControlState->IsSourceControlled());
    StateObj->SetBoolField(TEXT("isCheckedOut"), SourceControlState->IsCheckedOut());
    StateObj->SetBoolField(TEXT("isCurrent"), SourceControlState->IsCurrent());
    StateObj->SetBoolField(TEXT("isAdded"), SourceControlState->IsAdded());
    StateObj->SetBoolField(TEXT("isDeleted"), SourceControlState->IsDeleted());
    StateObj->SetBoolField(TEXT("isModified"), SourceControlState->IsModified());
    StateObj->SetBoolField(TEXT("isIgnored"), SourceControlState->IsIgnored());
    StateObj->SetBoolField(TEXT("isUnknown"), SourceControlState->IsUnknown());
    StateObj->SetBoolField(TEXT("canCheckIn"), SourceControlState->CanCheckIn());
    StateObj->SetBoolField(TEXT("canCheckout"), SourceControlState->CanCheckout());
    StateObj->SetBoolField(TEXT("canRevert"), SourceControlState->CanRevert());
    StateObj->SetBoolField(TEXT("canEdit"), SourceControlState->CanEdit());
    StateObj->SetBoolField(TEXT("canDelete"), SourceControlState->CanDelete());
    StateObj->SetBoolField(TEXT("canAdd"), SourceControlState->CanAdd());
    StateObj->SetBoolField(TEXT("isConflicted"), SourceControlState->IsConflicted());

    // Check if checked out by other
    FString WhoCheckedOut;
    bool bIsCheckedOutOther = SourceControlState->IsCheckedOutOther(&WhoCheckedOut);
    StateObj->SetBoolField(TEXT("isCheckedOutOther"), bIsCheckedOutOther);
    if (bIsCheckedOutOther && !WhoCheckedOut.IsEmpty()) {
      StateObj->SetStringField(TEXT("checkedOutBy"), WhoCheckedOut);
    }

    // Determine primary state string
    FString StateString = TEXT("unknown");
    if (!SourceControlState->IsSourceControlled()) {
      StateString = TEXT("not_controlled");
    } else if (SourceControlState->IsAdded()) {
      StateString = TEXT("added");
    } else if (SourceControlState->IsDeleted()) {
      StateString = TEXT("deleted");
    } else if (SourceControlState->IsConflicted()) {
      StateString = TEXT("conflicted");
    } else if (SourceControlState->IsCheckedOut()) {
      StateString = TEXT("checked_out");
    } else if (SourceControlState->IsModified()) {
      StateString = TEXT("modified");
    } else if (!SourceControlState->IsCurrent()) {
      StateString = TEXT("out_of_date");
    } else {
      StateString = TEXT("current");
    }
    StateObj->SetStringField(TEXT("state"), StateString);

    // Get display name
    StateObj->SetStringField(TEXT("displayName"), SourceControlState->GetDisplayName().ToString());

    StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("sourceControlEnabled"), true);
  Result->SetArrayField(TEXT("states"), StatesArray);
  Result->SetNumberField(TEXT("queriedCount"), AssetPaths.Num());

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Source control state retrieved"), Result, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("get_source_control_state requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
