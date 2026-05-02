# PROJECT KNOWLEDGE BASE

**Generated:** 2026-02-24 19:23:00 IST
**Commit:** 02f9b17
**Branch:** main

## OVERVIEW
MCP server for Unreal Engine 5 (5.0-5.7). Dual-process: TypeScript MCP server + C++ Bridge Plugin. 36 consolidated tools with action-based dispatch. Version 0.5.18.
## STRUCTURE
```
./
├── src/                    # TS Server (NodeNext ESM)
│   ├── tools/              # Tool definitions + handlers
│   │   ├── consolidated-tool-definitions.ts  # AUTO-GENERATED from generated/tool-manifest.json — DO NOT EDIT
│   │   ├── consolidated-tool-handlers.ts     # Tool routing
│   │   └── handlers/       # Domain handlers (40 files)
│   ├── automation/         # Bridge Client & Handshake (9 files)
│   ├── utils/              # Normalization & Security
├── generated/              # Committed codegen artifacts
│   └── tool-manifest.json  # Native-side tool schemas (source of truth)
├── config/
│   └── default-enabled-tools.json  # TS-side default-enabled tool set
├── plugins/                # UE Plugin (C++)
│       ├── Source/         # Native Handlers (56 files) & Subsystem
│       └── Config/         # Plugin Settings
├── tests/                  # Integration + Unit Tests
│   ├── test-runner.mjs     # Custom MCP test runner (1100+ lines)
│   └── mcp-tools/          # Domain-specific test files (core/world/authoring/gameplay/utility)
└── scripts/                # Maintenance & CI Helpers
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Add MCP Tool | `plugins/.../McpTool_*.cpp` | Native is source of truth; re-run `python Scripts/run-cmd.py DumpMcpManifest && npm run gen:tool-defs` |
| Route Tool | `src/tools/consolidated-tool-handlers.ts` | Register in `registerDefaultHandlers()` |
| Implement Handler | `src/tools/handlers/*-handlers.ts` | Call `executeAutomationRequest()` |
| Add UE Action | `plugins/.../Private/*Handlers.cpp` | Register in `Subsystem::InitializeHandlers()` |
| Fix UE Crashes | `McpAutomationBridgeHelpers.h` | Use `McpSafeAssetSave` for 5.7+ |
| Path Handling | `src/utils/normalize.ts` | Force `/Game/` prefix |
| CI Workflows | `.github/workflows/` | All actions use commit SHAs (secure) |
| Version Sync | `.github/workflows/bump-version.yml` | Updates 4 files atomically |
| Change default enabled tools | `config/default-enabled-tools.json` | Filters `tools/list` at startup |
| Regenerate tool schemas | `python Scripts/run-cmd.py DumpMcpManifest && npm run gen:tool-defs` | After any C++ tool change |
| Audit tool-defs quality | `npm run lint:tool-defs` | Lists missing descriptions and similar issues |

## CONVENTIONS
### Dual-Process Flow
0. **Schema Definition**: C++ `McpTool_*.cpp` declares each tool via `FMcpToolDefinition` + `FMcpSchemaBuilder` → `FMcpToolRegistry` → `DumpMcpManifest` commandlet → `npm run gen:tool-defs`. Native is the single source of truth for `name/description/inputSchema/category/annotations`.
1. **TS (MCP)**: Validates JSON Schema → Executes Tool Handler.
2. **Bridge (WS)**: TS sends JSON payload → C++ Subsystem dispatches to Game Thread.
3. **Execution**: C++ handler performs native UE API calls → Returns JSON result.

### UE 5.7 Safety
- **NO `UPackage::SavePackage()`**: Causes access violations in 5.7. Use `McpSafeAssetSave`.
- **SCS Ownership**: Component templates must be created via `SCS->CreateNode()` and `AddNode()`.
- **`ANY_PACKAGE`**: Deprecated. Use `nullptr` for path lookups.

### TypeScript Standards
- **Zero-Any Policy**: Strictly no `as any` in runtime code. Use `unknown` or interfaces.
- **Strict Mode**: Full TypeScript strict mode enabled (all checks).
- **Colocate Tests**: Unit tests (`.test.ts`) with source, integration in `tests/`.

## ANTI-PATTERNS
- **Console Hacks**: Never use `scripts/remove-saveasset.py` (legacy).
- **Hardcoded Paths**: Avoid `X:\` or `C:\` absolute paths in scripts.
- **Breaking STDOUT**: Never `console.log` in runtime (JSON-RPC only).
- **Incomplete Tools**: No "Not Implemented" stubs. 100% TS + C++ coverage required.
- **Bypass Registry**: Always use `toolRegistry.register()`, never call handlers directly.
- **Raw WS Calls**: Use `executeAutomationRequest()` instead of WebSocket directly.
- **Hand-edit Tool Schemas**: Never modify `consolidated-tool-definitions.ts` directly — it is regenerated from `generated/tool-manifest.json`. Edit the corresponding `McpTool_*.cpp` and re-run codegen.

## UNIQUE STYLES
- **Consolidated Tools**: 36 tools with action-based dispatch (single schema file).
- **Dual Test Runners**: Vitest (unit) + Custom MCP runner (integration).
- **Mock Mode**: Set `MOCK_UNREAL_CONNECTION=true` for offline CI.
- **Non-Standard Layout**: `src/tools/handlers/` nested 2 levels deep.

## COMMANDS
```bash
npm run build:core      # Build TypeScript
npm run test:unit       # Vitest unit tests
npm test                # UE Integration (Requires Editor)
npm run test:smoke      # Mock mode smoke test
npm run gen:tool-defs   # Regenerate consolidated-tool-definitions.ts from manifest
npm run lint:tool-defs  # Audit tool manifest for missing descriptions
```

## NOTES
- **Engine Reference**: Check engine code at `X:\Unreal_Engine\UE_5.7\Engine`, `UE_5.6`, `UE_5.5`, `UE_5.4`, `UE_5.3`, `UE_5.2`, `UE_5.1`, `UE_5.0`.
- **Version Files**: Version in `package.json`, `server.json`, `src/index.ts`.
- **Test Patterns**: Integration tests use pipe-separated expectations (`success|error|timeout`).