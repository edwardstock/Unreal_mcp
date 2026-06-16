import { describe, expect, it } from 'vitest';
import { generateToolDefsSource } from '../../../scripts/lib/generate-tool-defs-source.mjs';

const fixture = [
  {
    name: 'manage_lighting',
    description: 'Lighting controls.',
    category: 'world',
    inputSchema: {
      type: 'object',
      properties: {
        action: { type: 'string', enum: ['list_light_types'], description: 'Action.' }
      },
      required: ['action']
    }
  },
  {
    name: 'inspect',
    description: 'Inspect actors.',
    category: 'core',
    inputSchema: {
      type: 'object',
      properties: { target: { type: 'string', description: 'Target name.' } }
    },
    annotations: { readOnly: true }
  }
];

describe('generateToolDefsSource', () => {
  it('emits the AUTO-GENERATED header', () => {
    const out = generateToolDefsSource(fixture);
    expect(out).toMatch(/AUTO-GENERATED FROM generated\/tool-manifest\.json/);
    expect(out).toMatch(/DO NOT EDIT/);
  });

  it('emits a ToolDefinition interface without outputSchema', () => {
    const out = generateToolDefsSource(fixture);
    expect(out).toMatch(/export interface ToolDefinition/);
    expect(out).not.toMatch(/outputSchema/);
    expect(out).not.toMatch(/commonSchemas/);
  });

  it('emits consolidatedToolDefinitions with all entries', () => {
    const out = generateToolDefsSource(fixture);
    expect(out).toMatch(/export const consolidatedToolDefinitions: ToolDefinition\[\] =/);
    expect(out).toMatch(/"name": "manage_lighting"/);
    expect(out).toMatch(/"name": "inspect"/);
    expect(out).toMatch(/"category": "world"/);
    expect(out).toMatch(/"readOnly": true/);
  });

  it('produces valid TypeScript that round-trips through JSON.parse on the data block', () => {
    const out = generateToolDefsSource(fixture);
    const match = out.match(/export const consolidatedToolDefinitions: ToolDefinition\[\] = (\[[\s\S]*\]);/);
    expect(match).not.toBeNull();
    const parsed = JSON.parse(match![1]);
    expect(parsed).toHaveLength(2);
    expect(parsed[0].name).toBe('manage_lighting');
  });

  it('throws when manifest is not an array', () => {
    expect(() => generateToolDefsSource({} as never)).toThrow();
  });
});
