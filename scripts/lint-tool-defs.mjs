#!/usr/bin/env node
// CLI: node scripts/lint-tool-defs.mjs [--manifest <path>]
// Always exits 0; prints warnings grouped by tool.

import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { lintToolManifest } from './lib/lint-tool-defs.mjs';

function arg(flag, fallback) {
  const i = process.argv.indexOf(flag);
  if (i === -1) return fallback;
  return process.argv[i + 1];
}

const __dirname = dirname(fileURLToPath(import.meta.url));
const pluginRoot = resolve(__dirname, '..');
const manifestPath = resolve(pluginRoot, arg('--manifest', 'generated/tool-manifest.json'));

const manifest = JSON.parse(readFileSync(manifestPath, 'utf8'));
const warnings = lintToolManifest(manifest);

const byTool = new Map();
for (const w of warnings) {
  const key = w.tool;
  if (!byTool.has(key)) byTool.set(key, []);
  byTool.get(key).push(w);
}

for (const [tool, toolWarnings] of byTool) {
  for (const w of toolWarnings) {
    const where = w.property ? ` property "${w.property}"` : '';
    console.log(`WARN ${tool}:${where} ${w.kind}`);
  }
}

console.log(`Total: ${warnings.length} warnings across ${byTool.size} tools`);
process.exit(0);
