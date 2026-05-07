#!/usr/bin/env node
// scripts/rebuild-mcp.mjs
//
// Single-shot pipeline to refresh the stdio MCP server after a C++ tool change.
// Run order:
//   1. Run DumpMcpManifest from the Unreal project root
//   2. Regenerate src/tools/consolidated-tool-definitions.ts from generated/tool-manifest.json
//   3. Verify generated TypeScript tool definitions match the manifest
//   4. tsc -p tsconfig.json  (type-check + emit dist/)
//   5. lint:tool-defs

import { execFileSync, execSync } from 'node:child_process';
import { existsSync, readdirSync, statSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import dotenv from 'dotenv';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const manifestPath = resolve(root, 'generated/tool-manifest.json');
const cliPath = resolve(root, 'dist/cli.js');

dotenv.config({ path: resolve(root, '.env'), quiet: true });

function arg(flag, fallback = undefined) {
  const i = process.argv.indexOf(flag);
  if (i === -1) return fallback;
  return process.argv[i + 1];
}

function step(label, cmd, cwd = root) {
  console.log(`\n--- ${label} ---`);
  execSync(cmd, { stdio: 'inherit', cwd });
}

function findProjectFile(startDir) {
  let current = resolve(startDir);
  while (true) {
    const uproject = readdirSync(current).find((entry) => entry.endsWith('.uproject'));
    if (uproject) {
      return resolve(current, uproject);
    }

    const parent = dirname(current);
    if (parent === current) {
      return undefined;
    }
    current = parent;
  }
}

function resolveProjectFile() {
  const configured = arg('--project', process.env.UE_PROJECT_PATH);
  const projectPath = configured ? resolve(configured) : findProjectFile(root);
  if (!projectPath || !existsSync(projectPath)) {
    throw new Error('Unreal project not found. Pass --project <path-to.uproject> or set UE_PROJECT_PATH.');
  }
  return projectPath;
}

function resolveUnrealEditorCmd() {
  const configured = arg('--unreal-editor-cmd', process.env.UE_EDITOR_CMD ?? process.env.UNREAL_EDITOR_CMD);
  if (configured) {
    const cmdPath = resolve(configured);
    if (!existsSync(cmdPath)) {
      throw new Error(`UnrealEditor-Cmd not found at ${cmdPath}`);
    }
    return cmdPath;
  }

  const engineRoot = arg('--engine-root', process.env.UE_ENGINE_ROOT ?? process.env.UNREAL_ENGINE_ROOT);
  if (engineRoot) {
    const cmdPath = resolve(engineRoot, 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe');
    if (!existsSync(cmdPath)) {
      throw new Error(`UnrealEditor-Cmd not found under engine root ${engineRoot}`);
    }
    return cmdPath;
  }

  throw new Error('UnrealEditor-Cmd not configured. Pass --unreal-editor-cmd <path> or set UE_EDITOR_CMD / UE_ENGINE_ROOT.');
}

function runDumpMcpManifest() {
  const projectFile = resolveProjectFile();
  const editorCmd = resolveUnrealEditorCmd();
  const outputArg = `-Output=${manifestPath}`;

  console.log(`Unreal project: ${projectFile}`);
  console.log(`UnrealEditor-Cmd: ${editorCmd}`);
  console.log(`Manifest output: ${manifestPath}`);

  execFileSync(editorCmd, [projectFile, '-run=DumpMcpManifest', outputArg], {
    stdio: 'inherit',
    cwd: dirname(projectFile),
  });
}

console.log(`Using MCP root: ${root}`);

try {
  console.log('\n--- 1/5 Dump native MCP manifest ---');
  runDumpMcpManifest();

  if (!existsSync(manifestPath)) {
    throw new Error(`generated/tool-manifest.json not found at ${manifestPath}`);
  }

  const manifestMtime = statSync(manifestPath).mtime;
  console.log(`Using manifest: ${manifestPath}`);
  console.log(`  manifest mtime: ${manifestMtime.toISOString()}`);

  step('2/5 Regenerate consolidated tool defs', 'node scripts/generate-tool-defs.mjs');
  step('3/5 Verify consolidated tool defs', 'node scripts/generate-tool-defs.mjs --check');
  step('4/5 Build TypeScript (type-check + emit dist/)', 'npx tsc -p tsconfig.json');
  step('5/5 Lint tool defs', 'node scripts/lint-tool-defs.mjs');
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
