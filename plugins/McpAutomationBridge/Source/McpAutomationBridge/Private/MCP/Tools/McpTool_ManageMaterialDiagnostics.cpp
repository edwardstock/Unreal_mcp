// McpTool_ManageMaterialDiagnostics.cpp - manage_material_diagnostics tool definition

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageMaterialDiagnostics : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_material_diagnostics"); }

	FString GetDescription() const override
	{
		return TEXT("Inspect, validate, repair, compile, save, and reload material-family assets.");
	}

	FString GetCategory() const override { return TEXT("authoring"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("subAction"), {
				TEXT("dump_raw_graph"),
				TEXT("dump_raw_expression"),
				TEXT("dump_pins"),
				TEXT("dump_parameter_namespace"),
				TEXT("validate_material_graph"),
				TEXT("repair_function_call_pins"),
				TEXT("repair_custom_expression_outputs"),
				TEXT("repair_missing_expression_guids"),
				TEXT("remove_null_expressions"),
				TEXT("compile_material_diagnostics"),
				TEXT("verify_save_reload")
			}, TEXT("Material diagnostics sub-action to perform."))
			.String(TEXT("assetPath"), TEXT("Material, material function, material instance, or material function instance asset path."))
			.FreeformObject(TEXT("expression"), TEXT("ExpressionTarget object or string reference."))
			.Integer(TEXT("maxDepth"), TEXT("Maximum reflected dump depth. Default: 3."))
			.StringEnum(TEXT("verbosity"), {
				TEXT("summary"),
				TEXT("normal"),
				TEXT("full")
			}, TEXT("Raw dump verbosity. Default: normal."))
			.Array(TEXT("classAllowList"), TEXT("Allowed expression class names. Empty means all classes."))
			.Array(TEXT("propertyAllowList"), TEXT("Allowed reflected property names. Empty means all properties."))
			.Bool(TEXT("includeMainMaterialPins"), TEXT("Include owner-level material pins for UMaterial assets."))
			.Bool(TEXT("includeInherited"), TEXT("Include inherited parameter values when available."))
			.Bool(TEXT("includeLayered"), TEXT("Include layered parameter identities when available."))
			.Array(TEXT("checks"), TEXT("Validation checks to run. Empty means default checks."))
			.ArrayOfObjects(TEXT("expressions"), TEXT("ExpressionTarget objects for selective repair."))
			.Number(TEXT("timeoutSeconds"), TEXT("Compile poll timeout in seconds. Default: 10."))
			.Bool(TEXT("save"), TEXT("Save before repair or reload verification."))
			.Bool(TEXT("includeRawSummary"), TEXT("Include before/after raw graph summaries for reload verification."))
			.Required({TEXT("subAction"), TEXT("assetPath")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageMaterialDiagnostics);
