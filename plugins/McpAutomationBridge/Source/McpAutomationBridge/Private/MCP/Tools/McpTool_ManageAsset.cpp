// McpTool_ManageAsset.cpp — manage_asset tool definition (45 actions)

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
		return TEXT("Create, import, duplicate, rename, delete assets. "
			"Edit Material graphs and instances. Analyze dependencies.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildAnnotations() const override
	{
		// manage_asset can permanently delete and overwrite content (delete, delete_asset,
		// delete_assets, bulk_delete, rename, move, fixup_redirectors, etc.). Mark the tool
		// destructive so MCP clients (e.g. Claude Code) do not auto-approve calls and the
		// user gets the standard Allow once / Allow always / Deny prompt before execution.
		auto Annotations = MakeShared<FJsonObject>();
		Annotations->SetBoolField(TEXT("destructiveHint"), true);
		Annotations->SetBoolField(TEXT("idempotentHint"), false);
		return Annotations;
	}

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("list"),
				TEXT("import"),
				TEXT("duplicate"),
				TEXT("duplicate_asset"),
				TEXT("rename"),
				TEXT("rename_asset"),
				TEXT("move"),
				TEXT("move_asset"),
				TEXT("delete"),
				TEXT("delete_asset"),
				TEXT("delete_assets"),
				TEXT("create_folder"),
				TEXT("search_assets"),
				TEXT("get_dependencies"),
				TEXT("get_source_control_state"),
				TEXT("analyze_graph"),
				TEXT("get_asset_graph"),
				TEXT("create_thumbnail"),
				TEXT("set_tags"),
				TEXT("get_metadata"),
				TEXT("set_metadata"),
				TEXT("validate"),
				TEXT("fixup_redirectors"),
				TEXT("find_by_tag"),
				TEXT("generate_report"),
				TEXT("create_material"),
				TEXT("create_material_instance"),
				TEXT("create_render_target"),
				TEXT("generate_lods"),
				TEXT("add_material_parameter"),
				TEXT("list_instances"),
				TEXT("reset_instance_parameters"),
				TEXT("exists"),
				TEXT("get_material_stats"),
				TEXT("get_material_instance_info"),
				TEXT("find_material_expressions"),
				TEXT("get_material_expression_details"),
				TEXT("bulk_get_material_expression_details"),
				TEXT("get_material_expression_connections"),
				TEXT("get_landscape_material_context"),
				TEXT("compile_material_diagnostics"),
				TEXT("nanite_rebuild_mesh"),
				TEXT("bulk_rename"),
				TEXT("bulk_delete"),
				TEXT("source_control_checkout"),
				TEXT("source_control_submit"),
				TEXT("add_material_node"),
				TEXT("set_material_node_position"),
				TEXT("move_material_node"),
				TEXT("bulk_set_material_node_positions"),
				TEXT("bulk_move_material_nodes"),
				TEXT("connect_material_pins"),
				TEXT("remove_material_node"),
				TEXT("break_material_connections"),
				TEXT("create_material_comment"),
				TEXT("wrap_material_nodes_in_comment"),
				TEXT("create_named_reroute"),
				TEXT("use_named_reroute"),
				TEXT("replace_long_connection_with_named_reroute"),
				TEXT("align_material_nodes"),
				TEXT("get_material_node_details"),
				TEXT("rebuild_material")
			}, TEXT("Action to perform"))
			.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("directory"), TEXT("Path to a directory."))
			.Array(TEXT("classNames"), TEXT("UClass names to filter by (e.g., 'Material', 'StaticMesh') for list / search_assets."))
			.Array(TEXT("packagePaths"), TEXT("Package paths to scope list / search_assets. Empty means all paths."))
			.Bool(TEXT("recursivePaths"), TEXT("When true, list / search_assets descends into subdirectories of packagePaths."))
			.Bool(TEXT("recursiveClasses"), TEXT("When true, list / search_assets includes subclasses of classNames."))
			.Number(TEXT("limit"), TEXT("Maximum number of results to return (paginated list / search_assets)."))
			.Number(TEXT("offset"), TEXT("Skip this many results before returning (paginated list / search_assets)."))
			.String(TEXT("sourcePath"), TEXT("Source path for import/move/copy."))
			.String(TEXT("destinationPath"), TEXT("Destination path for move/copy."))
			.Array(TEXT("assetPaths"), TEXT("List of asset paths for batch operations (delete_assets, bulk_rename, bulk_delete, source_control_submit, etc.)."))
			.Number(TEXT("lodCount"), TEXT("Number of LODs to generate via generate_lods."))
			.FreeformObject(TEXT("reductionSettings"), TEXT("Per-LOD mesh reduction settings for generate_lods (e.g., screen size, percent triangles)."))
			.String(TEXT("nodeName"), TEXT("Name identifier."))
			.String(TEXT("eventName"), TEXT("Name of the event."))
			.String(TEXT("memberClass"), TEXT("Variable/member class name (e.g., 'StaticMeshComponent') when adding parameters or member entries via authoring actions."))
			.Number(TEXT("posX"), TEXT("Material graph X target absolute position for set_material_node_position / move_material_node."))
			.Number(TEXT("posY"), TEXT("Material graph Y target absolute position for set_material_node_position / move_material_node."))
			.String(TEXT("newName"), TEXT("New name for renaming."))
			.Bool(TEXT("overwrite"), TEXT("Overwrite if the asset/file already exists."))
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
			.Bool(TEXT("fixupRedirectors"), TEXT("After move/rename, run fixup-redirectors to consolidate references and delete redirector stubs."))
			.String(TEXT("directoryPath"), TEXT("Path to a directory."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("path"), TEXT("Path to a directory."))
			.String(TEXT("parentMaterial"), TEXT("Material asset path."))
			.FreeformObject(TEXT("parameters"), TEXT("Parameter overrides for create_material_instance: key/value pairs whose value shape matches the parameter type (number for scalar, {r,g,b,a} for vector, asset path for texture, bool for switch)."))
			.Number(TEXT("width"), TEXT("Width in pixels for create_thumbnail / create_render_target."))
			.Number(TEXT("height"), TEXT("Height in pixels for create_thumbnail / create_render_target."))
			.String(TEXT("format"), TEXT("Format identifier: pixel format (e.g., 'RGBA16f') for create_render_target, output format ('png'/'json'/'csv') for export/report actions."))
			.String(TEXT("meshPath"), TEXT("Mesh asset path."))
			.String(TEXT("tag"), TEXT("Name of the tag."))
			.FreeformObject(TEXT("metadata"), TEXT("Key-value metadata pairs to attach via set_metadata."))
			.String(TEXT("graphName"), TEXT("Name of the graph."))
			.String(TEXT("nodeType"), TEXT("Material node type identifier for add_material_node (e.g., 'MaterialExpressionConstant', 'TextureSampleParameter2D')."))
			.String(TEXT("nodeId"), TEXT("ID of the node."))
			.String(TEXT("expressionPath"), TEXT("Material expression path."))
			.String(TEXT("expressionName"), TEXT("Material expression object name."))
			.String(TEXT("expressionGuid"), TEXT("Material expression GUID."))
			.String(TEXT("className"), TEXT("Material expression class name."))
			.Number(TEXT("sourceExpressionIndex"), TEXT("Source expression index."))
			.Number(TEXT("targetExpressionIndex"), TEXT("Target expression index."))
			.String(TEXT("sourceExpressionPath"), TEXT("Source expression path."))
			.String(TEXT("targetExpressionPath"), TEXT("Target expression path."))
			.String(TEXT("anchorExpressionPath"), TEXT("Anchor expression path."))
			.Number(TEXT("anchorExpressionIndex"), TEXT("Anchor expression index."))
			.String(TEXT("anchorNodeId"), TEXT("Anchor node ID."))
			.String(TEXT("sourceNodeId"), TEXT("ID of the source node."))
			.String(TEXT("targetNodeId"), TEXT("ID of the target node."))
			.Number(TEXT("sourceOutputIndex"), TEXT("Source output index."))
			.String(TEXT("inputName"), TEXT("Name of the pin."))
			.String(TEXT("targetInputPin"), TEXT("Name of the target input pin."))
			.String(TEXT("fromNodeId"), TEXT("ID of the source node."))
			.String(TEXT("fromPin"), TEXT("Name of the source pin."))
			.String(TEXT("toNodeId"), TEXT("ID of the target node."))
			.String(TEXT("toPin"), TEXT("Name of the target pin."))
			.String(TEXT("parameterName"), TEXT("Name of the parameter."))
			.FreeformObject(TEXT("value"), TEXT("Generic value (any type)."))
			.Number(TEXT("x"), TEXT("Material graph X position for add_material_node placement (use posX for set_material_node_position / move_material_node)."))
			.Number(TEXT("y"), TEXT("Material graph Y position for add_material_node placement (use posY for set_material_node_position / move_material_node)."))
			.String(TEXT("comment"), TEXT("Comment text for create_material_comment / wrap_material_nodes_in_comment."))
			.String(TEXT("operation"), TEXT("Alignment or distribution operation."))
			.String(TEXT("backend"), TEXT("native or graph_editor."))
			.String(TEXT("placementMode"), TEXT("absolute, next_to, or free."))
			.String(TEXT("direction"), TEXT("right, left, above, or below."))
			.Bool(TEXT("avoidOverlap"), TEXT("Avoid overlapping existing material nodes."))
			.FreeformObject(TEXT("placement"), TEXT("Material node placement options."))
			.Number(TEXT("padding"), TEXT("Padding for comment wrapping."))
			.Bool(TEXT("groupMode"), TEXT("Whether comment moves grouped nodes."))
			.Number(TEXT("minDistance"), TEXT("Minimum distance for long connection replacement."))
			.String(TEXT("declarationId"), TEXT("Named reroute declaration ID."))
			.String(TEXT("declarationGuid"), TEXT("Named reroute declaration GUID."))
			.String(TEXT("declarationName"), TEXT("Named reroute declaration name."))
			.String(TEXT("parentNodeId"), TEXT("ID of the node."))
			.String(TEXT("childNodeId"), TEXT("ID of the node."))
			.Number(TEXT("maxDepth"), TEXT("Maximum recursion depth for get_dependencies / analyze_graph. Zero or unset means unlimited."))
			.String(TEXT("prefix"), TEXT("Prefix to prepend to asset names during bulk_rename."))
			.String(TEXT("suffix"), TEXT("Suffix to append to asset names during bulk_rename."))
			.String(TEXT("searchText"), TEXT("Substring to find in asset names during bulk_rename."))
			.String(TEXT("replaceText"), TEXT("Replacement text for searchText during bulk_rename."))
			.Array(TEXT("paths"), TEXT("List of paths for bulk operations like fixup_redirectors and source_control_submit."))
			.String(TEXT("description"), TEXT("Free-text description body. For set_metadata, the human-readable description value; for material comments, the comment text body. Distinct from the short label 'desc'."))
			.Bool(TEXT("checkoutFiles"), TEXT("When true, automatically check out files from source control before edit/delete operations."))
			.Bool(TEXT("showConfirmation"), TEXT("When true, show modal confirmation dialogs (default false to keep automation headless)."))
			.String(TEXT("pinName"), TEXT("Name of the pin."))
			.String(TEXT("desc"), TEXT("Short label/identifier (e.g., named-reroute display name, report short title). Distinct from the longer free-text 'description'."))
			.String(TEXT("materialPath"), TEXT("Material asset path."))
			.String(TEXT("texturePath"), TEXT("Texture asset path."))
			.String(TEXT("expressionClass"), TEXT("Fully-qualified material expression class name for add_material_node (e.g., 'MaterialExpressionMultiply'). Same role as 'className' on material-graph actions."))
			.Number(TEXT("coordinateIndex"), TEXT("UV coordinate index (0-based) for texture-sample expression nodes."))
			.String(TEXT("parameterType"), TEXT("Parameter type for add_material_parameter: 'scalar', 'vector', 'static_switch', 'texture_object', or 'texture_sample'."))
			.ArrayOfObjects(TEXT("nodes"), TEXT("Batch list of material node specs for bulk_set_material_node_positions / bulk_move_material_nodes / wrap_material_nodes_in_comment."))
			.Array(TEXT("tags"), TEXT("Asset tag list (string array) for set_tags / find_by_tag. Use 'tag' (singular) for single-tag actions."))
			.String(TEXT("actorName"), TEXT("Actor name for landscape context diagnostics."))
			.String(TEXT("actorPath"), TEXT("Actor object path for landscape context diagnostics."))
			.String(TEXT("landscapeName"), TEXT("Landscape actor name."))
			.String(TEXT("landscapePath"), TEXT("Landscape actor object path."))
			.Bool(TEXT("includeEffective"), TEXT("Include effective inherited parameter values."))
			.Bool(TEXT("overriddenOnly"), TEXT("Restrict diagnostics to explicitly overridden parameters."))
			.Bool(TEXT("includeConsumers"), TEXT("Include downstream consumers in expression connection diagnostics."))
			.String(TEXT("folderPath"), TEXT("Path to a directory."))
			.String(TEXT("sourceNode"), TEXT("ID of the source node."))
			.String(TEXT("targetNode"), TEXT("ID of the target node."))
			.String(TEXT("outputPin"), TEXT("Name of the source pin."))
			.String(TEXT("inputPin"), TEXT("Name of the target pin."))
			.String(TEXT("type"), TEXT("Action-specific type discriminator. Prefer the more specific 'parameterType' / 'expressionClass' / 'nodeType' when the action exposes them; this generic 'type' exists only for actions that have not yet been migrated to a specific name."))
			.FreeformObject(TEXT("defaultValue"), TEXT("Generic value (any type)."))
			.Number(TEXT("expressionIndex"), TEXT("Material expression index."))
			.Array(TEXT("indices"), TEXT("Indices of expressions to fetch details for in one round-trip. Provide exactly one of indices, guids, or nodeIds."))
			.Array(TEXT("guids"), TEXT("Expression GUIDs to fetch details for in one round-trip. Provide exactly one of indices, guids, or nodeIds."))
			.Array(TEXT("nodeIds"), TEXT("Node IDs to fetch details for in one round-trip. Provide exactly one of indices, guids, or nodeIds."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageAsset);
