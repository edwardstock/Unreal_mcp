# Plugins/McpAutomationBridge

Native C++ Automation Bridge for Unreal Engine 5.0-5.7.

## OVERVIEW
Editor-only UE subsystem executing automation requests received via WebSocket. 56 handler files, 70 C++ source files total.

## STRUCTURE
```
Source/McpAutomationBridge/
├── Public/
│   ├── McpAutomationBridgeSubsystem.h  # Main subsystem, handler declarations
│   └── McpAutomationBridgeSettings.h   # Host/Port/Token config
├── Private/
│   ├── McpAutomationBridgeSubsystem.cpp    # Initialize, tick, dispatch
│   ├── McpAutomationBridge_ProcessRequest.cpp  # Request routing
│   ├── *Handlers.cpp                       # Action implementations (56 files)
│   └── McpAutomationBridgeHelpers.h        # Critical UE 5.7 safety helpers
└── McpAutomationBridge.Build.cs
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Add handler | `*Handlers.cpp` | Declare in `Subsystem.h`, register in `InitializeHandlers()` |
| Save Asset | `McpSafeAssetSave(Asset)` | Use helper in `McpAutomationBridgeHelpers.h` |
| Component creation | `SCS->CreateNode()` | Use proper SCS ownership for UE 5.7 |
| JSON Parsing | `FJsonObjectConverter` | UE standard for Struct ↔ JSON |
| Path Security | `SanitizeProjectRelativePath()` | Block traversal attacks |

## CONVENTIONS
- **Game Thread Safety**: Handlers dispatched to game thread automatically by subsystem.
- **UE 5.7+ SCS**: Component templates owned by `SCS_Node`, not Blueprint.
- **Safe Saving**: NEVER use `UPackage::SavePackage()`. Use `McpSafeAssetSave`.
- **ANY_PACKAGE**: Deprecated. Use `nullptr` for path-based object lookups.

## ANTI-PATTERNS
- **Modal Dialogs**: Avoid `UEditorAssetLibrary::SaveAsset()` on new assets (crashes D3D12).
- **Hardcoded Paths**: Do not use absolute Windows paths in handlers.
- **Blocking Thread**: WebSocket frame processing must not block game thread.

## File organization

One domain per file. Don't dump everything into a single mega-handler file.
The `McpAutomationBridge_AssetWorkflowHandlers.cpp` 7000-line monolith
mixing asset CRUD with material graph operations is the anti-pattern
this convention exists to prevent.

**File naming:** `McpAutomationBridge_<ToolName>_<Domain>.cpp` for native
handlers; one TS handler file per tool under `src/tools/handlers/`.

**Soft size guideline: 3000 lines.** When a file approaches that size with
one more handler, split before adding.

**Scope rule for refactors:** apply this convention to files you touch in
a given change, not to the whole codebase at once.

The TS-side `consolidated-tool-definitions.ts` is auto-generated from native
`McpTool_*.cpp` schemas via `npm run mcp:rebuild` — exempt from manual-split
rules, never hand-edited. The hand-edited dispatch file is
`consolidated-tool-handlers.ts` (different file).
