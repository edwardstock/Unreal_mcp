import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, AssetArgs } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalNumber, extractOptionalBoolean, extractOptionalArray } from './argument-helper.js';
import { ResponseFactory } from '../../utils/response-factory.js';
import { sanitizePath } from '../../utils/validation.js';

/**
 * Detect path traversal attempts in user input.
 * Returns true if the path contains suspicious traversal patterns.
 */
function isPathTraversalAttempt(path: string): boolean {
  if (!path || typeof path !== 'string') return false;
  const normalized = path.toLowerCase();
  // Check for directory traversal patterns
  const traversalPatterns = [
    '../', '..\\',           // Parent directory traversal
    '/etc/', '/proc/', '/sys/', // Unix system paths
    'c:\\', 'c:/',           // Windows absolute paths
    '\\\\', '//',            // UNC paths
    '%2e%2e', '%252e',       // URL-encoded traversal
    '....//', '....\\',      // Double-dot variations
  ];
  return traversalPatterns.some(pattern => normalized.includes(pattern));
}

/**
 * Validate paths for security issues.
 * Returns an error response if traversal detected, null otherwise.
 */
function validatePathSecurity(pathValue: string | undefined, paramName: string): Record<string, unknown> | null {
  if (!pathValue) return null;
  if (isPathTraversalAttempt(pathValue)) {
    return cleanObject({
      success: false,
      error: 'SECURITY_VIOLATION',
      message: `Path traversal attempt detected in ${paramName}. Access denied.`,
      [paramName]: pathValue
    });
  }
  return null;
}

/**
 * Validate an array of paths for security issues.
 * Returns an error response if any path has traversal detected, null otherwise.
 */
function validatePathsSecurity(paths: string[] | undefined, paramName: string): Record<string, unknown> | null {
  if (!paths || !Array.isArray(paths)) return null;
  for (const p of paths) {
    if (isPathTraversalAttempt(p)) {
      return cleanObject({
        success: false,
        error: 'SECURITY_VIOLATION',
        message: `Path traversal attempt detected in ${paramName}. Access denied.`,
        [paramName]: paths
      });
    }
  }
  return null;
}

/** Asset info from list response */
interface AssetListItem {
  path?: string;
  package?: string;
  name?: string;
}

/** Response from list/search operations */
interface AssetListResponse {
  success?: boolean;
  assets?: AssetListItem[];
  result?: { assets?: AssetListItem[]; folders?: string[]; totalCount?: number };
  folders?: string[];
  totalCount?: number;
  [key: string]: unknown;
}

/** Response from asset operations */
interface AssetOperationResponse {
  success?: boolean;
  message?: string;
  error?: string;
  errorCode?: string;
  tags?: Record<string, unknown>;
  metadata?: Record<string, unknown>;
  [key: string]: unknown;
}

export async function handleAssetTools(action: string, args: HandlerArgs, tools: ITools): Promise<Record<string, unknown>> {
  try {
    switch (action) {
      case 'list': {
        // Route through C++ HandleListAssets for proper asset enumeration
        const params = normalizeArgs(args, [
          { key: 'path', aliases: ['directory', 'directoryPath', 'assetPath'], default: '/Game' },
          { key: 'limit', default: 50 },
          { key: 'offset', default: 0 },
          { key: 'recursive', aliases: ['recursivePaths'], default: false },
          { key: 'depth', default: undefined }
        ]);

        let path = extractOptionalString(params, 'path') ?? '/Game';
        path = sanitizePath(path);

        const limit = extractOptionalNumber(params, 'limit') ?? 50;
        const offset = extractOptionalNumber(params, 'offset') ?? 0;
        const recursive = extractOptionalBoolean(params, 'recursive') ?? false;
        const depth = extractOptionalNumber(params, 'depth');

        const effectiveRecursive = recursive === true || (depth !== undefined && depth > 0);

        const res = await executeAutomationRequest(tools, 'list', {
          path,
          recursive: effectiveRecursive,
          depth,
          pagination: { limit, offset }
        }) as AssetListResponse;

        const assets: AssetListItem[] = (Array.isArray(res.assets) ? res.assets :
          (Array.isArray(res.result) ? res.result : (res.result?.assets || [])));

        // New: Handle folders
        const folders: string[] = Array.isArray(res.folders) ? res.folders : (res.result?.folders || []);

        const totalCount = res.totalCount ?? res.result?.totalCount ?? assets.length;
        const limitedAssets = assets.slice(0, limit);
        const remaining = Math.max(0, totalCount - limit);

        let message = `Found ${totalCount} assets`;
        if (folders.length > 0) {
          message += ` and ${folders.length} folders`;
        }
        message += `: ${limitedAssets.map((a) => a.path || a.package || a.name || 'unknown').join(', ')}`;

        if (folders.length > 0 && limitedAssets.length < limit) {
          const remainingLimit = limit - limitedAssets.length;
          if (remainingLimit > 0) {
            const limitedFolders = folders.slice(0, remainingLimit);
            if (limitedAssets.length > 0) message += ', ';
            message += `Folders: [${limitedFolders.join(', ')}]`;
            if (folders.length > remainingLimit) message += '...';
          }
        }

        if (remaining > 0) {
          message += `... and ${remaining} others`;
        }

        return ResponseFactory.success({
          assets: limitedAssets,
          folders: folders,
          totalCount: totalCount,
          count: limitedAssets.length
        }, message);
      }
      case 'create_folder': {
        const params = normalizeArgs(args, [
          { key: 'path', aliases: ['directoryPath'], required: true }
        ]);
        // Validate path format
        const folderPath = extractString(params, 'path').trim();
        if (!folderPath.startsWith('/')) {
          return ResponseFactory.error('VALIDATION_ERROR', `Invalid folder path: '${folderPath}'. Path must start with '/'`);
        }
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          path: folderPath,
          subAction: 'create_folder'
        }) as AssetOperationResponse;
        return ResponseFactory.success(res, 'Folder created successfully');
      }
      case 'import': {
        const params = normalizeArgs(args, [
          { key: 'sourcePath', required: true },
          { key: 'destinationPath', required: true },
          { key: 'overwrite', default: false },
          { key: 'save', default: true }
        ]);

        const sourcePath = extractString(params, 'sourcePath');
        const destinationPath = extractString(params, 'destinationPath');
        const overwrite = extractOptionalBoolean(params, 'overwrite') ?? false;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = await executeAutomationRequest(tools, 'manage_asset', {
          sourcePath,
          destinationPath,
          overwrite,
          save,
          subAction: 'import'
        }) as AssetOperationResponse;

        // CRITICAL FIX: Pass through C++ failures instead of wrapping them
        // This prevents false positives where TS reports success when C++ failed
        if (res && typeof res.success === 'boolean' && res.success === false) {
          const errorCode = typeof res.error === 'string' ? res.error.toUpperCase() : 'IMPORT_FAILED';
          const message = typeof res.message === 'string' ? res.message : 'Asset import failed';
          return cleanObject({
            success: false,
            error: errorCode,
            message: message,
            sourcePath,
            destinationPath,
            data: res
          });
        }

        return ResponseFactory.success(res, 'Asset imported successfully');
      }
      case 'duplicate_asset':
      case 'duplicate': {
        const params = normalizeArgs(args, [
          { key: 'sourcePath', aliases: ['assetPath'], required: true },
          { key: 'destinationPath' },
          { key: 'newName' }
        ]);

        const sourcePath = extractString(params, 'sourcePath');
        let destinationPath = extractOptionalString(params, 'destinationPath');
        const newName = extractOptionalString(params, 'newName');

        if (newName) {
          if (!destinationPath) {
            const lastSlash = sourcePath.lastIndexOf('/');
            const parentDir = lastSlash > 0 ? sourcePath.substring(0, lastSlash) : '/Game';
            destinationPath = `${parentDir}/${newName}`;
          } else if (!destinationPath.endsWith(newName)) {
            if (destinationPath.endsWith('/')) {
              destinationPath = `${destinationPath}${newName}`;
            }
          }
        }

        if (!destinationPath) {
          throw new Error('destinationPath or newName is required for duplicate action');
        }

        const res = await executeAutomationRequest(tools, 'manage_asset', {
          sourcePath,
          destinationPath,
          subAction: 'duplicate'
        }) as AssetOperationResponse;
        return ResponseFactory.success(res, 'Asset duplicated successfully');
      }
      case 'rename_asset':
      case 'rename': {
        const params = normalizeArgs(args, [
          { key: 'sourcePath', aliases: ['assetPath'], required: true },
          { key: 'destinationPath' },
          { key: 'newName' }
        ]);

        const sourcePath = extractString(params, 'sourcePath');
        let destinationPath = extractOptionalString(params, 'destinationPath');
        const newName = extractOptionalString(params, 'newName');

        if (!destinationPath && newName) {
          const lastSlash = sourcePath.lastIndexOf('/');
          const parentDir = lastSlash > 0 ? sourcePath.substring(0, lastSlash) : '/Game';
          destinationPath = `${parentDir}/${newName}`;
        }

        if (!destinationPath) throw new Error('Missing destinationPath or newName');

        const res = await executeAutomationRequest(tools, 'manage_asset', {
          sourcePath,
          destinationPath,
          subAction: 'rename'
        }) as AssetOperationResponse;

        if (res && res.success === false) {
          const msg = (res.message || '').toLowerCase();
          if (msg.includes('already exists') || msg.includes('exists')) {
            return cleanObject({
              success: false,
              error: 'ASSET_ALREADY_EXISTS',
              message: res.message || 'Asset already exists at destination',
              sourcePath,
              destinationPath
            });
          }
        }
        return cleanObject(res);
      }
      case 'move_asset':
      case 'move': {
        const params = normalizeArgs(args, [
          { key: 'sourcePath', aliases: ['assetPath'], required: true },
          { key: 'destinationPath' }
        ]);

        const sourcePath = extractString(params, 'sourcePath');
        let destinationPath = extractOptionalString(params, 'destinationPath');
        const assetName = sourcePath.split('/').pop();
        if (assetName && destinationPath && !destinationPath.endsWith(assetName)) {
          destinationPath = `${destinationPath.replace(/\/$/, '')}/${assetName}`;
        }

        const res = await executeAutomationRequest(tools, 'manage_asset', {
          sourcePath,
          destinationPath: destinationPath ?? '',
          subAction: 'move'
        }) as AssetOperationResponse;
        return ResponseFactory.success(res, 'Asset moved successfully');
      }
      case 'delete_assets':
      case 'delete_asset':
      case 'delete': {
        let paths: string[] = [];
        const argsTyped = args as AssetArgs;
        // Check for array of paths first (delete_assets uses 'paths')
        if (Array.isArray(argsTyped.paths)) {
          paths = argsTyped.paths.filter((p): p is string => typeof p === 'string' && p.trim().length > 0);
        } else if (Array.isArray(argsTyped.assetPaths) || Array.isArray(argsTyped.asset_paths)) {
          paths = (argsTyped.assetPaths || argsTyped.asset_paths) as string[];
        } else {
          // Support both camelCase and snake_case parameter names for single path
          const single = argsTyped.assetPath || argsTyped.asset_path || argsTyped.path;
          if (typeof single === 'string' && single.trim()) {
            paths = [single.trim()];
          }
        }

        if (paths.length === 0) {
          // Return graceful error response instead of throwing
          // This handles cleanup scenarios where paths may be empty
          return ResponseFactory.error('INVALID_ARGUMENT', 'No paths provided for delete action. Provide assetPath (string) or assetPaths (array).');
        }

        // Normalize paths: strip object sub-path suffix (e.g., /Game/Folder/Asset.Asset -> /Game/Folder/Asset)
        // This handles the common pattern where full object paths are provided instead of package paths
        const normalizedPaths = paths.map(p => {
          let normalized = p.replace(/\\/g, '/').trim();
          // If the path contains a dot after the last slash, it's likely an object path (e.g., /Game/Folder/Asset.Asset)
          const lastSlash = normalized.lastIndexOf('/');
          if (lastSlash >= 0) {
            const afterSlash = normalized.substring(lastSlash + 1);
            const dotIndex = afterSlash.indexOf('.');
            if (dotIndex > 0) {
              // Strip the .ObjectName suffix
              normalized = normalized.substring(0, lastSlash + 1 + dotIndex);
            }
          }
          return normalized;
        });

        const res = await executeAutomationRequest(tools, 'manage_asset', {
          paths: normalizedPaths,
          subAction: 'delete'
        }) as AssetOperationResponse;
        
        // CRITICAL FIX: Check if C++ returned success=false and pass it through
        // This prevents false positives where TS wraps a failed C++ response as success
        if (res && typeof res.success === 'boolean' && res.success === false) {
          const errorCode = typeof res.error === 'string' ? res.error.toUpperCase() : 'OPERATION_FAILED';
          const message = typeof res.message === 'string' ? res.message : 'Asset deletion failed';
          return cleanObject({
            success: false,
            error: errorCode,
            message: message,
            paths: normalizedPaths,
            data: res
          });
        }
        
        return ResponseFactory.success(res, 'Assets deleted successfully');
      }

      case 'generate_lods': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'lodCount', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const lodCount = typeof params.lodCount === 'number' ? params.lodCount : Number(params.lodCount);
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          assetPath,
          lodCount,
          subAction: 'generate_lods'
        }) as AssetOperationResponse;
        return ResponseFactory.success(res, 'LODs generated successfully');
      }
      case 'create_thumbnail': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'width' },
          { key: 'height' }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const width = extractOptionalNumber(params, 'width');
        const height = extractOptionalNumber(params, 'height');
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          assetPath,
          width,
          height,
          subAction: 'generate_thumbnail'
        }) as AssetOperationResponse;
        return ResponseFactory.success(res, 'Thumbnail created successfully');
      }
      case 'set_tags': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'tags', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const tags = extractOptionalArray<string>(params, 'tags') ?? [];

        if (!assetPath) {
          return ResponseFactory.error('INVALID_ARGUMENT', 'assetPath is required');
        }

        // Note: Array.isArray check is unnecessary - extractOptionalArray always returns an array

        // Forward to C++ automation bridge which uses UEditorAssetLibrary::SetMetadataTag
        const res = await executeAutomationRequest(tools, 'set_tags', {
          assetPath,
          tags
        });
        return ResponseFactory.success(res, 'Tags set successfully');
      }
      case 'get_metadata': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          assetPath,
          subAction: 'get_metadata'
        }) as AssetOperationResponse;
        const tags = res.tags || {};
        const metadata = res.metadata || {};
        const merged = { ...tags, ...metadata };
        const tagCount = Object.keys(merged).length;

        const cleanRes = cleanObject(res);
        cleanRes.message = `Metadata retrieved (${tagCount} items)`;
        cleanRes.tags = tags;
        if (Object.keys(metadata).length > 0) {
          cleanRes.metadata = metadata;
        }

        return ResponseFactory.success(cleanRes, cleanRes.message as string);
      }
      case 'set_metadata': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'metadata', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const metadata = params.metadata as Record<string, unknown>;
        const res = await executeAutomationRequest(tools, 'set_metadata', { ...args, assetPath, metadata });
        return ResponseFactory.success(res, 'Metadata set successfully');
      }
      case 'validate':
      case 'validate_asset': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          assetPath,
          subAction: 'validate'
        }) as AssetOperationResponse;
        return ResponseFactory.success(res, 'Asset validation complete');
      }
      case 'generate_report': {
        const params = normalizeArgs(args, [
          { key: 'directory' },
          { key: 'reportType' },
          { key: 'outputPath' }
        ]);
        const directory = extractOptionalString(params, 'directory') ?? '';
        const reportType = extractOptionalString(params, 'reportType');
        const outputPath = extractOptionalString(params, 'outputPath');
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          directory,
          reportType,
          outputPath,
          subAction: 'generate_report'
        }) as AssetOperationResponse;
        return ResponseFactory.success(res, 'Report generated successfully');
      }
      case 'create_material_instance': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'parentMaterial', required: true },
          { key: 'savePath', aliases: ['path'] }
        ]);
        const name = extractString(params, 'name');
        const parentMaterial = extractString(params, 'parentMaterial');
        const savePath = extractOptionalString(params, 'savePath');
        
        const res = await executeAutomationRequest(
          tools,
          'create_material_instance',
          { ...args, name, parentMaterial, savePath },
          'Automation bridge not available for create_material_instance'
        ) as AssetOperationResponse;

        const result = res ?? {};
        const errorCode = typeof result.error === 'string' ? result.error.toUpperCase() : '';
        const message = typeof result.message === 'string' ? result.message : '';
        const argsTyped = args as AssetArgs;

        if (errorCode === 'PARENT_NOT_FOUND' || message.toLowerCase().includes('parent material not found')) {
          // Keep specific error structure for this business logic case
          return cleanObject({
            success: false,
            error: 'PARENT_NOT_FOUND',
            message: message || 'Parent material not found',
            path: (result as Record<string, unknown>).path,
            parentMaterial: argsTyped.parentMaterial
          });
        }

        return ResponseFactory.success(res, 'Material instance created successfully');
      }
      case 'search_assets': {
        const params = normalizeArgs(args, [
          { key: 'searchText' },
          { key: 'classNames' },
          { key: 'packagePaths' },
          { key: 'recursivePaths' },
          { key: 'recursiveClasses' },
          { key: 'limit' },
          { key: 'offset' }
        ]);
        const searchText = extractOptionalString(params, 'searchText');
        const classNames = extractOptionalArray<string>(params, 'classNames');
        const packagePaths = extractOptionalArray<string>(params, 'packagePaths');
        // When searchText is provided, default recursivePaths to true for broad search
        const recursivePaths = extractOptionalBoolean(params, 'recursivePaths') ?? (searchText ? true : undefined);
        const recursiveClasses = extractOptionalBoolean(params, 'recursiveClasses');
        const limit = extractOptionalNumber(params, 'limit');
        const offset = extractOptionalNumber(params, 'offset');

        // SECURITY: Validate packagePaths for traversal attempts
        const pathSecurityError = validatePathsSecurity(packagePaths, 'packagePaths');
        if (pathSecurityError) return pathSecurityError;

        const res = await executeAutomationRequest(tools, 'asset_query', {
          searchText,
          classNames,
          packagePaths,
          recursivePaths,
          recursiveClasses,
          limit,
          offset,
          subAction: 'search_assets'
        }) as AssetOperationResponse;
        return ResponseFactory.success(res, 'Assets found');
      }
      case 'find_by_tag': {
        const params = normalizeArgs(args, [
          { key: 'tag', required: true },
          { key: 'value' }
        ]);
        const tag = extractString(params, 'tag');
        const value = extractOptionalString(params, 'value');

        // SECURITY: Validate any path parameters for traversal attempts
        // The test harness may pass 'assetPath' or 'path' with traversal payload
        const argsTyped = args as AssetArgs;
        const assetPathCheck = validatePathSecurity(argsTyped.assetPath, 'assetPath');
        if (assetPathCheck) return assetPathCheck;
        const pathCheck = validatePathSecurity(argsTyped.path, 'path');
        if (pathCheck) return pathCheck;

        const res = await executeAutomationRequest(tools, 'asset_query', {
          tag,
          value,
          subAction: 'find_by_tag'
        }) as AssetOperationResponse;
        return ResponseFactory.success(res, 'Assets found by tag');
      }
      case 'get_dependencies': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'recursive' }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const recursive = extractOptionalBoolean(params, 'recursive');
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          assetPath,
          recursive,
          subAction: 'get_dependencies'
        }) as AssetOperationResponse;
        return ResponseFactory.success(res, 'Dependencies retrieved');
      }
      case 'get_source_control_state': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const res = await executeAutomationRequest(tools, 'asset_query', {
          assetPath,
          subAction: 'get_source_control_state'
        }) as AssetOperationResponse;
        return ResponseFactory.success(res, 'Source control state retrieved');
      }
      case 'source_control_enable': {
        // Enable source control by specifying a provider (perforce, svn, etc.)
        const params = normalizeArgs(args, [
          { key: 'provider', default: 'None' }
        ]);
        const provider = extractOptionalString(params, 'provider') ?? 'None';
        const res = await executeAutomationRequest(tools, 'source_control_enable', {
          provider
        });
        return ResponseFactory.success(res, 'Source control enabled');
      }
      case 'analyze_graph': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'maxDepth' }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const maxDepth = extractOptionalNumber(params, 'maxDepth');
        const res = await executeAutomationRequest(tools, 'analyze_graph', {
          assetPath,
          maxDepth
        });
        return ResponseFactory.success(res, 'Graph analysis complete');
      }
      case 'create_render_target': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'packagePath', aliases: ['path'], default: '/Game' },
          { key: 'width' },
          { key: 'height' },
          { key: 'format' }
        ]);
        const name = extractString(params, 'name');
        const packagePath = extractOptionalString(params, 'packagePath') ?? '/Game';
        const width = extractOptionalNumber(params, 'width');
        const height = extractOptionalNumber(params, 'height');
        const format = extractOptionalString(params, 'format');
        const res = await executeAutomationRequest(tools, 'manage_render', {
          subAction: 'create_render_target',
          name,
          packagePath,
          width,
          height,
          format,
          save: true
        });
        return ResponseFactory.success(res, 'Render target created successfully');
      }
      case 'nanite_rebuild_mesh': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['meshPath'], required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const res = await executeAutomationRequest(tools, 'manage_render', {
          subAction: 'nanite_rebuild_mesh',
          assetPath
        });
        return ResponseFactory.success(res, 'Nanite mesh rebuilt successfully');
      }
      case 'fixup_redirectors': {
        const argsTyped = args as AssetArgs;
        const directoryRaw = typeof argsTyped.directory === 'string' && argsTyped.directory.trim().length > 0
          ? argsTyped.directory.trim()
          : (typeof argsTyped.directoryPath === 'string' && argsTyped.directoryPath.trim().length > 0
            ? argsTyped.directoryPath.trim()
            : '');

        // Pass all args through to C++ handler, with normalized directoryPath
        const payload: Record<string, unknown> = { ...args };
        if (directoryRaw) {
          payload.directoryPath = directoryRaw;
        }

        const res = await executeAutomationRequest(tools, 'fixup_redirectors', payload);
        return ResponseFactory.success(res, 'Redirectors fixed up successfully');
      }
      case 'add_material_parameter': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'parameterName', aliases: ['name'], required: true },
          { key: 'parameterType', aliases: ['type'] },
          { key: 'value', aliases: ['defaultValue'] }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const parameterName = extractString(params, 'parameterName');
        const parameterType = extractOptionalString(params, 'parameterType');
        const value = params.value;
        const res = await executeAutomationRequest(tools, 'add_material_parameter', {
          assetPath,
          name: parameterName,
          type: parameterType,
          value
        });
        return ResponseFactory.success(res, 'Material parameter added successfully');
      }
      case 'list_instances': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const res = await executeAutomationRequest(tools, 'list_instances', {
          assetPath
        });
        return ResponseFactory.success(res, 'Instances listed successfully');
      }
      case 'reset_instance_parameters': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const res = await executeAutomationRequest(tools, 'reset_instance_parameters', {
          assetPath
        });
        return ResponseFactory.success(res, 'Instance parameters reset successfully');
      }
      case 'exists': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const res = await executeAutomationRequest(tools, 'exists', {
          assetPath
        });
        return ResponseFactory.success(res, 'Asset existence check complete');
      }
      case 'get_material_stats': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const res = await executeAutomationRequest(tools, 'get_material_stats', {
          assetPath
        });
        return ResponseFactory.success(res, 'Material stats retrieved');
      }
      case 'add_material_node': {
        const materialNodeAliases: Record<string, string> = {
          'Multiply': 'MaterialExpressionMultiply',
          'Add': 'MaterialExpressionAdd',
          'Subtract': 'MaterialExpressionSubtract',
          'Divide': 'MaterialExpressionDivide',
          'Power': 'MaterialExpressionPower',
          'Clamp': 'MaterialExpressionClamp',
          'Constant': 'MaterialExpressionConstant',
          'Constant2Vector': 'MaterialExpressionConstant2Vector',
          'Constant3Vector': 'MaterialExpressionConstant3Vector',
          'Constant4Vector': 'MaterialExpressionConstant4Vector',
          'TextureSample': 'MaterialExpressionTextureSample',
          'TextureCoordinate': 'MaterialExpressionTextureCoordinate',
          'Panner': 'MaterialExpressionPanner',
          'Rotator': 'MaterialExpressionRotator',
          'Lerp': 'MaterialExpressionLinearInterpolate',
          'LinearInterpolate': 'MaterialExpressionLinearInterpolate',
          'Sine': 'MaterialExpressionSine',
          'Cosine': 'MaterialExpressionCosine',
          'Append': 'MaterialExpressionAppendVector',
          'AppendVector': 'MaterialExpressionAppendVector',
          'ComponentMask': 'MaterialExpressionComponentMask',
          'Fresnel': 'MaterialExpressionFresnel',
          'Time': 'MaterialExpressionTime',
          'ScalarParameter': 'MaterialExpressionScalarParameter',
          'VectorParameter': 'MaterialExpressionVectorParameter',
          'StaticSwitchParameter': 'MaterialExpressionStaticSwitchParameter'
        };

        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'nodeType', aliases: ['type'], required: true, map: materialNodeAliases },
          { key: 'posX' },
          { key: 'posY' },
          { key: 'x' },
          { key: 'y' },
          { key: 'placementMode' },
          { key: 'direction' },
          { key: 'anchorExpressionIndex' },
          { key: 'anchorExpressionPath' },
          { key: 'anchorNodeId' },
          { key: 'avoidOverlap' },
          { key: 'placement' }
        ]);

        const assetPath = extractString(params, 'assetPath');
        const nodeType = extractString(params, 'nodeType');
        const posX = extractOptionalNumber(params, 'posX');
        const posY = extractOptionalNumber(params, 'posY');
        const x = extractOptionalNumber(params, 'x');
        const y = extractOptionalNumber(params, 'y');
        const placementMode = extractOptionalString(params, 'placementMode');
        const direction = extractOptionalString(params, 'direction');
        const anchorExpressionIndex = extractOptionalNumber(params, 'anchorExpressionIndex');
        const anchorExpressionPath = extractOptionalString(params, 'anchorExpressionPath');
        const anchorNodeId = extractOptionalString(params, 'anchorNodeId');
        const avoidOverlap = extractOptionalBoolean(params, 'avoidOverlap');

        const res = await executeAutomationRequest(tools, 'add_material_node', {
          assetPath,
          nodeType,
          posX,
          posY,
          x,
          y,
          placementMode,
          direction,
          anchorExpressionIndex,
          anchorExpressionPath,
          anchorNodeId,
          avoidOverlap,
          placement: (params as Record<string, unknown>).placement
        });
        return ResponseFactory.success(res, 'Material node added successfully');
      }
      case 'set_material_node_position':
      case 'move_material_node':
      case 'bulk_set_material_node_positions':
      case 'bulk_move_material_nodes':
      case 'create_material_comment':
      case 'wrap_material_nodes_in_comment':
      case 'create_named_reroute':
      case 'use_named_reroute':
      case 'replace_long_connection_with_named_reroute':
      case 'align_material_nodes': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true }
        ]);
        const res = await executeAutomationRequest(tools, action, {
          ...args,
          ...params
        });
        return ResponseFactory.success(res, 'Material graph action executed successfully');
      }
      case 'connect_material_pins': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'sourceNodeId', aliases: ['sourceNode'] },
          { key: 'sourceExpressionIndex' },
          { key: 'sourceExpressionPath' },
          { key: 'sourcePin', aliases: ['fromPin', 'outputPin'] },
          { key: 'sourceOutputIndex' },
          { key: 'targetNodeId', aliases: ['targetNode'] },
          { key: 'targetExpressionIndex' },
          { key: 'targetExpressionPath' },
          { key: 'targetPin', aliases: ['toPin', 'inputPin', 'targetInputPin'], required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const sourceNodeId = extractOptionalString(params, 'sourceNodeId');
        const sourceExpressionIndex = extractOptionalNumber(params, 'sourceExpressionIndex');
        const sourceExpressionPath = extractOptionalString(params, 'sourceExpressionPath');
        const sourcePin = extractOptionalString(params, 'sourcePin');
        const sourceOutputIndex = extractOptionalNumber(params, 'sourceOutputIndex');
        const targetNodeId = extractOptionalString(params, 'targetNodeId');
        const targetExpressionIndex = extractOptionalNumber(params, 'targetExpressionIndex');
        const targetExpressionPath = extractOptionalString(params, 'targetExpressionPath');
        const targetPin = extractString(params, 'targetPin');
        const res = await executeAutomationRequest(tools, 'connect_material_pins', {
          assetPath,
          sourceNodeId,
          sourceExpressionIndex,
          sourceExpressionPath,
          sourcePin,
          sourceOutputIndex,
          targetNodeId,
          targetExpressionIndex,
          targetExpressionPath,
          targetPin
        });
        return ResponseFactory.success(res, 'Material pins connected successfully');
      }
      case 'remove_material_node': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'nodeId' },
          { key: 'expressionIndex' },
          { key: 'expressionPath' },
          { key: 'expressionName' }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const nodeId = extractOptionalString(params, 'nodeId');
        const expressionIndex = extractOptionalNumber(params, 'expressionIndex');
        const expressionPath = extractOptionalString(params, 'expressionPath');
        const expressionName = extractOptionalString(params, 'expressionName');
        const res = await executeAutomationRequest(tools, 'remove_material_node', {
          assetPath,
          nodeId,
          expressionIndex,
          expressionPath,
          expressionName
        });
        return ResponseFactory.success(res, 'Material node removed successfully');
      }
      case 'break_material_connections': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'nodeId' },
          { key: 'expressionIndex' },
          { key: 'expressionPath' },
          { key: 'expressionName' },
          { key: 'pinName' }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const nodeId = extractOptionalString(params, 'nodeId');
        const expressionIndex = extractOptionalNumber(params, 'expressionIndex');
        const expressionPath = extractOptionalString(params, 'expressionPath');
        const expressionName = extractOptionalString(params, 'expressionName');
        const pinName = extractOptionalString(params, 'pinName');
        const res = await executeAutomationRequest(tools, 'break_material_connections', {
          assetPath,
          nodeId,
          expressionIndex,
          expressionPath,
          expressionName,
          pinName
        });
        return ResponseFactory.success(res, 'Material connections broken successfully');
      }
      case 'get_material_node_details': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'nodeId', required: false },  // Optional - if not provided, lists all nodes
          { key: 'expressionIndex' },  // Alternative to nodeId - numeric index
          { key: 'expressionPath' },
          { key: 'expressionName' }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const nodeId = extractOptionalString(params, 'nodeId');
        const expressionIndex = extractOptionalNumber(params, 'expressionIndex');
        const expressionPath = extractOptionalString(params, 'expressionPath');
        const expressionName = extractOptionalString(params, 'expressionName');
        
        const res = await executeAutomationRequest(tools, 'get_material_node_details', {
          assetPath,
          nodeId,
          expressionIndex,
          expressionPath,
          expressionName
        });
        return ResponseFactory.success(res, 'Material node details retrieved');
      }
      case 'get_material_instance_info': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'includeEffective', default: true },
          { key: 'overriddenOnly', default: false }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const includeEffective = extractOptionalBoolean(params, 'includeEffective') ?? true;
        const overriddenOnly = extractOptionalBoolean(params, 'overriddenOnly') ?? false;
        const res = await executeAutomationRequest(tools, 'get_material_instance_info', {
          assetPath,
          includeEffective,
          overriddenOnly
        }) as AssetOperationResponse;
        if (res.success === false) {
          return ResponseFactory.errorWithCode(res.errorCode ?? res.error ?? 'OPERATION_FAILED', res.message ?? 'Failed to get material instance diagnostics');
        }
        return ResponseFactory.success(res, 'Material instance diagnostics retrieved');
      }
      case 'find_material_expressions': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'className', aliases: ['expressionClass'] },
          { key: 'parameterName' },
          { key: 'expressionName' },
          { key: 'expressionPath' },
          { key: 'expressionGuid' },
          { key: 'nodeId' },
          { key: 'desc' },
          // NEW3: forward includeDetails so C++ inlines per-entry typed details
          { key: 'includeDetails' }
        ]);
        const res = await executeAutomationRequest(tools, 'find_material_expressions', {
          ...params
        }) as AssetOperationResponse;
        if (res.success === false) {
          return ResponseFactory.errorWithCode(res.errorCode ?? res.error ?? 'OPERATION_FAILED', res.message ?? 'Failed to find material expressions');
        }
        return ResponseFactory.success(res, 'Material expressions found');
      }
      case 'get_material_expression_details':
      case 'get_material_expression_connections': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'nodeId' },
          { key: 'expressionIndex' },
          { key: 'expressionPath' },
          { key: 'expressionName' },
          { key: 'expressionGuid' },
          { key: 'parameterName' },
          { key: 'className', aliases: ['expressionClass'] },
          { key: 'includeConsumers', default: action === 'get_material_expression_connections' }
        ]);
        const res = await executeAutomationRequest(tools, action, {
          ...params
        }) as AssetOperationResponse;
        if (res.success === false) {
          return ResponseFactory.errorWithCode(res.errorCode ?? res.error ?? 'OPERATION_FAILED', res.message ?? `Failed to execute ${action}`);
        }
        return ResponseFactory.success(res, action === 'get_material_expression_connections'
          ? 'Material expression connections retrieved'
          : 'Material expression details retrieved');
      }
      case 'get_landscape_material_context': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'] },
          { key: 'actorName', aliases: ['landscapeName'] },
          { key: 'actorPath', aliases: ['landscapePath'] }
        ]);
        const res = await executeAutomationRequest(tools, 'get_landscape_material_context', {
          ...params
        }) as AssetOperationResponse;
        if (res.success === false) {
          return ResponseFactory.errorWithCode(res.errorCode ?? res.error ?? 'OPERATION_FAILED', res.message ?? 'Failed to get landscape material context');
        }
        return ResponseFactory.success(res, 'Landscape material context retrieved');
      }
      case 'compile_material_diagnostics': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'save', default: false }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const save = extractOptionalBoolean(params, 'save') ?? false;
        const res = await executeAutomationRequest(tools, 'compile_material_diagnostics', {
          assetPath,
          save
        }) as AssetOperationResponse;
        if (res.success === false) {
          return ResponseFactory.errorWithCode(res.errorCode ?? res.error ?? 'OPERATION_FAILED', res.message ?? 'Failed to compile material diagnostics');
        }
        return ResponseFactory.success(res, 'Material compile diagnostics retrieved');
      }
      case 'rebuild_material': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const res = await executeAutomationRequest(tools, 'rebuild_material', {
          assetPath
        });
        return ResponseFactory.success(res, 'Material rebuilt successfully');
      }
      case 'bulk_rename': {
        // Accept either folderPath or assetPaths
        // Map pattern->searchText and replacement->replaceText for C++ compatibility
        const argsTyped = args as AssetArgs;
        const folderPath = argsTyped.folderPath ?? argsTyped.path;
        const assetPaths = argsTyped.assetPaths ?? argsTyped.paths;

        // SECURITY: Validate folderPath for traversal attempts
        const folderPathSecurity = validatePathSecurity(
          typeof folderPath === 'string' ? folderPath : undefined, 'folderPath'
        );
        if (folderPathSecurity) return folderPathSecurity;

        // SECURITY: Validate path parameter for traversal attempts (test may pass 'path')
        const pathParamSecurity = validatePathSecurity(
          typeof argsTyped.path === 'string' ? argsTyped.path : undefined, 'path'
        );
        if (pathParamSecurity) return pathParamSecurity;

        // SECURITY: Validate assetPaths array for traversal attempts
        const assetPathsSecurity = validatePathsSecurity(assetPaths, 'assetPaths');
        if (assetPathsSecurity) return assetPathsSecurity;
        
        if (!folderPath && (!assetPaths || (Array.isArray(assetPaths) && assetPaths.length === 0))) {
          return ResponseFactory.error('INVALID_ARGUMENT', 'Either folderPath or assetPaths is required for bulk_rename');
        }
        
        const res = await executeAutomationRequest(tools, 'bulk_rename', {
          folderPath,
          assetPaths,
          searchText: argsTyped.pattern,
          replaceText: argsTyped.replacement,
          prefix: argsTyped.prefix,
          suffix: argsTyped.suffix
        });
        return ResponseFactory.success(res, 'Bulk rename completed');
      }
      case 'bulk_delete': {
        // Accept either folderPath or assetPaths
        const argsTyped = args as AssetArgs;
        const folderPath = argsTyped.folderPath ?? argsTyped.path;
        const assetPaths = argsTyped.assetPaths ?? argsTyped.paths;
        
        if (!folderPath && (!assetPaths || (Array.isArray(assetPaths) && assetPaths.length === 0))) {
          return ResponseFactory.error('INVALID_ARGUMENT', 'Either folderPath or assetPaths is required for bulk_delete');
        }
        
        const res = await executeAutomationRequest(tools, 'bulk_delete', {
          ...args,
          folderPath,
          assetPaths
        });
        return ResponseFactory.success(res, 'Bulk delete completed');
      }
      // ===== N3: get_set_material_attributes_overrides — passthrough to bridge =====
      case 'get_set_material_attributes_overrides': {
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          ...args,
          subAction: action
        }) as AssetOperationResponse;
        if (res.success === false) {
          return ResponseFactory.errorWithCode(res.errorCode ?? res.error ?? 'OPERATION_FAILED', res.message ?? `Failed: ${action}`);
        }
        return ResponseFactory.success(res, res.message ?? `${action} succeeded`);
      }
      // ===== R10: bulk_get_material_expression_details — identifier-kind validation + passthrough =====
      case 'bulk_get_material_expression_details': {
        const a = args as Record<string, unknown>;
        const kinds = ['indices', 'guids', 'nodeIds'].filter(k => Array.isArray(a[k]) && (a[k] as unknown[]).length > 0);
        if (kinds.length !== 1) {
          throw new Error('bulk_get_material_expression_details: provide exactly one non-empty array of indices, guids, or nodeIds');
        }
        return await executeAutomationRequest(
          tools,
          'manage_asset',
          { ...args, subAction: action }
        ) as Record<string, unknown>;
      }
      default: {
        // Pass through to C++ for any subAction. Native FMcpToolRegistry's
        // manage_asset schema is the source of truth for valid actions; if
        // the action is unsupported, C++ returns its own UNKNOWN_SUBACTION
        // error. The old TS-side allowlist (VALID_ASSET_ACTIONS) silently
        // rejected actions C++ DID support, so it's been removed.
        const res = await executeAutomationRequest(tools, 'manage_asset', { ...args, subAction: action }) as AssetOperationResponse;
        const result = res ?? {};
        const errorCode = typeof result.error === 'string' ? result.error.toUpperCase() : '';
        const message = typeof result.message === 'string' ? result.message : '';
        const argsTyped = args as AssetArgs;

        // Check for unknown/invalid action errors from C++ (UNKNOWN_ACTION or INVALID_SUBACTION)
        if (errorCode === 'UNKNOWN_ACTION' || errorCode === 'INVALID_SUBACTION' ||
            message.toLowerCase().includes('unknown action') || message.toLowerCase().includes('unknown subaction')) {
          return cleanObject({
            success: false,
            error: 'UNKNOWN_ACTION',
            message: `Unknown asset action: ${action}`,
            action: action || 'manage_asset',
            assetPath: argsTyped.assetPath ?? argsTyped.path
          });
        }

        // CRITICAL FIX: Check if C++ returned success=false and pass it through
        // This prevents false positives where TS wraps a failed C++ response as success
        if (typeof result.success === 'boolean' && result.success === false) {
          return cleanObject({
            success: false,
            error: errorCode || 'OPERATION_FAILED',
            message: message || 'Asset operation failed',
            action: action || 'manage_asset',
            assetPath: argsTyped.assetPath ?? argsTyped.path,
            data: result
          });
        }

        return ResponseFactory.success(res, 'Asset action executed successfully');
      }
    }
  } catch (error) {
    return ResponseFactory.error(error);
  }
}
