#!/usr/bin/env node
// CLI: node scripts/generate-tool-defs.mjs
//   [--manifest <path-to-tool-manifest.json>]
//   [--out <path-to-consolidated-tool-definitions.ts>]
//
// Defaults assume the script runs from the plugin root (Plugins/Unreal_mcp/).

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { generateToolDefsSource } from './lib/generate-tool-defs-source.mjs';

function arg(flag, fallback) {
  const i = process.argv.indexOf(flag);
  if (i === -1) return fallback;
  return process.argv[i + 1];
}

const __dirname = dirname(fileURLToPath(import.meta.url));
const pluginRoot = resolve(__dirname, '..');
const manifestPath = resolve(pluginRoot, arg('--manifest', 'generated/tool-manifest.json'));
const outPath = resolve(pluginRoot, arg('--out', 'src/tools/consolidated-tool-definitions.ts'));

const raw = readFileSync(manifestPath, 'utf8');
const manifest = JSON.parse(raw);
const source = generateToolDefsSource(manifest);

mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, source, 'utf8');

const compactSize = JSON.stringify(manifest).length;
console.log(`gen:tool-defs wrote ${manifest.length} tools to ${outPath}`);
console.log(`manifest size (compact): ${compactSize} bytes`);
