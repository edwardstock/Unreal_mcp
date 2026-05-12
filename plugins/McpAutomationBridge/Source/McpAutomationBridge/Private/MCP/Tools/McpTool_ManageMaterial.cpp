// McpTool_ManageMaterial.cpp — manage_material tool definition (55 subActions per spec section 5)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageMaterial : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_material"); }

	FString GetDescription() const override
	{
		return TEXT("Consolidated material authoring tool. Create materials, instances, "
			"functions, function instances, landscape/decal/post-process variants. "
			"Edit material graphs (nodes, connections, comments, named reroutes). "
			"Read and set material/function instance parameters with typed batch setters. "
			"Author function inputs/outputs and custom HLSL expressions. Compile and "
			"diagnose materials.");
	}

	FString GetCategory() const override { return TEXT("authoring"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		// 55 subActions per spec section 5. Total breakdown:
		//   Creation (7) + Top-level material properties (5) + Graph reads (4)
		// + Graph writes (5) + Visual/layout (4) + Named reroutes (3)
		// + Material instance parameters (7) + Material function instance parameters (7)
		// + Function authoring (4) + Function calls (2) + Custom expression (2)
		// + Landscape (3) + Compile (2) = 55
		const TArray<FString> ManageMaterialActions = {
			// Creation (7)
			TEXT("create_materials"),
			TEXT("create_material_instances"),
			TEXT("create_material_functions"),
			TEXT("create_material_function_instances"),
			TEXT("create_landscape_materials"),
			TEXT("create_decal_materials"),
			TEXT("create_post_process_materials"),

			// Top-level material properties (5)
			TEXT("set_blend_modes"),
			TEXT("set_shading_models"),
			TEXT("set_material_domains"),
			TEXT("set_material_attributes_modes"),
			TEXT("set_two_sided_flags"),

			// Graph reads (4)
			TEXT("find_material_expressions"),
			TEXT("get_materials_info"),
			TEXT("get_material_stats"),
			TEXT("list_material_expression_classes"),

			// Graph writes (5)
			TEXT("add_material_nodes"),
			TEXT("update_material_nodes"),
			TEXT("remove_material_nodes"),
			TEXT("connect_material_pins"),
			TEXT("break_material_connections"),

			// Visual/layout (4)
			TEXT("set_material_node_positions"),
			TEXT("align_material_nodes"),
			TEXT("create_material_comments"),
			TEXT("wrap_material_nodes_in_comments"),

			// Named reroutes (3)
			TEXT("create_named_reroutes"),
			TEXT("use_named_reroutes"),
			TEXT("replace_long_connections_with_named_reroutes"),

			// Material instance parameters (7)
			TEXT("get_material_instance_parameters"),
			TEXT("reset_material_instance_parameters"),
			TEXT("clear_material_instance_parameters"),
			TEXT("set_material_instance_scalar_parameters"),
			TEXT("set_material_instance_vector_parameters"),
			TEXT("set_material_instance_texture_parameters"),
			TEXT("set_material_instance_static_switch_parameters"),

			// Material function instance parameters (7)
			TEXT("get_material_function_instance_parameters"),
			TEXT("reset_material_function_instance_parameters"),
			TEXT("clear_material_function_instance_parameters"),
			TEXT("set_material_function_instance_scalar_parameters"),
			TEXT("set_material_function_instance_vector_parameters"),
			TEXT("set_material_function_instance_texture_parameters"),
			TEXT("set_material_function_instance_static_switch_parameters"),

			// Function authoring (4)
			TEXT("add_function_inputs"),
			TEXT("add_function_outputs"),
			TEXT("update_function_inputs"),
			TEXT("update_function_outputs"),

			// Function calls (2)
			TEXT("add_material_function_calls"),
			TEXT("update_material_function_calls"),

			// Custom expression (2)
			TEXT("add_custom_expressions"),
			TEXT("update_custom_expressions"),

			// Landscape (3)
			TEXT("add_landscape_layers"),
			TEXT("configure_landscape_layer_blends"),
			TEXT("get_landscape_material_context"),

			// Compile (2)
			TEXT("compile_materials"),
			TEXT("compile_materials_diagnostics")
		};

		return FMcpSchemaBuilder()
			.StringEnum(TEXT("subAction"), ManageMaterialActions,
				TEXT("Canonical manage_material sub-action to perform."))

			// Standard meta
			.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.Array(TEXT("assetPaths"),
				TEXT("List of asset paths for batch operations."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("path"), TEXT("Directory path for asset creation."))
			.String(TEXT("parentPath"),
				TEXT("Parent material or material function interface path."))
			.String(TEXT("parentMaterial"),
				TEXT("Path to parent material for material instances."))
			.String(TEXT("functionPath"), TEXT("Path to function asset."))
			.String(TEXT("localId"),
				TEXT("Caller-supplied local node id used to refer to a node within a "
					"single batch payload (resolved against materialRootSentinel for "
					"top-level pins)."))
			.Number(TEXT("x"), TEXT("Node X position."))
			.Number(TEXT("y"), TEXT("Node Y position."))
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))

			// Top-level material properties
			.StringEnum(TEXT("materialDomain"), {
				TEXT("Surface"),
				TEXT("DeferredDecal"),
				TEXT("LightFunction"),
				TEXT("Volume"),
				TEXT("PostProcess"),
				TEXT("UI")
			}, TEXT("Material domain type."))
			.StringEnum(TEXT("blendMode"), {
				TEXT("Opaque"),
				TEXT("Masked"),
				TEXT("Translucent"),
				TEXT("Additive"),
				TEXT("Modulate"),
				TEXT("AlphaComposite"),
				TEXT("AlphaHoldout")
			}, TEXT("Blend mode."))
			.StringEnum(TEXT("shadingModel"), {
				TEXT("DefaultLit"),
				TEXT("Unlit"),
				TEXT("Subsurface"),
				TEXT("SubsurfaceProfile"),
				TEXT("PreintegratedSkin"),
				TEXT("ClearCoat"),
				TEXT("Hair"),
				TEXT("Cloth"),
				TEXT("Eye"),
				TEXT("TwoSidedFoliage"),
				TEXT("ThinTranslucent")
			}, TEXT("Shading model."))
			.Bool(TEXT("twoSided"), TEXT("Enable two-sided rendering."))

			// Batch payload arrays (the new generation of plural actions accepts
			// these batch arrays; legacy singular fields are still emitted as a
			// fall-through for older payload shapes).
			.ArrayOfObjects(TEXT("nodes"),
				TEXT("Batch list of material node specs. Each item: "
					"{ localId, nodeType|expressionClass, x, y, desc, ... }."))
			.ArrayOfObjects(TEXT("connections"),
				TEXT("Batch list of pin connections for add_material_nodes, "
					"update_material_nodes, and connect_material_pins. Each item: "
					"{ fromNode, fromPin, fromOutputIndex, toNode, toPin }. "
					"connect_material_pins is batch-only and requires this array."))
			.ArrayOfObjects(TEXT("updates"),
				TEXT("Batch list of node-update specs for update_material_nodes and "
					"related update actions. update_material_nodes may also include "
					"top-level connections[] to reconnect pins in the same validated batch."))
			.ArrayOfObjects(TEXT("identifiers"),
				TEXT("Batch list of mixed-type expression identifiers (number index, "
					"string nodeId/guid, or {expressionPath, expressionIndex}). "
					"Used by find/get/remove and reset/clear parameter actions."))
			.ArrayOfObjects(TEXT("items"),
				TEXT("Generic batch items array used by plural actions whose payload "
					"shape varies per sub-action."))
			.ArrayOfObjects(TEXT("calls"),
				TEXT("Batch list of material function call specs for "
					"add_material_function_calls. Update operations use "
					"the unified `updates` field per spec section 7."))
			.ArrayOfObjects(TEXT("inputs"),
				TEXT("Batch list of function input or custom-expression input specs."))
			.ArrayOfObjects(TEXT("outputs"),
				TEXT("Batch list of function output or custom-expression additional "
					"output specs."))

			// Find / discovery filters
			.String(TEXT("filter"),
				TEXT("Free-text filter string for find_material_expressions / "
					"list_material_expression_classes."))
			.String(TEXT("category"),
				TEXT("Category name filter (list_material_expression_classes)."))
			.String(TEXT("parameterName"), TEXT("Name of the parameter."))
			.String(TEXT("parameterGroup"),
				TEXT("Parameter group name filter."))
			.StringEnum(TEXT("samplerType"), {
				TEXT("Color"),
				TEXT("LinearColor"),
				TEXT("Normal"),
				TEXT("Masks"),
				TEXT("Alpha"),
				TEXT("VirtualColor"),
				TEXT("VirtualNormal")
			}, TEXT("Texture sampler type."))
			.String(TEXT("referencesTexture"),
				TEXT("Find expressions that reference this texture path."))
			.String(TEXT("expressionName"),
				TEXT("Material expression object name filter."))
			.String(TEXT("desc"),
				TEXT("Short label/identifier for nodes, comments, named reroutes."))
			.Number(TEXT("limit"),
				TEXT("Maximum number of results (paginated find/list)."))
			.Number(TEXT("offset"),
				TEXT("Skip this many results before returning (paginated)."))
			.Bool(TEXT("includeDetails"),
				TEXT("Include detailed expression properties in find results."))
			.Bool(TEXT("includeConnections"),
				TEXT("Include pin connections in get_materials_info / "
					"find_material_expressions output."))
			.Bool(TEXT("includeConsumers"),
				TEXT("Include downstream consumers in expression diagnostics."))
			.Bool(TEXT("isOrphan"),
				TEXT("Filter to expressions that have no graph consumers."))

			// Per-action discriminators
			.String(TEXT("nodeType"),
				TEXT("Material expression node type or class name."))
			.String(TEXT("expressionClass"),
				TEXT("Fully-qualified material expression class name."))
			.String(TEXT("nodeId"), TEXT("ID of the node."))
			.String(TEXT("pinName"), TEXT("Name of the pin."))
			.String(TEXT("texturePath"), TEXT("Texture asset path."))

			// Custom expression authoring
			.String(TEXT("code"),
				TEXT("HLSL source code for add_custom_expressions / "
					"update_custom_expressions."))
			.Array(TEXT("additionalDefines"),
				TEXT("Custom expression #define entries."))
			.Array(TEXT("includeFilePaths"),
				TEXT("Custom expression #include file paths."))
			.StringEnum(TEXT("outputType"), {
				TEXT("Float1"),
				TEXT("Float2"),
				TEXT("Float3"),
				TEXT("Float4"),
				TEXT("MaterialAttributes")
			}, TEXT("Output type for custom expression / function output."))

			// Update node behavior
			.StringEnum(TEXT("onPinRemoved"), {
				TEXT("break"),
				TEXT("preserve")
			}, TEXT("Behavior when an update removes a pin: 'break' to drop existing "
				"connections, 'preserve' to keep them where possible."))

			// Material attributes mode
			.StringEnum(TEXT("worldPositionShaderOffset"), {
				TEXT("Default"),
				TEXT("ExcludeFromMain")
			}, TEXT("World-position-offset shader behavior toggle."))

			// Comment / reroute layout
			.String(TEXT("comment"),
				TEXT("Comment text body for create_material_comments / "
					"wrap_material_nodes_in_comments."))
			.Number(TEXT("padding"),
				TEXT("Padding for comment wrapping."))
			.Number(TEXT("minDistance"),
				TEXT("Minimum connection distance for "
					"replace_long_connections_with_named_reroutes."))
			.String(TEXT("operation"),
				TEXT("Alignment / distribution operation for align_material_nodes."))

			// Polymorphic value used by typed setters and defaults
			.FreeformObject(TEXT("value"),
				TEXT("Polymorphic value: number for scalar setters, "
					"{r,g,b,a} object for vector setters, asset path string for "
					"texture setters, bool for static-switch setters."))
			.FreeformObject(TEXT("defaultValue"),
				TEXT("Default value for parameter / function input."))
			.FreeformObject(TEXT("previewValue"),
				TEXT("Preview value for material function input authoring."))
			.FreeformObject(TEXT("parameter"),
				TEXT("Material parameter identity: name, type, association, index."))
			.FreeformObject(TEXT("channelNames"),
				TEXT("Vector parameter channel names. Available keys: r, g, b, a."))

			// Landscape
			.String(TEXT("layerName"), TEXT("Name of the landscape layer."))
			.StringEnum(TEXT("blendType"), {
				TEXT("LB_WeightBlend"),
				TEXT("LB_AlphaBlend"),
				TEXT("LB_HeightBlend")
			}, TEXT("Landscape layer blend type."))
			.ArrayOfObjects(TEXT("layers"),
				TEXT("Array of landscape layer configurations."))
			.String(TEXT("actorName"),
				TEXT("Actor name for landscape context diagnostics."))
			.String(TEXT("actorPath"),
				TEXT("Actor object path for landscape context diagnostics."))
			.String(TEXT("landscapeName"), TEXT("Landscape actor name."))
			.String(TEXT("landscapePath"),
				TEXT("Landscape actor object path."))

			// Compile
			.Number(TEXT("timeoutSeconds"),
				TEXT("Compile poll timeout in seconds for compile_materials / "
					"compile_materials_diagnostics."))

			// Informational response-only sentinel (documented here for the schema
			// audience; clients usually do not send it on input).
			.String(TEXT("materialRootSentinel"),
				TEXT("Informational sentinel localId returned in batch responses to "
					"identify the material's root pin set when resolving connections "
					"that target main material pins."))

			.Required({TEXT("subAction")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageMaterial);
