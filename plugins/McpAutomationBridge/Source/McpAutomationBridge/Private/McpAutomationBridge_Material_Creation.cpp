// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_Material_Creation.cpp
//
// Task F.4 - 7 plural-batched creation actions for the manage_material tool.
//
// External handlers exposed by this TU:
//   - create_materials                       (UMaterial)
//   - create_material_instances              (UMaterialInstanceConstant)
//   - create_material_functions              (UMaterialFunction)
//   - create_material_function_instances     (UMaterialFunctionInstance)
//   - create_landscape_materials             (UMaterial, MD_Surface, opaque)
//   - create_decal_materials                 (UMaterial, MD_DeferredDecal, translucent)
//   - create_post_process_materials          (UMaterial, MD_PostProcess, opaque)
//
// All seven actions take {items: [{packagePath, assetName, parentPath?, ...}, ...]}.
// Per-item failures don't abort the batch; each item gets its own row in the
// 'results' array of the final response. Engine content (/Engine/, /EnginePlugins/)
// is rejected per-item with errorCode = "ENGINE_PATH_BLOCKED".
//
// Plan: docs/superpowers/plans/2026-05-07-mcp-material-tools-redesign.md
// Spec: docs/superpowers/specs/2026-05-07-mcp-material-tools-redesign-design.md (sec 5)

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR

// Asset Tools & Registry
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"

// Material Core
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInstance.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialFunctionMaterialLayer.h"
#include "Materials/MaterialFunctionMaterialLayerBlend.h"
#include "Materials/MaterialInstanceConstant.h"

// MaterialDomain enum (UE 5.1+)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "MaterialDomain.h"
#endif

// Factories
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialFunctionFactoryNew.h"
#include "Factories/MaterialFunctionInstanceFactory.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"

// Core
#include "EditorAssetLibrary.h"
#include "Misc/PackageName.h"

#endif // WITH_EDITOR

DEFINE_LOG_CATEGORY_STATIC(LogMcpMaterialCreation, Log, All);

namespace
{
#if WITH_EDITOR

// True for any path under engine content; per-item create is refused for these.
static bool McpCreationIsEngineAsset(const FString& InPath)
{
    return InPath.StartsWith(TEXT("/Engine/")) || InPath.StartsWith(TEXT("/EnginePlugins/"));
}

// Result row scaffold. Every per-item row carries {packagePath, assetName,
// success}; success rows additionally carry {assetPath}; failure rows additionally
// carry {error, errorCode}.
static TSharedPtr<FJsonObject> McpMakeCreationRow(
    const FString& PackagePath, const FString& AssetName, bool bSuccess)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("packagePath"), PackagePath);
    Row->SetStringField(TEXT("assetName"), AssetName);
    Row->SetBoolField(TEXT("success"), bSuccess);
    return Row;
}

// Stamp an error row with {error, errorCode}.
static void McpStampCreationError(
    TSharedPtr<FJsonObject>& Row, const FString& Message, const FString& Code)
{
    if (!Row.IsValid()) return;
    Row->SetBoolField(TEXT("success"), false);
    Row->SetStringField(TEXT("error"), Message);
    Row->SetStringField(TEXT("errorCode"), Code);
}

// Read the per-item {packagePath, assetName} pair, also accepting legacy aliases
// 'path' and 'name'. OutPackagePath / OutAssetName are filled even on validation
// failure (so the result row can echo what the caller sent).
static void McpReadItemPathName(
    const TSharedPtr<FJsonObject>& Item,
    FString& OutPackagePath, FString& OutAssetName)
{
    OutPackagePath.Reset();
    OutAssetName.Reset();
    if (!Item.IsValid()) return;
    if (!Item->TryGetStringField(TEXT("packagePath"), OutPackagePath))
    {
        Item->TryGetStringField(TEXT("path"), OutPackagePath);
    }
    if (!Item->TryGetStringField(TEXT("assetName"), OutAssetName))
    {
        Item->TryGetStringField(TEXT("name"), OutAssetName);
    }
}

// Read the parent asset path for instance-style items, accepting both
// 'parentPath' and the legacy 'parentMaterial'.
static FString McpReadItemParentPath(const TSharedPtr<FJsonObject>& Item)
{
    if (!Item.IsValid()) return FString();
    FString S;
    if (Item->TryGetStringField(TEXT("parentPath"), S) && !S.IsEmpty())
    {
        return S;
    }
    if (Item->TryGetStringField(TEXT("parentMaterial"), S) && !S.IsEmpty())
    {
        return S;
    }
    if (Item->TryGetStringField(TEXT("parentFunction"), S) && !S.IsEmpty())
    {
        return S;
    }
    return FString();
}

// Default per-item save flag is true (matching the legacy single-item create_*
// handlers). Items can opt out with `"save": false`.
static bool McpReadItemSaveFlag(const TSharedPtr<FJsonObject>& Item, bool bDefault = true)
{
    if (!Item.IsValid()) return bDefault;
    bool bSave = bDefault;
    Item->TryGetBoolField(TEXT("save"), bSave);
    return bSave;
}

// Validate the {packagePath, assetName} pair and produce the canonical package
// path on success. On failure, fills OutErrorMessage / OutErrorCode and returns
// false; the caller stamps the row.
static bool McpValidateCreationTarget(
    const FString& InPackagePath, const FString& InAssetName,
    FString& OutValidatedPath, FString& OutSanitizedName,
    FString& OutErrorMessage, FString& OutErrorCode)
{
    if (InAssetName.IsEmpty())
    {
        OutErrorMessage = TEXT("Missing 'assetName'.");
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        return false;
    }

    // Sanitize and verify the asset name has no characters dropped beyond the
    // common underscore normalization. Mirrors the legacy create_material rule.
    OutSanitizedName = SanitizeAssetName(InAssetName);
    {
        const FString NormalizedOriginal = InAssetName.Replace(TEXT("_"), TEXT(""));
        const FString NormalizedSanitized = OutSanitizedName.Replace(TEXT("_"), TEXT(""));
        if (NormalizedSanitized != NormalizedOriginal)
        {
            OutErrorMessage = FString::Printf(
                TEXT("Invalid asset name '%s': contains characters that cannot be used in asset names. Valid name would be: '%s'"),
                *InAssetName, *OutSanitizedName);
            OutErrorCode = TEXT("INVALID_NAME");
            return false;
        }
    }

    if (InPackagePath.IsEmpty())
    {
        OutErrorMessage = TEXT("Missing 'packagePath'.");
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        return false;
    }

    // Engine-asset-block; refuse to create under /Engine/ or /EnginePlugins/.
    if (McpCreationIsEngineAsset(InPackagePath))
    {
        OutErrorMessage = FString::Printf(
            TEXT("Refusing to create asset under engine content: '%s'."),
            *InPackagePath);
        OutErrorCode = TEXT("ENGINE_PATH_BLOCKED");
        return false;
    }

    // Path traversal + normalization.
    FString PathError;
    if (!ValidateAssetCreationPath(InPackagePath, OutSanitizedName, OutValidatedPath, PathError))
    {
        OutErrorMessage = PathError;
        OutErrorCode = TEXT("INVALID_PATH");
        return false;
    }

    if (OutValidatedPath.Contains(TEXT(":")))
    {
        OutErrorMessage = FString::Printf(
            TEXT("Invalid path '%s': absolute Windows paths are not allowed"),
            *OutValidatedPath);
        OutErrorCode = TEXT("INVALID_PATH");
        return false;
    }

    FText MountReason;
    if (!FPackageName::IsValidLongPackageName(OutValidatedPath, true, &MountReason))
    {
        OutErrorMessage = FString::Printf(
            TEXT("Invalid package path '%s': %s"),
            *OutValidatedPath, *MountReason.ToString());
        OutErrorCode = TEXT("INVALID_PATH");
        return false;
    }

    // Re-check engine content after normalization, in case the validator
    // rewrote the path (defense in depth).
    if (McpCreationIsEngineAsset(OutValidatedPath))
    {
        OutErrorMessage = FString::Printf(
            TEXT("Refusing to create asset under engine content: '%s'."),
            *OutValidatedPath);
        OutErrorCode = TEXT("ENGINE_PATH_BLOCKED");
        return false;
    }

    // Existing-asset collision; refuse to overwrite.
    const FString FullAssetPath = OutValidatedPath + TEXT(".") + OutSanitizedName;
    if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath))
    {
        UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(FullAssetPath);
        FString ExistingClassName = TEXT("Unknown");
        if (ExistingAsset && ExistingAsset->GetClass())
        {
            ExistingClassName = ExistingAsset->GetClass()->GetName();
        }
        OutErrorMessage = FString::Printf(
            TEXT("Asset '%s' already exists as %s."),
            *FullAssetPath, *ExistingClassName);
        OutErrorCode = TEXT("ASSET_EXISTS");
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Per-item workers - one per asset kind. Each fills OutRow with the canonical
// {packagePath, assetName, success, ...} fields and returns true on success.
// ---------------------------------------------------------------------------

// Apply the optional {materialDomain, blendMode, shadingModel, twoSided} fields
// onto a freshly-created UMaterial. Returns false (and stamps OutRow) when an
// enum string is unrecognised.
static bool McpApplyMaterialOptions(
    UMaterial* Material, const TSharedPtr<FJsonObject>& Item,
    TSharedPtr<FJsonObject>& OutRow)
{
    if (!Material || !Item.IsValid()) return true;

    FString DomainStr;
    if (Item->TryGetStringField(TEXT("materialDomain"), DomainStr))
    {
        bool bOk = true;
        if      (DomainStr == TEXT("Surface"))       Material->MaterialDomain = MD_Surface;
        else if (DomainStr == TEXT("DeferredDecal")) Material->MaterialDomain = MD_DeferredDecal;
        else if (DomainStr == TEXT("LightFunction")) Material->MaterialDomain = MD_LightFunction;
        else if (DomainStr == TEXT("Volume"))        Material->MaterialDomain = MD_Volume;
        else if (DomainStr == TEXT("PostProcess"))   Material->MaterialDomain = MD_PostProcess;
        else if (DomainStr == TEXT("UI"))            Material->MaterialDomain = MD_UI;
        else                                         bOk = false;
        if (!bOk)
        {
            McpStampCreationError(OutRow,
                FString::Printf(TEXT("Invalid materialDomain '%s'."), *DomainStr),
                TEXT("INVALID_ENUM"));
            return false;
        }
    }

    FString BlendStr;
    if (Item->TryGetStringField(TEXT("blendMode"), BlendStr))
    {
        bool bOk = true;
        if      (BlendStr == TEXT("Opaque"))         Material->BlendMode = BLEND_Opaque;
        else if (BlendStr == TEXT("Masked"))         Material->BlendMode = BLEND_Masked;
        else if (BlendStr == TEXT("Translucent"))    Material->BlendMode = BLEND_Translucent;
        else if (BlendStr == TEXT("Additive"))       Material->BlendMode = BLEND_Additive;
        else if (BlendStr == TEXT("Modulate"))       Material->BlendMode = BLEND_Modulate;
        else if (BlendStr == TEXT("AlphaComposite")) Material->BlendMode = BLEND_AlphaComposite;
        else if (BlendStr == TEXT("AlphaHoldout"))   Material->BlendMode = BLEND_AlphaHoldout;
        else                                         bOk = false;
        if (!bOk)
        {
            McpStampCreationError(OutRow,
                FString::Printf(TEXT("Invalid blendMode '%s'."), *BlendStr),
                TEXT("INVALID_ENUM"));
            return false;
        }
    }

    FString ShadingStr;
    if (Item->TryGetStringField(TEXT("shadingModel"), ShadingStr))
    {
        bool bOk = true;
        if      (ShadingStr == TEXT("Unlit"))             Material->SetShadingModel(MSM_Unlit);
        else if (ShadingStr == TEXT("DefaultLit"))        Material->SetShadingModel(MSM_DefaultLit);
        else if (ShadingStr == TEXT("Subsurface"))        Material->SetShadingModel(MSM_Subsurface);
        else if (ShadingStr == TEXT("SubsurfaceProfile")) Material->SetShadingModel(MSM_SubsurfaceProfile);
        else if (ShadingStr == TEXT("PreintegratedSkin")) Material->SetShadingModel(MSM_PreintegratedSkin);
        else if (ShadingStr == TEXT("ClearCoat"))         Material->SetShadingModel(MSM_ClearCoat);
        else if (ShadingStr == TEXT("Hair"))              Material->SetShadingModel(MSM_Hair);
        else if (ShadingStr == TEXT("Cloth"))             Material->SetShadingModel(MSM_Cloth);
        else if (ShadingStr == TEXT("Eye"))               Material->SetShadingModel(MSM_Eye);
        else if (ShadingStr == TEXT("TwoSidedFoliage"))   Material->SetShadingModel(MSM_TwoSidedFoliage);
        else if (ShadingStr == TEXT("ThinTranslucent"))   Material->SetShadingModel(MSM_ThinTranslucent);
        else                                              bOk = false;
        if (!bOk)
        {
            McpStampCreationError(OutRow,
                FString::Printf(TEXT("Invalid shadingModel '%s'."), *ShadingStr),
                TEXT("INVALID_ENUM"));
            return false;
        }
    }

    bool bTwoSided = false;
    if (Item->TryGetBoolField(TEXT("twoSided"), bTwoSided))
    {
        Material->TwoSided = bTwoSided;
    }

    return true;
}

// Create a new UMaterial under {packagePath/assetName}. Optional per-item fields
// {materialDomain, blendMode, shadingModel, twoSided} are honored when present.
static void McpCreateMaterialItem(
    const TSharedPtr<FJsonObject>& Item, TSharedPtr<FJsonObject>& OutRow)
{
    FString InPath, InName;
    McpReadItemPathName(Item, InPath, InName);
    OutRow = McpMakeCreationRow(InPath, InName, false);

    FString ValidatedPath, SanitizedName, ErrMsg, ErrCode;
    if (!McpValidateCreationTarget(InPath, InName, ValidatedPath, SanitizedName, ErrMsg, ErrCode))
    {
        McpStampCreationError(OutRow, ErrMsg, ErrCode);
        return;
    }

    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UPackage* Package = CreatePackage(*ValidatedPath);
    if (!Package)
    {
        McpStampCreationError(OutRow, TEXT("Failed to create package."), TEXT("PACKAGE_ERROR"));
        return;
    }

    UMaterial* NewMaterial = Cast<UMaterial>(Factory->FactoryCreateNew(
        UMaterial::StaticClass(), Package, FName(*SanitizedName),
        RF_Public | RF_Standalone, nullptr, GWarn));
    if (!NewMaterial)
    {
        McpStampCreationError(OutRow, TEXT("Failed to create material."), TEXT("CREATE_FAILED"));
        return;
    }

    if (!McpApplyMaterialOptions(NewMaterial, Item, OutRow))
    {
        // McpApplyMaterialOptions stamps OutRow on enum-mismatch.
        return;
    }

    NewMaterial->PostEditChange();
    NewMaterial->MarkPackageDirty();

    // Notify asset registry first; required before save in UE 5.7+.
    FAssetRegistryModule::AssetCreated(NewMaterial);

    if (McpReadItemSaveFlag(Item))
    {
        McpSafeAssetSave(NewMaterial);
    }

    OutRow->SetBoolField(TEXT("success"), true);
    OutRow->SetStringField(TEXT("assetPath"), NewMaterial->GetPathName());
}

// Create a new UMaterial with a forced {domain, blendMode}. Used by the three
// specialized variants (landscape / decal / post-process). Per-item options
// (other than the forced ones) are still applied when present.
static void McpCreateMaterialWithFixedDomain(
    const TSharedPtr<FJsonObject>& Item, TSharedPtr<FJsonObject>& OutRow,
    EMaterialDomain ForcedDomain, EBlendMode ForcedBlend)
{
    FString InPath, InName;
    McpReadItemPathName(Item, InPath, InName);
    OutRow = McpMakeCreationRow(InPath, InName, false);

    FString ValidatedPath, SanitizedName, ErrMsg, ErrCode;
    if (!McpValidateCreationTarget(InPath, InName, ValidatedPath, SanitizedName, ErrMsg, ErrCode))
    {
        McpStampCreationError(OutRow, ErrMsg, ErrCode);
        return;
    }

    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UPackage* Package = CreatePackage(*ValidatedPath);
    if (!Package)
    {
        McpStampCreationError(OutRow, TEXT("Failed to create package."), TEXT("PACKAGE_ERROR"));
        return;
    }

    UMaterial* NewMaterial = Cast<UMaterial>(Factory->FactoryCreateNew(
        UMaterial::StaticClass(), Package, FName(*SanitizedName),
        RF_Public | RF_Standalone, nullptr, GWarn));
    if (!NewMaterial)
    {
        McpStampCreationError(OutRow, TEXT("Failed to create material."), TEXT("CREATE_FAILED"));
        return;
    }

    NewMaterial->MaterialDomain = ForcedDomain;
    NewMaterial->BlendMode = ForcedBlend;

    NewMaterial->PostEditChange();
    NewMaterial->MarkPackageDirty();

    FAssetRegistryModule::AssetCreated(NewMaterial);

    if (McpReadItemSaveFlag(Item))
    {
        McpSafeAssetSave(NewMaterial);
    }

    OutRow->SetBoolField(TEXT("success"), true);
    OutRow->SetStringField(TEXT("assetPath"), NewMaterial->GetPathName());
}

// Create a new UMaterialInstanceConstant under {packagePath/assetName} with
// 'parentPath' resolved to a UMaterialInterface.
static void McpCreateMaterialInstanceItem(
    const TSharedPtr<FJsonObject>& Item, TSharedPtr<FJsonObject>& OutRow)
{
    FString InPath, InName;
    McpReadItemPathName(Item, InPath, InName);
    OutRow = McpMakeCreationRow(InPath, InName, false);

    FString ValidatedPath, SanitizedName, ErrMsg, ErrCode;
    if (!McpValidateCreationTarget(InPath, InName, ValidatedPath, SanitizedName, ErrMsg, ErrCode))
    {
        McpStampCreationError(OutRow, ErrMsg, ErrCode);
        return;
    }

    FString ParentPath = McpReadItemParentPath(Item);
    if (ParentPath.IsEmpty())
    {
        McpStampCreationError(OutRow, TEXT("Missing 'parentPath'."), TEXT("INVALID_ARGUMENT"));
        return;
    }
    {
        const FString SanitizedParent = SanitizeProjectRelativePath(ParentPath);
        if (SanitizedParent.IsEmpty())
        {
            McpStampCreationError(OutRow,
                FString::Printf(TEXT("Invalid parentPath '%s': traversal or invalid root."), *ParentPath),
                TEXT("INVALID_PATH"));
            return;
        }
        ParentPath = SanitizedParent;
    }

    UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, *ParentPath);
    if (!Parent)
    {
        McpStampCreationError(OutRow,
            FString::Printf(TEXT("Could not load parent material '%s'."), *ParentPath),
            TEXT("ASSET_NOT_FOUND"));
        return;
    }

    UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
    Factory->InitialParent = Parent;

    UPackage* Package = CreatePackage(*ValidatedPath);
    if (!Package)
    {
        McpStampCreationError(OutRow, TEXT("Failed to create package."), TEXT("PACKAGE_ERROR"));
        return;
    }

    UMaterialInstanceConstant* NewInstance = Cast<UMaterialInstanceConstant>(Factory->FactoryCreateNew(
        UMaterialInstanceConstant::StaticClass(), Package, FName(*SanitizedName),
        RF_Public | RF_Standalone, nullptr, GWarn));
    if (!NewInstance)
    {
        McpStampCreationError(OutRow, TEXT("Failed to create material instance."), TEXT("CREATE_FAILED"));
        return;
    }

    NewInstance->PostEditChange();
    NewInstance->MarkPackageDirty();

    FAssetRegistryModule::AssetCreated(NewInstance);

    if (McpReadItemSaveFlag(Item))
    {
        McpSafeAssetSave(NewInstance);
    }

    OutRow->SetBoolField(TEXT("success"), true);
    OutRow->SetStringField(TEXT("assetPath"), NewInstance->GetPathName());
    OutRow->SetStringField(TEXT("parentPath"), Parent->GetPathName());
}

// Create a new UMaterialFunction under {packagePath/assetName}. Optional per-item
// fields {description, exposeToLibrary} are honored.
static void McpCreateMaterialFunctionItem(
    const TSharedPtr<FJsonObject>& Item, TSharedPtr<FJsonObject>& OutRow)
{
    FString InPath, InName;
    McpReadItemPathName(Item, InPath, InName);
    OutRow = McpMakeCreationRow(InPath, InName, false);

    FString ValidatedPath, SanitizedName, ErrMsg, ErrCode;
    if (!McpValidateCreationTarget(InPath, InName, ValidatedPath, SanitizedName, ErrMsg, ErrCode))
    {
        McpStampCreationError(OutRow, ErrMsg, ErrCode);
        return;
    }

    UMaterialFunctionFactoryNew* Factory = NewObject<UMaterialFunctionFactoryNew>();
    UPackage* Package = CreatePackage(*ValidatedPath);
    if (!Package)
    {
        McpStampCreationError(OutRow, TEXT("Failed to create package."), TEXT("PACKAGE_ERROR"));
        return;
    }

    UMaterialFunction* NewFunc = Cast<UMaterialFunction>(Factory->FactoryCreateNew(
        UMaterialFunction::StaticClass(), Package, FName(*SanitizedName),
        RF_Public | RF_Standalone, nullptr, GWarn));
    if (!NewFunc)
    {
        McpStampCreationError(OutRow, TEXT("Failed to create material function."), TEXT("CREATE_FAILED"));
        return;
    }

    FString Description;
    if (Item.IsValid() && Item->TryGetStringField(TEXT("description"), Description) && !Description.IsEmpty())
    {
        NewFunc->Description = Description;
    }
    bool bExposeToLibrary = true;
    if (Item.IsValid())
    {
        Item->TryGetBoolField(TEXT("exposeToLibrary"), bExposeToLibrary);
    }
    NewFunc->bExposeToLibrary = bExposeToLibrary;

    NewFunc->PostEditChange();
    NewFunc->MarkPackageDirty();

    FAssetRegistryModule::AssetCreated(NewFunc);

    if (McpReadItemSaveFlag(Item))
    {
        McpSafeAssetSave(NewFunc);
    }

    OutRow->SetBoolField(TEXT("success"), true);
    OutRow->SetStringField(TEXT("assetPath"), NewFunc->GetPathName());
}

// Create a new UMaterialFunctionInstance under {packagePath/assetName} with
// 'parentPath' resolved to a UMaterialFunctionInterface. Optional 'instanceKind'
// in {function, materialLayer, materialLayerBlend} selects the factory.
static void McpCreateMaterialFunctionInstanceItem(
    const TSharedPtr<FJsonObject>& Item, TSharedPtr<FJsonObject>& OutRow)
{
    FString InPath, InName;
    McpReadItemPathName(Item, InPath, InName);
    OutRow = McpMakeCreationRow(InPath, InName, false);

    FString ValidatedPath, SanitizedName, ErrMsg, ErrCode;
    if (!McpValidateCreationTarget(InPath, InName, ValidatedPath, SanitizedName, ErrMsg, ErrCode))
    {
        McpStampCreationError(OutRow, ErrMsg, ErrCode);
        return;
    }

    FString ParentPath = McpReadItemParentPath(Item);
    if (ParentPath.IsEmpty())
    {
        McpStampCreationError(OutRow, TEXT("Missing 'parentPath'."), TEXT("INVALID_ARGUMENT"));
        return;
    }
    {
        const FString SanitizedParent = SanitizeProjectRelativePath(ParentPath);
        if (SanitizedParent.IsEmpty())
        {
            McpStampCreationError(OutRow,
                FString::Printf(TEXT("Invalid parentPath '%s': traversal or invalid root."), *ParentPath),
                TEXT("INVALID_PATH"));
            return;
        }
        ParentPath = SanitizedParent;
    }

    UMaterialFunctionInterface* ParentFunc = LoadObject<UMaterialFunctionInterface>(nullptr, *ParentPath);
    if (!ParentFunc)
    {
        McpStampCreationError(OutRow,
            FString::Printf(TEXT("Could not load parent function '%s'."), *ParentPath),
            TEXT("ASSET_NOT_FOUND"));
        return;
    }

    FString InstanceKind = TEXT("function");
    if (Item.IsValid())
    {
        Item->TryGetStringField(TEXT("instanceKind"), InstanceKind);
        if (InstanceKind.IsEmpty()) InstanceKind = TEXT("function");
    }

    if (InstanceKind == TEXT("materialLayer") && !Cast<UMaterialFunctionMaterialLayer>(ParentFunc))
    {
        McpStampCreationError(OutRow,
            TEXT("Parent is not a UMaterialFunctionMaterialLayer for instanceKind=materialLayer."),
            TEXT("UNSUPPORTED_ASSET_TYPE"));
        return;
    }
    if (InstanceKind == TEXT("materialLayerBlend") && !Cast<UMaterialFunctionMaterialLayerBlend>(ParentFunc))
    {
        McpStampCreationError(OutRow,
            TEXT("Parent is not a UMaterialFunctionMaterialLayerBlend for instanceKind=materialLayerBlend."),
            TEXT("UNSUPPORTED_ASSET_TYPE"));
        return;
    }

    // Build the right factory for the selected kind.
    UFactory* Factory = nullptr;
    if (InstanceKind == TEXT("materialLayer"))
    {
        UMaterialFunctionMaterialLayerInstanceFactory* F = NewObject<UMaterialFunctionMaterialLayerInstanceFactory>();
        F->InitialParent = ParentFunc;
        Factory = F;
    }
    else if (InstanceKind == TEXT("materialLayerBlend"))
    {
        UMaterialFunctionMaterialLayerBlendInstanceFactory* F = NewObject<UMaterialFunctionMaterialLayerBlendInstanceFactory>();
        F->InitialParent = ParentFunc;
        Factory = F;
    }
    else
    {
        UMaterialFunctionInstanceFactory* F = NewObject<UMaterialFunctionInstanceFactory>();
        F->InitialParent = ParentFunc;
        Factory = F;
    }

    // CreateAsset routes through AssetTools so it picks the right package
    // path + naming policy and notifies the registry for us.
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    UObject* NewAsset = AssetTools.CreateAsset(SanitizedName, ValidatedPath, nullptr, Factory);
    UMaterialFunctionInstance* NewInstance = Cast<UMaterialFunctionInstance>(NewAsset);
    if (!NewInstance)
    {
        McpStampCreationError(OutRow,
            TEXT("Failed to create material function instance."),
            TEXT("CREATE_FAILED"));
        return;
    }

    NewInstance->UpdateParameterSet();
    NewInstance->MarkPackageDirty();

    if (McpReadItemSaveFlag(Item, /*bDefault=*/false))
    {
        McpSafeAssetSave(NewInstance);
    }

    OutRow->SetBoolField(TEXT("success"), true);
    OutRow->SetStringField(TEXT("assetPath"), NewInstance->GetPathName());
    OutRow->SetStringField(TEXT("parentPath"), ParentFunc->GetPathName());
}

// ---------------------------------------------------------------------------
// Shared driver. Reads payload->items[] and applies the supplied per-item
// worker, building the final {success, results: [...]} response.
// ---------------------------------------------------------------------------

using FMcpCreationItemWorker = TFunction<void(const TSharedPtr<FJsonObject>&, TSharedPtr<FJsonObject>&)>;

static bool McpHandle_CreationCommon(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
    const TCHAR* ActionLabel, const FMcpCreationItemWorker& Worker)
{
    if (!Sub) return true;
    if (!Payload.IsValid())
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("Invalid payload"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* ItemsArr = nullptr;
    if (!Payload->TryGetArrayField(TEXT("items"), ItemsArr) || !ItemsArr || ItemsArr->Num() == 0)
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("'items' is required and must be a non-empty array of item objects."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> Results;
    Results.Reserve(ItemsArr->Num());
    int32 SuccessCount = 0;

    for (const TSharedPtr<FJsonValue>& V : *ItemsArr)
    {
        TSharedPtr<FJsonObject> Item;
        const TSharedPtr<FJsonObject>* ItemPtr = nullptr;
        if (V.IsValid() && V->TryGetObject(ItemPtr) && ItemPtr)
        {
            Item = *ItemPtr;
        }
        // If the array element isn't an object, synthesize an empty item so the
        // worker can produce a proper-shaped error row instead of dropping it.
        if (!Item.IsValid())
        {
            Item = MakeShared<FJsonObject>();
        }

        TSharedPtr<FJsonObject> Row;
        Worker(Item, Row);
        if (!Row.IsValid())
        {
            // Worker should always produce a row; defensive fallback.
            Row = McpMakeCreationRow(TEXT(""), TEXT(""), false);
            McpStampCreationError(Row,
                TEXT("Internal error: worker did not produce a result row."),
                TEXT("INTERNAL_ERROR"));
        }
        bool bRowOk = false;
        Row->TryGetBoolField(TEXT("success"), bRowOk);
        if (bRowOk) ++SuccessCount;
        Results.Add(MakeShared<FJsonValueObject>(Row));
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetArrayField(TEXT("results"), Results);
    Sub->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("%s: %d/%d succeeded."),
            ActionLabel, SuccessCount, ItemsArr->Num()),
        Resp);
    return true;
}

#endif // WITH_EDITOR
} // namespace

// ---------------------------------------------------------------------------
// External handlers (resolved via 'extern bool ...' from the dispatch site).
// ---------------------------------------------------------------------------

bool McpHandle_CreateMaterials(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_CreationCommon(Sub, RequestId, Payload, Socket,
        TEXT("create_materials"), &McpCreateMaterialItem);
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_CreateMaterialInstances(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_CreationCommon(Sub, RequestId, Payload, Socket,
        TEXT("create_material_instances"), &McpCreateMaterialInstanceItem);
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_CreateMaterialFunctions(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_CreationCommon(Sub, RequestId, Payload, Socket,
        TEXT("create_material_functions"), &McpCreateMaterialFunctionItem);
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_CreateMaterialFunctionInstances(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_CreationCommon(Sub, RequestId, Payload, Socket,
        TEXT("create_material_function_instances"), &McpCreateMaterialFunctionInstanceItem);
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_CreateLandscapeMaterials(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_CreationCommon(Sub, RequestId, Payload, Socket,
        TEXT("create_landscape_materials"),
        [](const TSharedPtr<FJsonObject>& Item, TSharedPtr<FJsonObject>& OutRow)
        {
            McpCreateMaterialWithFixedDomain(Item, OutRow, MD_Surface, BLEND_Opaque);
        });
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_CreateDecalMaterials(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_CreationCommon(Sub, RequestId, Payload, Socket,
        TEXT("create_decal_materials"),
        [](const TSharedPtr<FJsonObject>& Item, TSharedPtr<FJsonObject>& OutRow)
        {
            McpCreateMaterialWithFixedDomain(Item, OutRow, MD_DeferredDecal, BLEND_Translucent);
        });
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_CreatePostProcessMaterials(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_CreationCommon(Sub, RequestId, Payload, Socket,
        TEXT("create_post_process_materials"),
        [](const TSharedPtr<FJsonObject>& Item, TSharedPtr<FJsonObject>& OutRow)
        {
            McpCreateMaterialWithFixedDomain(Item, OutRow, MD_PostProcess, BLEND_Opaque);
        });
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}
