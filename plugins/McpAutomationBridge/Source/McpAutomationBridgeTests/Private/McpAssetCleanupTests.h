#pragma once

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "Factories/MaterialFactoryNew.h"
#include "FileHelpers.h"
#include "HAL/PlatformFileManager.h"
#include "IAssetTools.h"
#include "Materials/Material.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "Framework/Application/SlateApplication.h"

// Smoke tests covering the manage_asset cleanup ergonomics that broke task 7.7
// of the OpenSpec change `complete-material-authoring-mcp`. See
// openspec/changes/complete-material-authoring-mcp/SMOKE_BUGS.md for the bug
// list these tests guard against.

// ---------------------------------------------------------------------------
// Bug 2 regression: HandleCreateFolder must verify folder existence with
// DoesDirectoryExist, not DoesAssetExist. Locks in the engine semantics our
// VerifyDirectoryExists helper relies on.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpAssetCleanupCreateFolderExistsAfterTest,
    "LHGame.Mcp.AssetCleanup.CreateFolderExistsAfter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpAssetCleanupCreateFolderExistsAfterTest::RunTest(const FString& Parameters)
{
    const FString FolderPath = TEXT("/Game/__McpTest_AssetCleanup_CreateFolder__");

    if (UEditorAssetLibrary::DoesDirectoryExist(FolderPath))
    {
        UEditorAssetLibrary::DeleteDirectory(FolderPath);
    }

    const bool bMade = UEditorAssetLibrary::MakeDirectory(FolderPath);
    TestTrue(TEXT("MakeDirectory succeeded"), bMade);

    TestTrue(TEXT("DoesDirectoryExist returns true for created folder"),
        UEditorAssetLibrary::DoesDirectoryExist(FolderPath));

    // Important!
    // The original bug was that HandleCreateFolder used VerifyAssetExists, which calls
    // DoesAssetExist - and that returns false for a folder. The fix uses DoesDirectoryExist.
    // This assertion locks in the engine contract our fix depends on.
    TestFalse(TEXT("DoesAssetExist returns false for content-browser folder"),
        UEditorAssetLibrary::DoesAssetExist(FolderPath));

    UEditorAssetLibrary::DeleteDirectory(FolderPath);
    return true;
}

// ---------------------------------------------------------------------------
// Bug 1 regression: deletion of folders containing safe assets must run fully
// non-interactively. Drives the same code path the smoke run hit (a temporary
// folder containing a UMaterial) and asserts no modal Slate window appears.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpAssetCleanupHeadlessFolderDeleteTest,
    "LHGame.Mcp.AssetCleanup.HeadlessFolderDelete",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpAssetCleanupHeadlessFolderDeleteTest::RunTest(const FString& Parameters)
{
    const FString FolderPath = TEXT("/Game/__McpTest_AssetCleanup_HeadlessDelete__");
    const FString AssetName = TEXT("M_McpTest_HeadlessDelete");

    if (UEditorAssetLibrary::DoesDirectoryExist(FolderPath))
    {
        UEditorAssetLibrary::DeleteDirectory(FolderPath);
    }
    UEditorAssetLibrary::MakeDirectory(FolderPath);

    IAssetTools& AT = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UObject* CreatedAsset = AT.CreateAsset(AssetName, FolderPath, UMaterial::StaticClass(), Factory);
    TestNotNull(TEXT("Material created"), CreatedAsset);
    if (!CreatedAsset)
    {
        return false;
    }

    FEditorFileUtils::SaveDirtyPackages(false, true, true, false, false, false);

    // Snapshot any modal window count before delete; ObjectTools::DeleteObjects with
    // bShowConfirmation=false must not increase it.
    int32 ModalWindowsBefore = 0;
    if (FSlateApplication::IsInitialized())
    {
        ModalWindowsBefore = FSlateApplication::Get().GetActiveModalWindow().IsValid() ? 1 : 0;
    }

    TArray<UObject*> ToDelete;
    ToDelete.Add(CreatedAsset);
    const int32 Deleted = ObjectTools::DeleteObjects(ToDelete, /*bShowConfirmation=*/ false);

    int32 ModalWindowsAfter = 0;
    if (FSlateApplication::IsInitialized())
    {
        ModalWindowsAfter = FSlateApplication::Get().GetActiveModalWindow().IsValid() ? 1 : 0;
    }

    TestEqual(TEXT("Asset deleted via non-interactive ObjectTools"), Deleted, 1);
    TestEqual(TEXT("No new modal window opened during delete"),
        ModalWindowsAfter, ModalWindowsBefore);

    // Cleanup remaining empty folder via registry+filesystem (no UI).
    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    AssetRegistry.RemovePath(FolderPath);

    FString LocalPath;
    if (FPackageName::TryConvertLongPackageNameToFilename(FolderPath, LocalPath))
    {
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        if (PlatformFile.DirectoryExists(*LocalPath))
        {
            PlatformFile.DeleteDirectoryRecursively(*LocalPath);
        }
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
