// McpTool_ManageAsset.cpp — manage_asset tool definition (26 canonical actions per spec section 4)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageAsset : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_asset"); }

	FString GetDescription() const override
	{
		return TEXT("Generic, type-agnostic asset operations: list, search, import, "
			"duplicate, rename, move, delete, dependency analysis, metadata/tags, "
			"thumbnails, LODs, Nanite rebuild, validation, redirector cleanup, "
			"reports, source control, and render target creation.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildAnnotations() const override
	{
		// manage_asset can permanently delete and overwrite content (delete_assets,
		// rename_assets, move_assets, fixup_redirectors, etc.). Mark the tool
		// destructive so MCP clients (e.g. Claude Code) do not auto-approve calls and
		// the user gets the standard Allow once / Allow always / Deny prompt before
		// execution.
		auto Annotations = MakeShared<FJsonObject>();
		Annotations->SetBoolField(TEXT("destructiveHint"), true);
		Annotations->SetBoolField(TEXT("idempotentHint"), false);
		return Annotations;
	}

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		// 26 canonical plural action names per spec section 4. All material- and
		// texture-specific actions live in the dedicated tools (manage_material,
		// manage_texture).
		const TArray<FString> ActionEnum = {
			TEXT("list_assets"),
			TEXT("search_assets"),
			TEXT("assets_exist"),
			TEXT("import_assets"),
			TEXT("duplicate_assets"),
			TEXT("rename_assets"),
			TEXT("move_assets"),
			TEXT("delete_assets"),
			TEXT("create_folders"),
			TEXT("get_assets_dependencies"),
			TEXT("get_assets_graph"),
			TEXT("analyze_assets_graph"),
			TEXT("get_assets_metadata"),
			TEXT("set_assets_metadata"),
			TEXT("set_assets_tags"),
			TEXT("find_assets_by_tag"),
			TEXT("create_thumbnails"),
			TEXT("generate_lods"),
			TEXT("nanite_rebuild_meshes"),
			TEXT("validate_assets"),
			TEXT("fixup_redirectors"),
			TEXT("generate_assets_report"),
			TEXT("source_control_checkout_assets"),
			TEXT("source_control_submit_assets"),
			TEXT("get_assets_source_control_state"),
			TEXT("create_render_targets")
		};
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("subAction"), ActionEnum,
				TEXT("Canonical manage_asset sub-action to perform."))
			.StringEnum(TEXT("action"), ActionEnum,
				TEXT("Compatibility alias for subAction. New callers should use subAction."))

			// Single + batch asset path inputs
			.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.Array(TEXT("assetPaths"),
				TEXT("List of asset paths for batch operations (delete_assets, "
					"rename_assets, move_assets, source_control_submit_assets, etc.)."))

			// list / search filters
			.String(TEXT("directory"), TEXT("Path to a directory."))
			.Array(TEXT("classNames"),
				TEXT("UClass names to filter by (e.g., 'Material', 'StaticMesh') for "
					"list_assets / search_assets."))
			.Array(TEXT("packagePaths"),
				TEXT("Package paths to scope list_assets / search_assets. Empty "
					"means all paths."))
			.Bool(TEXT("recursivePaths"),
				TEXT("When true, list_assets / search_assets descends into "
					"subdirectories of packagePaths."))
			.Bool(TEXT("recursiveClasses"),
				TEXT("When true, list_assets / search_assets includes subclasses of "
					"classNames."))
			.Number(TEXT("limit"),
				TEXT("Maximum number of results to return (paginated list_assets / "
					"search_assets)."))
			.Number(TEXT("offset"),
				TEXT("Skip this many results before returning (paginated list_assets "
					"/ search_assets)."))

			// Import / duplicate / rename / move
			.String(TEXT("sourcePath"), TEXT("Source path for import/move/copy."))
			.String(TEXT("destinationPath"),
				TEXT("Destination path for move/copy."))
			.String(TEXT("newName"), TEXT("New name for renaming."))
			.Bool(TEXT("overwrite"),
				TEXT("Overwrite if the asset/file already exists."))
			.Bool(TEXT("save"),
				TEXT("Save the asset(s) after the operation."))
			.Bool(TEXT("fixupRedirectors"),
				TEXT("After move/rename, run fixup-redirectors to consolidate "
					"references and delete redirector stubs."))
			.String(TEXT("prefix"),
				TEXT("Prefix to prepend to asset names during batch rename."))
			.String(TEXT("suffix"),
				TEXT("Suffix to append to asset names during batch rename."))
			.String(TEXT("searchText"),
				TEXT("Substring to find in asset names during batch rename."))
			.String(TEXT("replaceText"),
				TEXT("Replacement text for searchText during batch rename."))

			// Folder management
			.String(TEXT("directoryPath"), TEXT("Path to a directory."))
			.String(TEXT("folderPath"), TEXT("Path to a directory."))
			.String(TEXT("path"), TEXT("Path to a directory."))
			.String(TEXT("name"), TEXT("Name identifier."))

			// Generate LODs / Nanite
			.String(TEXT("meshPath"), TEXT("Mesh asset path."))
			.Number(TEXT("lodCount"),
				TEXT("Number of LODs to generate via generate_lods."))
			.FreeformObject(TEXT("reductionSettings"),
				TEXT("Per-LOD mesh reduction settings for generate_lods (e.g., "
					"screen size, percent triangles)."))

			// Thumbnails / render targets
			.Number(TEXT("width"),
				TEXT("Width in pixels for create_thumbnails / create_render_targets."))
			.Number(TEXT("height"),
				TEXT("Height in pixels for create_thumbnails / create_render_targets."))
			.String(TEXT("format"),
				TEXT("Format identifier: pixel format (e.g., 'RGBA16f') for "
					"create_render_targets, output format ('png'/'json'/'csv') for "
					"export/report actions."))

			// Tags / metadata
			.String(TEXT("tag"), TEXT("Name of the tag."))
			.Array(TEXT("tags"),
				TEXT("Asset tag list (string array) for set_assets_tags / "
					"find_assets_by_tag. Use 'tag' (singular) for single-tag actions."))
			.FreeformObject(TEXT("metadata"),
				TEXT("Key-value metadata pairs to attach via set_assets_metadata."))
			.String(TEXT("description"),
				TEXT("Free-text description body for set_assets_metadata."))

			// Dependency analysis
			.Number(TEXT("maxDepth"),
				TEXT("Maximum recursion depth for get_assets_dependencies / "
					"analyze_assets_graph. Zero or unset means unlimited."))

			// Source control
			.Array(TEXT("paths"),
				TEXT("List of paths for bulk operations like fixup_redirectors and "
					"source_control_submit_assets."))
			.Bool(TEXT("checkoutFiles"),
				TEXT("When true, automatically check out files from source control "
					"before edit/delete operations."))
			.Bool(TEXT("showConfirmation"),
				TEXT("When true, show modal confirmation dialogs (default false to "
					"keep automation headless)."))

			.Required({TEXT("subAction")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageAsset);
