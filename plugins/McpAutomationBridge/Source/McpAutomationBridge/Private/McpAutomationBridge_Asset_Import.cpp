// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_Asset_Import.cpp
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

bool UMcpAutomationBridgeSubsystem::HandleImportAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString DestinationPath;
  Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);
  FString SourcePath;
  Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);

  if (DestinationPath.IsEmpty() || SourcePath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sourcePath and destinationPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Verify source file exists
  if (!FPaths::FileExists(SourcePath)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Source file not found: %s"), *SourcePath),
        nullptr, TEXT("SOURCE_NOT_FOUND"));
    return true;
  }

  // Sanitize destination path
  FString SafeDestPath = SanitizeProjectRelativePath(DestinationPath);
  if (SafeDestPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid destination path"), nullptr,
                           TEXT("INVALID_PATH"));
    return true;
  }

  FString DestPath = FPaths::GetPath(SafeDestPath);
  FString DestName = FPaths::GetBaseFilename(SafeDestPath);

  // If destination is just a folder, use that
  if (FPaths::GetExtension(SafeDestPath).IsEmpty()) {
    DestPath = SafeDestPath;
    DestName = FPaths::GetBaseFilename(SourcePath);
  }

  // Sanitize DestName: UE asset names cannot contain spaces or dots
  DestName.ReplaceInline(TEXT(" "), TEXT("_"));
  DestName.ReplaceInline(TEXT("."), TEXT("_"));

  // Defer the import to the next tick to avoid TaskGraph recursion issues with
  // UE 5.7+ Interchange Framework. See issue #137.
  // We use SetTimerForNextTick to ensure we're completely outside of any
  // TaskGraph callback chain before invoking the import.
  if (GEditor) {
    TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
    GEditor->GetTimerManager()->SetTimerForNextTick(
        [WeakThis, RequestId, SourcePath, DestPath, DestName, Socket]() {
          UMcpAutomationBridgeSubsystem *StrongThis = WeakThis.Get();
          if (!StrongThis) {
            return;
          }

          IAssetTools &AssetTools =
              FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools")
                  .Get();

          TArray<FString> Files;
          Files.Add(SourcePath);

          UAutomatedAssetImportData *ImportData =
              NewObject<UAutomatedAssetImportData>();
          ImportData->bReplaceExisting = true;
          ImportData->DestinationPath = DestPath;
          ImportData->Filenames = Files;

          TArray<UObject *> ImportedAssets =
              AssetTools.ImportAssetsAutomated(ImportData);

          // Find the first valid (non-null) asset in the array.
          // ImportAssetsAutomated can return arrays with nullptr entries.
          UObject *Asset = nullptr;
          for (UObject *ImportedObj : ImportedAssets) {
            if (ImportedObj) {
              Asset = ImportedObj;
              break;
            }
          }

          if (Asset) {
            // Compute the final asset path. If we rename, use the destination
            // path/name since RenameAssets may invalidate the Asset pointer.
            FString FinalAssetPath;
            bool bRenameSucceeded = true;

            // Rename if needed
            if (Asset->GetName() != DestName) {
              FAssetRenameData RenameData(Asset, DestPath, DestName);
              bRenameSucceeded = AssetTools.RenameAssets({RenameData});
              // After rename, compute path from destination (Asset pointer may
              // be stale)
              FinalAssetPath = DestPath / DestName + TEXT(".") + DestName;
            } else {
              // No rename needed, safe to use the asset's current path
              FinalAssetPath = Asset->GetPathName();
            }

            TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetStringField(TEXT("assetPath"), FinalAssetPath);
            if (!bRenameSucceeded) {
              Resp->SetBoolField(TEXT("renameWarning"), true);
            }
            // Add verification data
            UObject *ImportedAsset = UEditorAssetLibrary::LoadAsset(FinalAssetPath);
            if (ImportedAsset) {
              McpHandlerUtils::AddVerification(Resp, ImportedAsset);
            }
            StrongThis->SendAutomationResponse(
                Socket, RequestId, true,
                bRenameSucceeded ? TEXT("Asset imported")
                                 : TEXT("Asset imported but rename failed"),
                Resp, FString());
          } else {
            StrongThis->SendAutomationResponse(
                Socket, RequestId, false,
                FString::Printf(TEXT("Failed to import asset from '%s'"),
                                *SourcePath),
                nullptr, TEXT("IMPORT_FAILED"));
          }
        });
  } else {
    // Fallback: GEditor not available (shouldn't happen in editor context)
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available for deferred import"),
                           nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
  }

  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGenerateLODs(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("generate_lods"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Support both landscapePath (single) and assetPaths (array)
  FString LandscapePath;
  Payload->TryGetStringField(TEXT("landscapePath"), LandscapePath);
  
  // Support both assetPath (single) and assetPaths (array)
  FString SingleAssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), SingleAssetPath);
  
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray);

  // Support both lodCount and numLODs
  int32 NumLODs = 4;
  Payload->TryGetNumberField(TEXT("lodCount"), NumLODs);
  Payload->TryGetNumberField(TEXT("numLODs"), NumLODs);
  NumLODs = FMath::Clamp(NumLODs, 1, 50);

  // Build list of paths to process
  TArray<FString> Paths;
  
  // Add landscape path if provided
  if (!LandscapePath.IsEmpty()) {
    // Validate landscape path
    FString SafePath = SanitizeProjectRelativePath(LandscapePath);
    if (SafePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Invalid or unsafe landscape path: %s"), *LandscapePath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    Paths.Add(SafePath);
  }
  
  // Add single asset path if provided
  if (!SingleAssetPath.IsEmpty()) {
    FString SafePath = SanitizeProjectRelativePath(SingleAssetPath);
    if (SafePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Invalid or unsafe asset path: %s"), *SingleAssetPath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    Paths.Add(SafePath);
  }
  
  // Add asset paths if provided
  if (AssetPathsArray) {
    for (const auto &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        FString SafePath = SanitizeProjectRelativePath(Val->AsString());
        if (!SafePath.IsEmpty()) {
          Paths.Add(SafePath);
        }
      }
    }
  }

  if (Paths.Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("landscapePath or assetPaths required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // NOTE: ProcessAutomationRequest already dispatches to GameThread.
  // Wrapping ALL work in AsyncTask(GameThread, ...) caused the queued lambda
  // to sit behind the current dispatch cycle, so responses never reached the
  // MCP server before the 30-second timeout. Execute synchronously instead.
  int32 SuccessCount = 0;
  TArray<FString> NotFoundPaths;
  TArray<FString> NotMeshPaths;

  for (const FString &Path : Paths) {
    SendProgressUpdate(RequestId, -1.0f,
        FString::Printf(TEXT("Processing LOD generation for: %s"), *Path), true);

    UObject *Obj = LoadObject<UObject>(nullptr, *Path);

    if (!Obj) {
      NotFoundPaths.Add(Path);
      continue;
    }

    // Try Static Mesh
    if (UStaticMesh *Mesh = Cast<UStaticMesh>(Obj)) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("Generating %d LODs for static mesh %s"), NumLODs, *Path);

        Mesh->Modify();
        Mesh->SetNumSourceModels(NumLODs);

        // Configure LOD reduction settings with progressive reduction
        for (int32 LODIndex = 1; LODIndex < NumLODs; LODIndex++) {
          FStaticMeshSourceModel &SourceModel = Mesh->GetSourceModel(LODIndex);
          FMeshReductionSettings &ReductionSettings =
              SourceModel.ReductionSettings;

          // Progressive reduction: 50%, 25%, 12.5%...
          float ReductionPercent =
              1.0f / FMath::Pow(2.0f, static_cast<float>(LODIndex));
          ReductionSettings.PercentTriangles = ReductionPercent;
          ReductionSettings.PercentVertices = ReductionPercent;

          // Enable reduction for this LOD level
          SourceModel.BuildSettings.bRecomputeNormals = false;
          SourceModel.BuildSettings.bRecomputeTangents = false;
          SourceModel.BuildSettings.bUseMikkTSpace = true;
        }

        // Build the mesh with new LOD settings
        Mesh->Build();
        Mesh->PostEditChange();
        McpSafeAssetSave(Mesh);

        SuccessCount++;
      } else {
        // Asset exists but is not a static mesh
        NotMeshPaths.Add(Path);
      }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    
    // CRITICAL FIX: Return proper success/failure based on actual results
    // Previously always returned success=true even when 0 meshes processed
    bool bSuccess = SuccessCount > 0;
    Resp->SetBoolField(TEXT("success"), bSuccess);
    Resp->SetNumberField(TEXT("processed"), SuccessCount);
    Resp->SetNumberField(TEXT("requested"), Paths.Num());
    Resp->SetNumberField(TEXT("lodCount"), NumLODs);
    
    // Add details about failures
    if (NotFoundPaths.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> NotFoundArray;
      for (const FString& P : NotFoundPaths) {
        NotFoundArray.Add(MakeShared<FJsonValueString>(P));
      }
      Resp->SetArrayField(TEXT("notFoundPaths"), NotFoundArray);
      Resp->SetNumberField(TEXT("notFoundCount"), NotFoundPaths.Num());
    }
    
    if (NotMeshPaths.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> NotMeshArray;
      for (const FString& P : NotMeshPaths) {
        NotMeshArray.Add(MakeShared<FJsonValueString>(P));
      }
      Resp->SetArrayField(TEXT("notMeshPaths"), NotMeshArray);
      Resp->SetNumberField(TEXT("notMeshCount"), NotMeshPaths.Num());
    }
    
    FString Message;
    FString ErrorCode;
    
    if (bSuccess) {
      Message = FString::Printf(TEXT("Generated LODs for %d mesh(es)"), SuccessCount);
    } else if (NotFoundPaths.Num() > 0 && NotMeshPaths.Num() == 0) {
      Message = FString::Printf(TEXT("No assets found. %d path(s) not found."), NotFoundPaths.Num());
      ErrorCode = TEXT("ASSET_NOT_FOUND");
    } else if (NotMeshPaths.Num() > 0 && NotFoundPaths.Num() == 0) {
      Message = FString::Printf(TEXT("No static meshes found. %d asset(s) are not meshes."), NotMeshPaths.Num());
      ErrorCode = TEXT("INVALID_ASSET_TYPE");
    } else {
      Message = FString::Printf(TEXT("No LODs generated. %d not found, %d not meshes."), 
                                NotFoundPaths.Num(), NotMeshPaths.Num());
      ErrorCode = TEXT("LOD_GENERATION_FAILED");
    }
    
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess,
                                      Message, Resp, ErrorCode);

  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Requires editor"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// CREATE RENDER TARGET (action: create_render_targets)
// ============================================================================
// G.2: native handler for the manage_asset.create_render_targets action.
// Mirrors the body of HandleRenderAction's "create_render_target" branch
// (RenderHandlers.cpp) so manage_asset can route directly without dispatching
// through manage_render. The TS-side route adjustment lands in G.3.

#if WITH_EDITOR
#include "Engine/TextureRenderTarget2D.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleCreateRenderTarget(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
        TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Name;
  Payload->TryGetStringField(TEXT("name"), Name);
  if (Name.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
        TEXT("name parameter is required for create_render_targets"),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  int32 Width = 256;
  int32 Height = 256;
  Payload->TryGetNumberField(TEXT("width"), Width);
  Payload->TryGetNumberField(TEXT("height"), Height);

  FString FormatStr;
  Payload->TryGetStringField(TEXT("format"), FormatStr);

  FString PackagePath = TEXT("/Game/RenderTargets");
  Payload->TryGetStringField(TEXT("packagePath"), PackagePath);
  FString PathAlias;
  if (Payload->TryGetStringField(TEXT("path"), PathAlias) && !PathAlias.IsEmpty()) {
    PackagePath = PathAlias;
  }

  if (!DoesAssetDirectoryExistOnDisk(PackagePath)) {
    SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("Parent folder does not exist: %s. Create the folder first or use an existing path."), *PackagePath),
        TEXT("PARENT_FOLDER_NOT_FOUND"));
    return true;
  }

  const FString FullPath = PackagePath / Name;
  if (UEditorAssetLibrary::DoesAssetExist(FullPath)) {
    SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("Asset already exists at path: %s. Delete it first or use a different name."), *FullPath),
        TEXT("ASSET_ALREADY_EXISTS"));
    return true;
  }

  UPackage* Package = CreatePackage(*FullPath);
  UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(
      Package, UTextureRenderTarget2D::StaticClass(), FName(*Name),
      RF_Public | RF_Standalone);
  if (!RT) {
    SendAutomationError(Socket, RequestId,
        TEXT("Failed to create render target asset"), TEXT("CREATE_FAILED"));
    return true;
  }

  EPixelFormat Format = PF_B8G8R8A8;
  if (!FormatStr.IsEmpty()) {
    if (FormatStr.Equals(TEXT("RGBA8"), ESearchCase::IgnoreCase) ||
        FormatStr.Equals(TEXT("BGRA8"), ESearchCase::IgnoreCase)) {
      Format = PF_B8G8R8A8;
    } else if (FormatStr.Equals(TEXT("RGBA16F"), ESearchCase::IgnoreCase) ||
               FormatStr.Equals(TEXT("FloatRGBA"), ESearchCase::IgnoreCase)) {
      Format = PF_FloatRGBA;
    } else if (FormatStr.Equals(TEXT("RGBA32F"), ESearchCase::IgnoreCase)) {
      Format = PF_A32B32G32R32F;
    } else if (FormatStr.Equals(TEXT("R8"), ESearchCase::IgnoreCase)) {
      Format = PF_R8;
    } else if (FormatStr.Equals(TEXT("RG8"), ESearchCase::IgnoreCase)) {
      Format = PF_G8;
    } else if (FormatStr.Equals(TEXT("R16F"), ESearchCase::IgnoreCase)) {
      Format = PF_R16F;
    } else if (FormatStr.Equals(TEXT("R32F"), ESearchCase::IgnoreCase)) {
      Format = PF_R32_FLOAT;
    } else if (FormatStr.Equals(TEXT("A2B10G10R10"), ESearchCase::IgnoreCase)) {
      Format = PF_A2B10G10R10;
    }
    RT->InitCustomFormat(Width, Height, Format, false);
  } else {
    RT->InitAutoFormat(Width, Height);
  }
  RT->UpdateResourceImmediate(true);
  RT->MarkPackageDirty();
  FAssetRegistryModule::AssetCreated(RT);

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("assetPath"), RT->GetPathName());
  Result->SetNumberField(TEXT("width"), Width);
  Result->SetNumberField(TEXT("height"), Height);
  if (!FormatStr.IsEmpty()) {
    Result->SetStringField(TEXT("format"), FormatStr);
  }
  SendAutomationResponse(Socket, RequestId, true,
      TEXT("Render target created successfully"), Result, FString());
  return true;
#else
  SendAutomationError(Socket, RequestId,
      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}
