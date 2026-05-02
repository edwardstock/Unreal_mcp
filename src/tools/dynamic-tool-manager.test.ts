import { describe, expect, it } from 'vitest';
import { DynamicToolManager } from './dynamic-tool-manager.js';

describe('DynamicToolManager defaults', () => {
  it('enables only the named tools when defaultEnabledSet is provided', () => {
    const mgr = new DynamicToolManager();
    mgr.initialize({ defaultEnabledSet: new Set(['manage_lighting', 'inspect']) });

    const enabled = mgr.getEnabledToolDefinitions().map((d) => d.name);
    expect(enabled).toContain('manage_lighting');
    expect(enabled).toContain('inspect');
    expect(enabled).not.toContain('manage_widget_authoring');
  });

  it('enables every tool when defaultEnabledSet is null (back-compat)', () => {
    const mgr = new DynamicToolManager();
    mgr.initialize({ defaultEnabledSet: null });

    const status = mgr.getStatus();
    expect(status.enabledTools).toBe(status.totalTools);
    expect(status.disabledTools).toBe(0);
  });

  it('counts category enabledCount based on actual enabled state', () => {
    const mgr = new DynamicToolManager();
    mgr.initialize({ defaultEnabledSet: new Set(['inspect']) });

    const core = mgr.listCategories().find((c) => c.name === 'core');
    expect(core).toBeDefined();
    expect(core!.enabledCount).toBe(1); // only `inspect`, even though core has more tools
    expect(core!.toolCount).toBeGreaterThan(1);
  });

  it('logs a warning for unknown tool names but does not throw', () => {
    const mgr = new DynamicToolManager();
    expect(() =>
      mgr.initialize({ defaultEnabledSet: new Set(['no_such_tool', 'inspect']) })
    ).not.toThrow();

    const enabled = mgr.getEnabledToolDefinitions().map((d) => d.name);
    expect(enabled).toContain('inspect');
    expect(enabled).not.toContain('no_such_tool');
  });
});
