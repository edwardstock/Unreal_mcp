// Pure linter: takes a parsed manifest, returns an array of warnings.
// Each warning is { tool: string, kind: string, property?: string }.

export function lintToolManifest(manifest) {
  if (!Array.isArray(manifest)) {
    throw new Error('lintToolManifest: expected manifest to be an array');
  }
  const warnings = [];

  for (const tool of manifest) {
    const name = tool?.name ?? '<unnamed>';

    if (!tool?.description || typeof tool.description !== 'string' || tool.description.trim() === '') {
      warnings.push({ tool: name, kind: 'missing-tool-description' });
    }

    const schema = tool?.inputSchema;
    if (!schema || typeof schema !== 'object') continue;
    const properties = schema.properties ?? {};
    const required = Array.isArray(schema.required) ? schema.required : [];

    for (const [propName, propSchema] of Object.entries(properties)) {
      if (!propSchema || typeof propSchema !== 'object') continue;
      const desc = propSchema.description;
      if (!desc || typeof desc !== 'string' || desc.trim() === '') {
        warnings.push({ tool: name, kind: 'missing-property-description', property: propName });
      }
      // type:'object' without inner properties is a smell, UNLESS the schema
      // declares additionalProperties:true — that's the FreeformObject case
      // (intentionally any-shape, e.g. parameter values whose type depends on
      // runtime context).
      const isFreeform = propSchema.additionalProperties === true;
      if (
        propSchema.type === 'object'
        && !isFreeform
        && (!propSchema.properties || Object.keys(propSchema.properties).length === 0)
      ) {
        warnings.push({ tool: name, kind: 'object-without-properties', property: propName });
      }
      if (Array.isArray(propSchema.enum) && propSchema.enum.length === 0) {
        warnings.push({ tool: name, kind: 'empty-enum', property: propName });
      }
    }

    for (const reqName of required) {
      if (!(reqName in properties)) {
        warnings.push({ tool: name, kind: 'required-not-in-properties', property: reqName });
      }
    }
  }

  return warnings;
}
