#!/usr/bin/env node
// scripts/rebuild-mcp.mjs
//
// Single-shot pipeline to refresh the stdio MCP server after a C++ tool change.
// Run order:
//   1. Regenerate src/tools/consolidated-tool-definitions.ts from generated/tool-manifest.json
//   2. tsc -p tsconfig.json  (type-check + emit dist/)
//   3. lint:tool-defs (warnings only, non-blocking)
//
// PREREQUISITE: Run the DumpMcpManifest commandlet first, in your own terminal
// from the project root (Claude Code's sandbox cannot run commandlets):
//   python Scripts/run-cmd.py DumpMcpManifest --skip-build
//
// That refreshes generated/tool-manifest.json from the live C++ registry.
// This script picks it up and produces a working dist/cli.js for stdio MCP.

import { execSync } from 'node:child_process';
import { existsSync, statSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const manifestPath = resolve(root, 'generated/tool-manifest.json');
const cliPath = resolve(root, 'dist/cli.js');

function step(label, cmd) {
  console.log(`\n--- ${label} ---`);
  execSync(cmd, { stdio: 'inherit', cwd: root });
}

if (!existsSync(manifestPath)) {
  console.error(`[ERROR] generated/tool-manifest.json not found at ${manifestPath}`);
  console.error('Refresh it first (run from project root, in your own terminal):');
  console.error('  python Scripts/run-cmd.py DumpMcpManifest --skip-build');
  process.exit(1);
}

const manifestMtime = statSync(manifestPath).mtime;
console.log(`Using manifest: ${manifestPath}`);
console.log(`  manifest mtime: ${manifestMtime.toISOString()}`);

try {
  step('1/3 Regenerate consolidated tool defs', 'node scripts/generate-tool-defs.mjs');
  step('2/3 Build TypeScript (type-check + emit dist/)', 'npx tsc -p tsconfig.json');
  step('3/3 Lint tool defs (warnings only, non-blocking)', 'node scripts/lint-tool-defs.mjs');
} catch (err) {
  console.error(`\n[ERROR] Pipeline failed at step: ${err.message ?? err}`);
  process.exit(1);
}

if (!existsSync(cliPath)) {
  console.error(`\n[ERROR] ${cliPath} missing after build`);
  process.exit(1);
}

const cliStat = statSync(cliPath);
console.log('\n[OK] stdio MCP ready');
console.log(`     ${cliPath}`);
console.log(`     size=${cliStat.size} bytes  mtime=${cliStat.mtime.toISOString()}`);
console.log('     Reference in .mcp.json:  "args": ["./Plugins/Unreal_mcp/dist/cli.js"]');
