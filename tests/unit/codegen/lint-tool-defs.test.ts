import { describe, expect, it } from 'vitest';
import { lintToolManifest } from '../../../scripts/lib/lint-tool-defs.mjs';

describe('lintToolManifest', () => {
  it('returns no warnings on a clean manifest', () => {
    const manifest = [
      {
        name: 'inspect',
        description: 'Inspect actors.',
        category: 'core',
        inputSchema: {
          type: 'object',
          properties: { target: { type: 'string', description: 'Target name.' } },
          required: ['target']
        }
      }
    ];
    expect(lintToolManifest(manifest)).toEqual([]);
  });

  it('flags missing top-level description', () => {
    const w = lintToolManifest([
      { name: 'foo', description: '', inputSchema: { type: 'object', properties: {} } }
    ]);
    expect(w).toContainEqual({ tool: 'foo', kind: 'missing-tool-description' });
  });

  it('flags missing property description', () => {
    const w = lintToolManifest([
      {
        name: 'foo',
        description: 'x',
        inputSchema: {
          type: 'object',
          properties: { action: { type: 'string' } }
        }
      }
    ]);
    expect(w).toContainEqual({ tool: 'foo', kind: 'missing-property-description', property: 'action' });
  });

  it('flags type:object with no properties', () => {
    const w = lintToolManifest([
      {
        name: 'foo',
        description: 'x',
        inputSchema: {
          type: 'object',
          properties: { meta: { type: 'object', description: 'm' } }
        }
      }
    ]);
    expect(w).toContainEqual({ tool: 'foo', kind: 'object-without-properties', property: 'meta' });
  });

  it('does NOT flag type:object with additionalProperties:true (freeform)', () => {
    const w = lintToolManifest([
      {
        name: 'foo',
        description: 'x',
        inputSchema: {
          type: 'object',
          properties: {
            value: {
              type: 'object',
              description: 'Generic value',
              additionalProperties: true
            }
          }
        }
      }
    ]);
    expect(w).toEqual([]);
  });

  it('flags required not in properties', () => {
    const w = lintToolManifest([
      {
        name: 'foo',
        description: 'x',
        inputSchema: {
          type: 'object',
          properties: { a: { type: 'string', description: 'a' } },
          required: ['a', 'ghost']
        }
      }
    ]);
    expect(w).toContainEqual({ tool: 'foo', kind: 'required-not-in-properties', property: 'ghost' });
  });

  it('flags empty enum', () => {
    const w = lintToolManifest([
      {
        name: 'foo',
        description: 'x',
        inputSchema: {
          type: 'object',
          properties: { mode: { type: 'string', description: 'm', enum: [] } }
        }
      }
    ]);
    expect(w).toContainEqual({ tool: 'foo', kind: 'empty-enum', property: 'mode' });
  });
});
