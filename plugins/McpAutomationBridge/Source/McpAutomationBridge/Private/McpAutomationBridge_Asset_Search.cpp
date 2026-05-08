// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_Asset_Search.cpp
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

bool UMcpAutomationBridgeSubsystem::HandleListAssets(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  // Parse filters
  FString PathFilter;
  FString ClassFilter;
  FString TagFilter;
  FString PathStartsWith;

  const TSharedPtr<FJsonObject> *FilterObj;
  if (Payload->TryGetObjectField(TEXT("filter"), FilterObj) && FilterObj) {
    (*FilterObj)->TryGetStringField(TEXT("path"), PathFilter);
    (*FilterObj)->TryGetStringField(TEXT("class"), ClassFilter);
    (*FilterObj)->TryGetStringField(TEXT("tag"), TagFilter);
    (*FilterObj)->TryGetStringField(TEXT("pathStartsWith"), PathStartsWith);
  } else {
    // Legacy support for direct path/recursive fields
    Payload->TryGetStringField(TEXT("path"), PathFilter);
  }

  // Sanitize PathFilter to remove trailing slash which can break AssetRegistry
  // lookups
  if (PathFilter.Len() > 1 && PathFilter.EndsWith(TEXT("/"))) {
    PathFilter.RemoveAt(PathFilter.Len() - 1);
  }

  bool bRecursive = true;
  Payload->TryGetBoolField(TEXT("recursive"), bRecursive);

  // Parse pagination
  int32 Offset = 0;
  int32 Limit = -1; // -1 means no limit
  Payload->TryGetNumberField(TEXT("offset"), Offset);
  Payload->TryGetNumberField(TEXT("limit"), Limit);
  const TSharedPtr<FJsonObject> *PaginationObj;
  if (Payload->TryGetObjectField(TEXT("pagination"), PaginationObj) &&
      PaginationObj) {
    (*PaginationObj)->TryGetNumberField(TEXT("offset"), Offset);
    (*PaginationObj)->TryGetNumberField(TEXT("limit"), Limit);
  }

  Offset = FMath::Max(0, Offset);

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  FARFilter Filter;
  Filter.bRecursivePaths = bRecursive;
  Filter.bRecursiveClasses = true;

  // Apply path filters
  if (!PathFilter.IsEmpty()) {
    Filter.PackagePaths.Add(FName(*PathFilter));
  } else if (!PathStartsWith.IsEmpty()) {
    // If we have a path prefix, assume it's a package path
    // Note: FARFilter doesn't support 'StartsWith' natively for paths in an
    // efficient way other than adding the path and set bRecursivePaths=true. So
    // if PathStartsWith is a folder, we use it.
    Filter.PackagePaths.Add(FName(*PathStartsWith));
  } else {
    // Default to /Game to prevent empty results or massive scan
    Filter.PackagePaths.Add(FName(TEXT("/Game")));
  }

  // Use cached AssetRegistry data — ScanPathsSynchronous() removed to prevent
  // blocking the GameThread (causes SSE/HTTP transport timeouts).
  // LIMITATION: Assets not yet indexed by the editor's background scanner
  // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.

  if (!ClassFilter.IsEmpty()) {
    // Support both short class names and full paths (best effort)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    FTopLevelAssetPath ClassPath(ClassFilter);
    if (ClassPath.IsValid()) {
      Filter.ClassPaths.Add(ClassPath);
    }
#else
    // UE 5.0: Use ClassNames instead of ClassPaths
    Filter.ClassNames.Add(FName(*ClassFilter));
#endif
  }

  // Tags are not standard on assets in the same way as actors.
  // AssetRegistry tags are Key-Value pairs.
  // If TagFilter is provided, we assume it checks for the existence of a tag
  // key or value. Implementing a generic "HasTag" is ambiguous. We'll assume
  // TagFilter refers to a metadata key presence.

  // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
  // Asset listing uses cached AssetRegistry data exclusively.
  // LIMITATION: Assets not yet indexed by the editor's background scanner
  // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
  TArray<FAssetData> AssetList;
  AssetRegistry.GetAssets(Filter, AssetList);

  // Post-filtering
  if (!ClassFilter.IsEmpty() || !TagFilter.IsEmpty()) {
    AssetList.RemoveAll([&](const FAssetData &Asset) {
      if (!ClassFilter.IsEmpty()) {
        // Check full class path or asset class name
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        FString AssetClass = Asset.AssetClassPath.ToString();
        FString AssetClassName = Asset.AssetClassPath.GetAssetName().ToString();
#else
        FString AssetClass = Asset.AssetClass.ToString();
        FString AssetClassName = Asset.AssetClass.ToString();
#endif
        if (!AssetClass.Equals(ClassFilter) &&
            !AssetClassName.Equals(ClassFilter)) {
          return true; // Remove
        }
      }
      if (!TagFilter.IsEmpty()) {
        if (!Asset.TagsAndValues.Contains(FName(*TagFilter))) {
          return true; // Remove
        }
      }
      return false;
    });
  }

  // Filter by Depth if specified
  // (Changes made to support depth and folders - Touch to force rebuild)
  int32 Depth = -1;
  Payload->TryGetNumberField(TEXT("depth"), Depth);

  if (Depth >= 0 && bRecursive && !PathFilter.IsEmpty()) {
    // Normalize base path for depth calculation
    FString BasePath = PathFilter;
    if (BasePath.EndsWith(TEXT("/"))) {
      BasePath.RemoveAt(BasePath.Len() - 1);
    }
    // Base depth: number of slashes in /Game/Foo is 2
    int32 BaseSlashCount = 0;
    for (const TCHAR *P = *BasePath; *P; ++P) {
      if (*P == TEXT('/'))
        BaseSlashCount++;
    }

    AssetList.RemoveAll([&](const FAssetData &Asset) {
      FString PkgPath = Asset.PackagePath.ToString();
      // If PkgPath is shorter than BasePath (shouldn't happen with filter),
      // keep it I guess? Actually we only care about descendants.

      int32 SlashCount = 0;
      for (const TCHAR *P = *PkgPath; *P; ++P) {
        if (*P == TEXT('/'))
          SlashCount++;
      }

      // Difference in slashes determines depth
      // /Game (1 slash) vs /Game/A (2 slashes) -> Diff 1 -> Depth 0 (immediate
      // child) Wait, PackagePath for /Game/A is /Game. PackagePath for
      // /Game/Sub/B is /Game/Sub.

      // Let's test:
      // Filter: /Game (Slash=1)
      // Asset: /Game/A (PackagePath=/Game, Slash=1). Diff=0. Depth 0? Yes.
      // Asset: /Game/Sub/B (PackagePath=/Game/Sub, Slash=2). Diff=1. Depth 1?
      // Yes.

      // If Depth=0, we want Diff=0.
      // If Depth=1, we want Diff<=1.

      return (SlashCount - BaseSlashCount) > Depth;
    });
  }

  int32 TotalCount = AssetList.Num();

  // Apply pagination
  if (Offset > 0) {
    if (Offset >= AssetList.Num()) {
      AssetList.Empty();
    } else {
      AssetList.RemoveAt(0, Offset);
    }
  }

  if (Limit >= 0 && AssetList.Num() > Limit) {
    AssetList.SetNum(Limit);
  }

  // Also fetch sub-folders if we are listing a directory (PathFilter is set)
  TArray<FString> SubPathList;
  if (!PathFilter.IsEmpty()) {
    // If non-recursive (or depth limited), we generally want at least the
    // immediate subfolders. GetSubPaths is non-recursive by default.
    AssetRegistry.GetSubPaths(PathFilter, SubPathList, false);

    // If Depth is specified, we might want deeper folders?
    // Actually, standard 'ls' behavior on a folder shows immediate children
    // (files and folders). If recursive, it shows everything. Let keeps it
    // simple: If we are listing a path, show its immediate subfolders. Getting
    // ALL recursive folders might be too much info if strictly not requested,
    // but 'GetSubPaths' with bInRecurse=true gets everything.

    // Decision:
    // If Recursive=true (and Depth not limited), maybe we don't strictly need
    // folders as assets cover it? But user asked for folders when assets are
    // missing. Default 'ls' shows immediate folders. So let's always include
    // immediate subfolders of the requested path.
  }

  TArray<TSharedPtr<FJsonValue>> AssetsArray;
  for (const FAssetData &Asset : AssetList) {
    TSharedPtr<FJsonObject> AssetObj = McpHandlerUtils::CreateResultObject();
    AssetObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    AssetObj->SetStringField(TEXT("path"), Asset.GetSoftObjectPath().ToString());
    AssetObj->SetStringField(TEXT("class"), Asset.AssetClassPath.ToString());
#else
    AssetObj->SetStringField(TEXT("path"), Asset.ToSoftObjectPath().ToString());
    AssetObj->SetStringField(TEXT("class"), Asset.AssetClass.ToString());
#endif
    AssetObj->SetStringField(TEXT("packagePath"), Asset.PackagePath.ToString());

    // Add tags for context if requested
    TArray<TSharedPtr<FJsonValue>> Tags;
    for (auto TagPair : Asset.TagsAndValues) {
      Tags.Add(MakeShared<FJsonValueString>(TagPair.Key.ToString()));
    }
    AssetObj->SetArrayField(TEXT("tags"), Tags);

    AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
  }

  TArray<TSharedPtr<FJsonValue>> FoldersJson;
  for (const FString &SubPath : SubPathList) {
    FoldersJson.Add(MakeShared<FJsonValueString>(SubPath));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetArrayField(TEXT("assets"), AssetsArray);
  Resp->SetArrayField(TEXT("folders"), FoldersJson);
  Resp->SetNumberField(TEXT("totalCount"), TotalCount);
  Resp->SetNumberField(TEXT("count"), AssetsArray.Num());
  Resp->SetNumberField(TEXT("offset"), Offset);

  SendAutomationResponse(Socket, RequestId, true, TEXT("Assets listed"), Resp,
                         FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleFindByTag(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("find_by_tag"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("find_by_tag payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Tag;
  if (!Payload->TryGetStringField(TEXT("tag"), Tag) || Tag.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("tag field is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // CRITICAL: Validate path parameter for security even if not used for actor search
  // This prevents false negatives in security testing and follows defense-in-depth
  FString Path;
  if (Payload->TryGetStringField(TEXT("path"), Path) && !Path.IsEmpty()) {
    FString SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("Invalid path (traversal/security violation): %s"), *Path),
          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    // Path is valid - could be used for scoping asset search in future
  }

  FName TagName(*Tag);
  TArray<TSharedPtr<FJsonValue>> Results;
  int32 MaxResults = 100;
  Payload->TryGetNumberField(TEXT("maxResults"), MaxResults);
  MaxResults = FMath::Clamp(MaxResults, 1, 1000);

  bool bSearchActors = true;
  bool bSearchComponents = false;
  bool bSearchAssets = false;
  Payload->TryGetBoolField(TEXT("searchActors"), bSearchActors);
  Payload->TryGetBoolField(TEXT("searchComponents"), bSearchComponents);
  Payload->TryGetBoolField(TEXT("searchAssets"), bSearchAssets);

  // Search in world
  if (GEditor && bSearchActors) {
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (World) {
      for (TActorIterator<AActor> It(World); It && Results.Num() < MaxResults; ++It) {
        AActor *Actor = *It;
        if (Actor && Actor->ActorHasTag(TagName)) {
          TSharedPtr<FJsonObject> ResultObj = McpHandlerUtils::CreateResultObject();
          ResultObj->SetStringField(TEXT("type"), TEXT("Actor"));
          ResultObj->SetStringField(TEXT("name"), Actor->GetName());
          ResultObj->SetStringField(TEXT("label"), Actor->GetActorLabel());
          ResultObj->SetStringField(TEXT("path"), Actor->GetPathName());
          ResultObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
          
          const FVector Location = Actor->GetActorLocation();
          TSharedPtr<FJsonObject> LocObj = McpHandlerUtils::CreateResultObject();
          LocObj->SetNumberField(TEXT("x"), Location.X);
          LocObj->SetNumberField(TEXT("y"), Location.Y);
          LocObj->SetNumberField(TEXT("z"), Location.Z);
          ResultObj->SetObjectField(TEXT("location"), LocObj);
          
          Results.Add(MakeShared<FJsonValueObject>(ResultObj));
        }
      }
    }
  }

  // Search for components with tag
  if (bSearchComponents && GEditor && Results.Num() < MaxResults) {
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (World) {
      for (TActorIterator<AActor> It(World); It && Results.Num() < MaxResults; ++It) {
        AActor *Actor = *It;
        if (Actor) {
          TInlineComponentArray<UActorComponent*> Components;
          Actor->GetComponents(Components);
          for (UActorComponent *Component : Components) {
            if (Component && Component->ComponentHasTag(TagName)) {
              TSharedPtr<FJsonObject> ResultObj = McpHandlerUtils::CreateResultObject();
              ResultObj->SetStringField(TEXT("type"), TEXT("Component"));
              ResultObj->SetStringField(TEXT("name"), Component->GetName());
              ResultObj->SetStringField(TEXT("class"), Component->GetClass()->GetName());
              ResultObj->SetStringField(TEXT("owner"), Actor->GetName());
              ResultObj->SetStringField(TEXT("path"), Component->GetPathName());
              Results.Add(MakeShared<FJsonValueObject>(ResultObj));
            }
          }
        }
      }
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("tag"), Tag);
  Resp->SetNumberField(TEXT("count"), Results.Num());
  Resp->SetArrayField(TEXT("results"), Results);

  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Found %d objects with tag '%s'"), Results.Num(), *Tag),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("find_by_tag requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
