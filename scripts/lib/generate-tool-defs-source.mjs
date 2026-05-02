// Pure renderer used by both scripts/generate-tool-defs.mjs and Vitest tests.
// Input: an array of tool manifest entries (parsed from generated/tool-manifest.json).
// Output: the full source text of src/tools/consolidated-tool-definitions.ts.
//
// Native (FMcpSchemaBuilder) emits literal JSON, so commonSchemas is already inlined
// in the input. This renderer never touches commonSchemas.

const HEADER = `// AUTO-GENERATED FROM generated/tool-manifest.json — DO NOT EDIT.
// Regenerate via: python Scripts/run-cmd.py DumpMcpManifest && npm run gen:tool-defs
//

export interface ToolDefinition {
  category?: 'core' | 'world' | 'authoring' | 'gameplay' | 'utility';
  name: string;
  description: string;
  inputSchema: Record<string, unknown>;
  [key: string]: unknown;
}
`;

export function generateToolDefsSource(manifest) {
  if (!Array.isArray(manifest)) {
    throw new Error('generateToolDefsSource: expected manifest to be an array');
  }
  const body = JSON.stringify(manifest, null, 2);
  return `${HEADER}export const consolidatedToolDefinitions: ToolDefinition[] = ${body};\n`;
}
