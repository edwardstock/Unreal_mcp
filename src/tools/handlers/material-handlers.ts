/**
 * Material Handlers — thin passthrough.
 *
 * Architecture: native C++ FMcpToolRegistry's manage_material schema is
 * the single source of truth for which subActions exist. This file no longer
 * duplicates the dispatch table — the default branch passes any unknown action
 * directly to native via executeAutomationRequest. C++ returns its own
 * UNKNOWN_SUBACTION error if the subAction is unregistered.
 *
 * Only client-side shims with real work remain as explicit cases:
 *   - create_material               — parses materialPath into name+path
 *   - create_material_instance      — backward-compat dual-format
 *                                     (name+path+parentMaterial vs
 *                                      instancePath+parentMaterialPath)
 *   - connect_nodes / connect_material_pins
 *                                  — pin-name-as-fallback id normalization
 *   - set_material_parameter        — TS-side generic dispatcher by parameterType
 *                                     (no native equivalent yet)
 *   - rebuild_material              — alias for compile_material
 *
 * Everything else collapses to passthrough; this is what unblocked actions like
 * add_texture_object_parameter, add_function_input/output, add_material_function_call
 * which were declared in the C++ schema but silently dropped by the old switch.
 */

import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs } from '../../types/handler-types.js';
import type { AutomationResponse } from '../../types/automation-responses.js';
import { executeAutomationRequest } from './common-handlers.js';
import {
  normalizeArgs,
  extractString,
  extractOptionalString,
  extractOptionalBoolean,
} from './argument-helper.js';
import { ResponseFactory } from '../../utils/response-factory.js';
import { TOOL_ACTIONS } from '../../utils/action-constants.js';

/** Helper to parse a full material path into name and directory */
function parseMaterialPath(fullPath: string | undefined): { name: string; path: string } | null {
  if (!fullPath) return null;
  const lastSlash = fullPath.lastIndexOf('/');
  if (lastSlash < 0) return { name: fullPath, path: '/Game' };
  const name = fullPath.substring(lastSlash + 1);
  const path = fullPath.substring(0, lastSlash);
  return { name, path };
}

/**
 * Handle material actions
 */
export async function handleMaterialTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  try {
    switch (action) {
      // ===== Shim: create_material — parse materialPath into name+path =====
      case 'create_material': {
        const rawArgs = args as Record<string, unknown>;
        const materialPath = extractOptionalString(rawArgs, 'materialPath') ??
                            extractOptionalString(rawArgs, 'material_path') ??
                            extractOptionalString(rawArgs, 'assetPath');

        let name: string;
        let path: string;

        if (materialPath) {
          const parsed = parseMaterialPath(materialPath);
          if (!parsed) {
            return ResponseFactory.error('Invalid materialPath format', 'INVALID_ARGUMENT');
          }
          name = parsed.name;
          path = parsed.path;
        } else {
          const params = normalizeArgs(args, [
            { key: 'name', required: true },
            { key: 'path', aliases: ['directory'], default: '/Game/Materials' },
          ]);
          name = extractString(params, 'name');
          path = extractOptionalString(params, 'path') ?? '/Game/Materials';
        }

        const materialDomain = extractOptionalString(rawArgs, 'materialDomain') ??
                              extractOptionalString(rawArgs, 'domain') ?? 'Surface';
        const blendMode = extractOptionalString(rawArgs, 'blendMode') ?? 'Opaque';
        const shadingModel = extractOptionalString(rawArgs, 'shadingModel') ?? 'DefaultLit';
        const twoSided = extractOptionalBoolean(rawArgs, 'twoSided') ?? false;
        const save = extractOptionalBoolean(rawArgs, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL, {
          subAction: 'create_material',
          name,
          path,
          materialDomain,
          blendMode,
          shadingModel,
          twoSided,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create material', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Material '${name}' created`);
      }

      // ===== Shim: create_material_instance — dual-format =====
      case 'create_material_instance': {
        const rawArgs = args as Record<string, unknown>;
        const instancePath = extractOptionalString(rawArgs, 'instancePath') ??
                            extractOptionalString(rawArgs, 'instance_path') ??
                            extractOptionalString(rawArgs, 'materialPath');
        const parentMaterialPath = extractOptionalString(rawArgs, 'parentMaterialPath') ??
                                  extractOptionalString(rawArgs, 'parent_material_path') ??
                                  extractOptionalString(rawArgs, 'parentMaterial') ??
                                  extractOptionalString(rawArgs, 'parent');

        let name: string;
        let path: string;
        let parentMaterial: string;

        if (instancePath) {
          const parsed = parseMaterialPath(instancePath);
          if (!parsed) {
            return ResponseFactory.error('Invalid instancePath format', 'INVALID_ARGUMENT');
          }
          name = parsed.name;
          path = parsed.path;
          parentMaterial = parentMaterialPath ?? '';
        } else {
          const params = normalizeArgs(args, [
            { key: 'name', required: true },
            { key: 'path', aliases: ['directory'], default: '/Game/Materials' },
            { key: 'parentMaterial', aliases: ['parent'], required: true },
          ]);
          name = extractString(params, 'name');
          path = extractOptionalString(params, 'path') ?? '/Game/Materials';
          parentMaterial = extractString(params, 'parentMaterial');
        }

        if (!parentMaterial) {
          return ResponseFactory.error('parentMaterialPath or parent is required', 'MISSING_PARENT');
        }

        const save = extractOptionalBoolean(rawArgs, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL, {
          subAction: 'create_material_instance',
          name,
          path,
          parentMaterial,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create material instance', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Material instance '${name}' created`);
      }

      // ===== Shim: connect_nodes / connect_material_pins — pin-name-as-id fallback =====
      case 'connect_nodes':
      case 'connect_material_pins': {
        const rawArgs = args as Record<string, unknown>;
        const assetPath = extractOptionalString(rawArgs, 'assetPath') ??
                         extractOptionalString(rawArgs, 'materialPath') ?? '';

        const sourceNodeId = extractOptionalString(rawArgs, 'sourceNodeId') ??
                            extractOptionalString(rawArgs, 'fromNode') ?? '';
        const targetNodeId = extractOptionalString(rawArgs, 'targetNodeId') ??
                            extractOptionalString(rawArgs, 'toNode') ?? '';
        const sourcePin = extractOptionalString(rawArgs, 'sourcePin') ??
                         extractOptionalString(rawArgs, 'fromPin') ?? '';
        const targetPin = extractOptionalString(rawArgs, 'targetPin') ??
                         extractOptionalString(rawArgs, 'toPin') ??
                         extractOptionalString(rawArgs, 'inputName') ?? '';

        // If node IDs not provided, use pin names as identifiers
        const effectiveSourceId = sourceNodeId || sourcePin;
        const effectiveTargetId = targetNodeId || targetPin;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL, {
          subAction: 'connect_nodes',
          assetPath,
          sourceNodeId: effectiveSourceId,
          sourcePin,
          targetNodeId: effectiveTargetId,
          inputName: targetPin,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to connect nodes', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Nodes connected');
      }

      // ===== Shim: rebuild_material — alias for compile_material =====
      case 'rebuild_material':
        return handleMaterialTools('compile_material', args, tools);

      // ===== Shim: set_material_parameter — TS-side generic dispatcher =====
      case 'set_material_parameter': {
        const rawArgs = args as Record<string, unknown>;
        const assetPath = extractOptionalString(rawArgs, 'assetPath') ??
                         extractOptionalString(rawArgs, 'materialPath') ??
                         extractOptionalString(rawArgs, 'instancePath') ?? '';
        const parameterName = extractOptionalString(rawArgs, 'parameterName') ?? '';
        const parameterType = extractOptionalString(rawArgs, 'parameterType') ?? 'scalar';
        const save = extractOptionalBoolean(rawArgs, 'save') ?? true;
        const value = rawArgs.value;

        if (!assetPath) {
          return ResponseFactory.error('Missing required argument: assetPath (or instancePath, materialPath)', 'MISSING_ASSET_PATH');
        }
        if (!parameterName) {
          return ResponseFactory.error('Missing required argument: parameterName', 'MISSING_PARAMETER_NAME');
        }
        if (value === undefined) {
          return ResponseFactory.error('Missing required argument: value', 'MISSING_VALUE');
        }

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL, {
          subAction: 'set_material_parameter',
          assetPath,
          parameterName,
          value,
          parameterType,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Parameter '${parameterName}' set`);
      }

      // ===== N1: get_custom_expression — passthrough to bridge =====
      case 'get_custom_expression':
      // ===== N2: get_parameter_defaults — passthrough to bridge =====
      case 'get_parameter_defaults': {
        const res = (await executeAutomationRequest(
          tools,
          TOOL_ACTIONS.MANAGE_MATERIAL,
          { subAction: action, ...(args as Record<string, unknown>) }
        )) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? `Failed: ${action}`, res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `${action} succeeded`);
      }

      // ===== Default: passthrough =====
      // The native C++ schema for manage_material is the source of
      // truth for which subActions are valid. Pass everything through as-is and
      // let C++ either dispatch it or return UNKNOWN_SUBACTION itself. This is
      // what unblocks ~50 actions the old hardcoded switch used to silently
      // reject (add_texture_object_parameter, add_function_input/output,
      // add_material_function_call, etc.).
      default: {
        const res = (await executeAutomationRequest(
          tools,
          TOOL_ACTIONS.MANAGE_MATERIAL,
          { subAction: action, ...(args as Record<string, unknown>) }
        )) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? `Failed: ${action}`, res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `${action} succeeded`);
      }
    }
  } catch (error) {
    const err = error instanceof Error ? error : new Error(String(error));
    return ResponseFactory.error(`Material error: ${err.message}`, 'MATERIAL_ERROR');
  }
}
