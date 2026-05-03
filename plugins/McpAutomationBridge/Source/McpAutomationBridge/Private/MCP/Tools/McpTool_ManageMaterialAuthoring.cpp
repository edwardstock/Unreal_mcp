// McpTool_ManageMaterialAuthoring.cpp — manage_material_authoring tool definition (38 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageMaterialAuthoring : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_material_authoring"); }

	FString GetDescription() const override
	{
		return TEXT("Create materials with expressions, parameters, functions, "
			"instances, and landscape blend layers.");
	}

	FString GetCategory() const override { return TEXT("authoring"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		const TArray<FString> AuthoringActions = {
				TEXT("create_material"),
				TEXT("set_blend_mode"),
				TEXT("set_shading_model"),
				TEXT("set_material_domain"),
				TEXT("add_texture_sample"),
				TEXT("add_texture_coordinate"),
				TEXT("add_scalar_parameter"),
				TEXT("add_vector_parameter"),
				TEXT("add_static_switch_parameter"),
				TEXT("add_math_node"),
				TEXT("add_world_position"),
				TEXT("add_vertex_normal"),
				TEXT("add_pixel_depth"),
				TEXT("add_fresnel"),
				TEXT("add_reflection_vector"),
				TEXT("add_panner"),
				TEXT("add_rotator"),
				TEXT("add_noise"),
				TEXT("add_voronoi"),
				TEXT("add_if"),
				TEXT("add_switch"),
				TEXT("add_custom_expression"),
				TEXT("connect_nodes"),
				TEXT("disconnect_nodes"),
				TEXT("create_material_function"),
				TEXT("add_function_input"),
				TEXT("add_function_output"),
				TEXT("use_material_function"),
				TEXT("create_material_instance"),
				TEXT("set_scalar_parameter_value"),
				TEXT("set_vector_parameter_value"),
				TEXT("set_texture_parameter_value"),
				TEXT("create_landscape_material"),
				TEXT("create_decal_material"),
				TEXT("create_post_process_material"),
				TEXT("add_landscape_layer"),
				TEXT("configure_layer_blend"),
				TEXT("compile_material"),
				TEXT("get_material_info"),
				TEXT("add_material_node"),
				TEXT("update_function_input"),
				TEXT("update_function_output"),
				TEXT("add_material_function_call"),
				TEXT("update_material_function_call"),
				TEXT("add_texture_object"),
				TEXT("add_texture_object_parameter"),
				TEXT("add_texture_sample_parameter"),
				TEXT("disconnect_input_pin"),
				TEXT("remove_material_node"),
				TEXT("move_material_node"),
				TEXT("set_material_attributes_mode"),
				TEXT("get_material_instance_info"),
				TEXT("set_material_instance_parent"),
				TEXT("get_material_instance_parameters"),
				TEXT("set_material_instance_parameter"),
				TEXT("reset_material_instance_parameter"),
				TEXT("clear_material_instance_parameters"),
				TEXT("bulk_set_material_instance_parameters"),
				TEXT("create_material_function_instance"),
				TEXT("get_material_function_instance_info"),
				TEXT("set_material_function_instance_parent"),
				TEXT("get_material_function_instance_parameters"),
				TEXT("set_material_function_instance_parameter"),
				TEXT("reset_material_function_instance_parameter"),
				TEXT("clear_material_function_instance_parameters"),
				TEXT("bulk_set_material_function_instance_parameters")
			};

		return FMcpSchemaBuilder()
			.StringEnum(TEXT("subAction"), AuthoringActions, TEXT("Canonical material authoring sub-action to perform."))
			.StringEnum(TEXT("action"), AuthoringActions, TEXT("Compatibility alias for subAction. New callers should use subAction."))
			.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("path"), TEXT("Directory path for asset creation."))
			.String(TEXT("parentPath"), TEXT("Parent material or material function interface path."))
			.String(TEXT("parentMaterial"), TEXT("Compatibility parent material path for material instances."))
			.String(TEXT("nodeType"), TEXT("Material expression node type or class name."))
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
			.Number(TEXT("x"), TEXT("Node X position."))
			.Number(TEXT("y"), TEXT("Node Y position."))
			.String(TEXT("texturePath"), TEXT("Texture asset path."))
			.String(TEXT("runtimeVirtualTexturePath"), TEXT("Runtime virtual texture asset path."))
			.String(TEXT("sparseVolumeTexturePath"), TEXT("Sparse volume texture asset path."))
			.String(TEXT("fontPath"), TEXT("Font asset path."))
			.Integer(TEXT("fontPage"), TEXT("Font page index."))
			.StringEnum(TEXT("samplerType"), {
				TEXT("Color"),
				TEXT("LinearColor"),
				TEXT("Normal"),
				TEXT("Masks"),
				TEXT("Alpha"),
				TEXT("VirtualColor"),
				TEXT("VirtualNormal")
			}, TEXT("Texture sampler type."))
			.Number(TEXT("coordinateIndex"), TEXT("UV channel index (0-7)."))
			.Number(TEXT("uTiling"), TEXT("U tiling factor."))
			.Number(TEXT("vTiling"), TEXT("V tiling factor."))
			.String(TEXT("parameterName"), TEXT("Name of the parameter."))
			.FreeformObject(TEXT("channelNames"),
				TEXT("Vector parameter channel names. Available keys: r, g, b, a."))
			.FreeformObject(TEXT("parameter"), TEXT("Material parameter identity: name, type, association, and index."))
			.FreeformObject(TEXT("defaultValue"),
				TEXT("Default value for parameter (number for scalar, object for vector, bool for switch)."))
			.FreeformObject(TEXT("previewValue"), TEXT("Preview value for material function input authoring."))
			.String(TEXT("group"), TEXT("Group name."))
			.Integer(TEXT("sortPriority"), TEXT("Sort priority for function inputs and parameters."))
			.Bool(TEXT("usePreviewValueAsDefault"), TEXT("Use preview value as default for function input."))
			.StringEnum(TEXT("blendInputRelevance"), {
				TEXT("General")
			}, TEXT("Material function input relevance. Authoring currently accepts only General."))
			.FreeformObject(TEXT("value"),
				TEXT("Value to set (number, vector object, or texture path)."))
			.ArrayOfObjects(TEXT("overrides"), TEXT("Bulk parameter override list."))
			.StringEnum(TEXT("operation"), {
				TEXT("Add"),
				TEXT("Subtract"),
				TEXT("Multiply"),
				TEXT("Divide"),
				TEXT("Lerp"),
				TEXT("Clamp"),
				TEXT("Power"),
				TEXT("SquareRoot"),
				TEXT("Abs"),
				TEXT("Floor"),
				TEXT("Ceil"),
				TEXT("Frac"),
				TEXT("Sine"),
				TEXT("Cosine"),
				TEXT("Saturate"),
				TEXT("OneMinus"),
				TEXT("Min"),
				TEXT("Max"),
				TEXT("Dot"),
				TEXT("Cross"),
				TEXT("Normalize"),
				TEXT("Append")
			}, TEXT("Math operation type."))
			.Number(TEXT("constA"), TEXT("Constant A input value."))
			.Number(TEXT("constB"), TEXT("Constant B input value."))
			.String(TEXT("code"), TEXT("Code or expression."))
			.StringEnum(TEXT("outputType"), {
				TEXT("Float1"),
				TEXT("Float2"),
				TEXT("Float3"),
				TEXT("Float4"),
				TEXT("MaterialAttributes")
			}, TEXT("Output type of custom expression."))
			.String(TEXT("description"),
				TEXT("Description for custom expression or function."))
			.String(TEXT("sourceNodeId"), TEXT("Source node ID for connection."))
			.FreeformObject(TEXT("sourceExpression"), TEXT("ExpressionTarget object for source expression."))
			.FreeformObject(TEXT("targetExpression"), TEXT("ExpressionTarget object for target expression."))
			.FreeformObject(TEXT("expression"), TEXT("ExpressionTarget object."))
			.FreeformObject(TEXT("target"), TEXT("Connection target object."))
			.Integer(TEXT("sourceOutputIndex"), TEXT("Source expression output index."))
			.String(TEXT("sourceOutputName"), TEXT("Source expression output name."))
			.String(TEXT("targetInputName"), TEXT("Target expression input name."))
			.String(TEXT("targetMaterialPin"), TEXT("Main material pin name."))
			.String(TEXT("sourcePin"), TEXT("Source pin name (output)."))
			.String(TEXT("targetNodeId"), TEXT("Target node ID for connection."))
			.String(TEXT("targetPin"), TEXT("Target pin name (input)."))
			.String(TEXT("nodeId"), TEXT("ID of the node."))
			.String(TEXT("pinName"), TEXT("Name of the pin."))
			.String(TEXT("functionPath"), TEXT("Path to function asset."))
			.Bool(TEXT("exposeToLibrary"),
				TEXT("Expose function to material library."))
			.String(TEXT("inputName"), TEXT("Name of the input."))
			.String(TEXT("outputName"), TEXT("Name of the output."))
			.StringEnum(TEXT("inputType"), {
				TEXT("Float1"),
				TEXT("Float2"),
				TEXT("Float3"),
				TEXT("Float4"),
				TEXT("Texture2D"),
				TEXT("TextureCube"),
				TEXT("Bool"),
				TEXT("MaterialAttributes")
			}, TEXT("Type of function input/output."))
			.String(TEXT("parentMaterial"),
				TEXT("Path to parent material for instances."))
			.StringEnum(TEXT("instanceKind"), {
				TEXT("function"),
				TEXT("materialLayer"),
				TEXT("materialLayerBlend")
			}, TEXT("Material function instance kind."))
			.ArrayOfObjects(TEXT("inputs"), TEXT("Custom expression input definitions."))
			.ArrayOfObjects(TEXT("additionalOutputs"), TEXT("Custom expression additional output definitions."))
			.ArrayOfObjects(TEXT("additionalDefines"), TEXT("Custom expression additional define definitions."))
			.Array(TEXT("includeFilePaths"), TEXT("Custom expression include file paths."))
			.String(TEXT("layerName"), TEXT("Name of the layer."))
			.StringEnum(TEXT("blendType"), {
				TEXT("LB_WeightBlend"),
				TEXT("LB_AlphaBlend"),
				TEXT("LB_HeightBlend")
			}, TEXT("Landscape layer blend type."))
			.ArrayOfObjects(TEXT("layers"),
				TEXT("Array of layer configurations for layer blend."))
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
			.Required({TEXT("subAction")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageMaterialAuthoring);
