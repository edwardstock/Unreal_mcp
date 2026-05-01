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
			.Array(TEXT("classNames"), TEXT(""))
			.Array(TEXT("packagePaths"), TEXT(""))
			.Bool(TEXT("recursivePaths"), TEXT(""))
			.Bool(TEXT("recursiveClasses"), TEXT(""))
			.Number(TEXT("limit"), TEXT(""))
			.Number(TEXT("offset"), TEXT(""))
			.String(TEXT("sourcePath"), TEXT("Source path for import/move/copy."))
			.String(TEXT("destinationPath"), TEXT("Destination path for move/copy."))
			.Array(TEXT("assetPaths"), TEXT(""))
			.Number(TEXT("lodCount"), TEXT(""))
			.FreeformObject(TEXT("reductionSettings"), TEXT(""))
			.String(TEXT("nodeName"), TEXT("Name identifier."))
			.String(TEXT("eventName"), TEXT("Name of the event."))
			.String(TEXT("memberClass"), TEXT(""))
			.Number(TEXT("posX"), TEXT(""))
			.Number(TEXT("posY"), TEXT(""))
			.String(TEXT("newName"), TEXT("New name for renaming."))
			.Bool(TEXT("overwrite"), TEXT("Overwrite if the asset/file already exists."))
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
			.Bool(TEXT("fixupRedirectors"), TEXT(""))
			.String(TEXT("directoryPath"), TEXT("Path to a directory."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("path"), TEXT("Path to a directory."))
			.String(TEXT("parentMaterial"), TEXT("Material asset path."))
			.FreeformObject(TEXT("parameters"), TEXT(""))
			.Number(TEXT("width"), TEXT(""))
			.Number(TEXT("height"), TEXT(""))
			.String(TEXT("format"), TEXT(""))
			.String(TEXT("meshPath"), TEXT("Mesh asset path."))
			.String(TEXT("tag"), TEXT("Name of the tag."))
			.FreeformObject(TEXT("metadata"), TEXT(""))
			.String(TEXT("graphName"), TEXT("Name of the graph."))
			.String(TEXT("nodeType"), TEXT(""))
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
			.Number(TEXT("x"), TEXT(""))
			.Number(TEXT("y"), TEXT(""))
			.String(TEXT("comment"), TEXT(""))
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
			.Number(TEXT("maxDepth"), TEXT(""))
			.String(TEXT("prefix"), TEXT(""))
			.String(TEXT("suffix"), TEXT(""))
			.String(TEXT("searchText"), TEXT(""))
			.String(TEXT("replaceText"), TEXT(""))
			.Array(TEXT("paths"), TEXT(""))
			.String(TEXT("description"), TEXT(""))
			.Bool(TEXT("checkoutFiles"), TEXT(""))
			.Bool(TEXT("showConfirmation"), TEXT(""))
			.String(TEXT("pinName"), TEXT("Name of the pin."))
			.String(TEXT("desc"), TEXT(""))
			.String(TEXT("materialPath"), TEXT("Material asset path."))
			.String(TEXT("texturePath"), TEXT("Texture asset path."))
			.String(TEXT("expressionClass"), TEXT(""))
			.Number(TEXT("coordinateIndex"), TEXT(""))
			.String(TEXT("parameterType"), TEXT(""))
			.ArrayOfObjects(TEXT("nodes"), TEXT(""))
			.Array(TEXT("tags"), TEXT(""))
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
			.String(TEXT("type"), TEXT(""))
			.FreeformObject(TEXT("defaultValue"), TEXT("Generic value (any type)."))
			.Number(TEXT("expressionIndex"), TEXT("Material expression index."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageAsset);
