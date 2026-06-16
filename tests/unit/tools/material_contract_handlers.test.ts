import { beforeEach, describe, expect, it, vi } from 'vitest';
import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { handleAssetTools } from '../../../src/tools/handlers/asset-handlers';
import { handleMaterialTools } from '../../../src/tools/handlers/material-handlers';

describe('material graph tool contract', () => {
  let mockTools: any;

  beforeEach(() => {
    mockTools = {
      automationBridge: {
        isConnected: vi.fn().mockReturnValue(true),
        sendAutomationRequest: vi.fn().mockResolvedValue({ success: true }),
      },
      assetTools: {
        createFolder: vi.fn(),
        importAsset: vi.fn(),
        duplicateAsset: vi.fn(),
        renameAsset: vi.fn(),
        moveAsset: vi.fn(),
        deleteAssets: vi.fn(),
        generateLODs: vi.fn(),
        createThumbnail: vi.fn(),
        getMetadata: vi.fn(),
        validate: vi.fn(),
        generateReport: vi.fn(),
        searchAssets: vi.fn(),
        findByTag: vi.fn(),
        getDependencies: vi.fn(),
        getSourceControlState: vi.fn(),
      },
    };
  });

  it('does not route legacy manage_asset.connect_material_pins through the old single-pin handler', async () => {
    mockTools.automationBridge.sendAutomationRequest.mockResolvedValueOnce({
      success: false,
      error: 'UNKNOWN_SUB_ACTION',
      message: 'Unknown manage_asset subAction: connect_material_pins',
    });

    const result = await handleAssetTools('connect_material_pins', {
      assetPath: '/Game/M',
      sourceNodeId: 'A',
      targetNodeId: 'B',
      targetPin: 'A',
    }, mockTools);

    const lastCall = mockTools.automationBridge.sendAutomationRequest.mock.lastCall;
    expect(lastCall[0]).toBe('manage_asset');
    expect(lastCall[1]).toMatchObject({
      subAction: 'connect_material_pins',
      assetPath: '/Game/M',
      sourceNodeId: 'A',
      targetNodeId: 'B',
      targetPin: 'A',
    });
    expect(result.success).toBe(false);
  });

  it('passes manage_material.connect_material_pins through as batch connections', async () => {
    await handleMaterialTools('connect_material_pins', {
      assetPath: '/Game/M',
      connections: [
        {
          fromNode: 'A',
          fromOutputIndex: 0,
          toNode: 'B',
          toPin: 'A',
        },
      ],
    }, mockTools);

    const lastCall = mockTools.automationBridge.sendAutomationRequest.mock.lastCall;
    expect(lastCall[0]).toBe('manage_material');
    expect(lastCall[1]).toEqual({
      subAction: 'connect_material_pins',
      assetPath: '/Game/M',
      connections: [
        {
          fromNode: 'A',
          fromOutputIndex: 0,
          toNode: 'B',
          toPin: 'A',
        },
      ],
    });
  });

  it('does not normalize manage_material.connect_nodes through the legacy connect shim', async () => {
    await handleMaterialTools('connect_nodes', {
      assetPath: '/Game/M',
      fromNode: 'A',
      fromPin: 'RGB',
      toNode: 'B',
      toPin: 'A',
    }, mockTools);

    const lastCall = mockTools.automationBridge.sendAutomationRequest.mock.lastCall;
    expect(lastCall[0]).toBe('manage_material');
    expect(lastCall[1]).toEqual({
      subAction: 'connect_nodes',
      assetPath: '/Game/M',
      fromNode: 'A',
      fromPin: 'RGB',
      toNode: 'B',
      toPin: 'A',
    });
  });

  it('does not keep legacy material graph cases in the manage_asset handler source', () => {
    const source = readFileSync(resolve(__dirname, '../../../src/tools/handlers/asset-handlers.ts'), 'utf8');

    expect(source).not.toContain("case 'set_material_node_position':");
    expect(source).not.toContain("case 'connect_material_pins':");
    expect(source).not.toContain("case 'remove_material_node':");
    expect(source).not.toContain("case 'break_material_connections':");
    expect(source).not.toContain("case 'bulk_get_material_expression_details':");
  });

  it('does not keep the legacy connect_nodes shim in the manage_material handler source', () => {
    const source = readFileSync(resolve(__dirname, '../../../src/tools/handlers/material-handlers.ts'), 'utf8');

    expect(source).not.toContain("case 'connect_nodes':");
    expect(source).not.toContain("subAction: 'connect_nodes'");
    expect(source).not.toContain('pin-name-as-fallback id normalization');
  });

  it('does not keep the native single-pin material connect handler', () => {
    const pluginRoot = resolve(__dirname, '../../../');
    const assetWorkflow = readFileSync(resolve(
      pluginRoot,
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_AssetWorkflowHandlers.cpp',
    ), 'utf8');
    const subsystemHeader = readFileSync(resolve(
      pluginRoot,
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Public/McpAutomationBridgeSubsystem.h',
    ), 'utf8');
    const nodeOps = readFileSync(resolve(
      pluginRoot,
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_MaterialAuthoring_NodeOps.cpp',
    ), 'utf8');

    expect(assetWorkflow).not.toContain('HandleConnectMaterialPins');
    expect(subsystemHeader).not.toContain('HandleConnectMaterialPins');
    expect(nodeOps).not.toContain('HandleConnectMaterialPins');
    expect(nodeOps).not.toContain('SubAction == TEXT("connect_nodes")');
  });

  it('does not keep the manage_material_graph shadow-tool TS plumbing', () => {
    const pluginRoot = resolve(__dirname, '../../../');
    const consolidatedHandlers = readFileSync(resolve(
      pluginRoot,
      'src/tools/consolidated-tool-handlers.ts',
    ), 'utf8');
    const graphHandlers = readFileSync(resolve(
      pluginRoot,
      'src/tools/handlers/graph-handlers.ts',
    ), 'utf8');

    // The MATERIAL_GRAPH_ACTION_MAP / isMaterialGraphAction / 'manage_material_graph'
    // chain is the silent proxy that re-routed manage_asset.connect_material_pins
    // into the legacy single-pin native handler. None of these names may survive.
    expect(consolidatedHandlers).not.toContain('MATERIAL_GRAPH_ACTION_MAP');
    expect(consolidatedHandlers).not.toContain('isMaterialGraphAction');
    expect(consolidatedHandlers).not.toContain("'manage_material_graph'");
    expect(consolidatedHandlers).not.toContain('"manage_material_graph"');

    expect(graphHandlers).not.toContain('handleMaterialGraph');
    expect(graphHandlers).not.toContain("case 'manage_material_graph'");
    expect(graphHandlers).not.toContain('"manage_material_graph"');
  });

  it('does not keep the manage_material_graph shadow-tool native module', () => {
    const pluginRoot = resolve(__dirname, '../../../');
    const nativeFile = resolve(
      pluginRoot,
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_MaterialGraphHandlers.cpp',
    );
    expect(existsSync(nativeFile)).toBe(false);

    const subsystemHeader = readFileSync(resolve(
      pluginRoot,
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Public/McpAutomationBridgeSubsystem.h',
    ), 'utf8');
    const subsystemImpl = readFileSync(resolve(
      pluginRoot,
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridgeSubsystem.cpp',
    ), 'utf8');
    const processRequest = readFileSync(resolve(
      pluginRoot,
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_ProcessRequest.cpp',
    ), 'utf8');

    expect(subsystemHeader).not.toContain('HandleMaterialGraphAction');
    expect(subsystemHeader).not.toContain('HandleAddMaterialTextureSample');
    expect(subsystemHeader).not.toContain('HandleAddMaterialExpression');
    expect(subsystemHeader).not.toContain('HandleCreateMaterialNodes');

    expect(subsystemImpl).not.toContain('HandleMaterialGraphAction');
    expect(subsystemImpl).not.toContain('add_material_texture_sample');
    expect(subsystemImpl).not.toContain('add_material_expression');
    expect(subsystemImpl).not.toContain('create_material_nodes');

    expect(processRequest).not.toContain('HandleMaterialGraphAction');
  });
});
