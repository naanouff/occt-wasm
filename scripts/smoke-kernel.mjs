#!/usr/bin/env node
/**
 * Smoke occt-kernel: box → tessellate → triangleCount > 0.
 * Writes dist/GAPS-kernel.md with init time and wasm size.
 */
import { writeFile, stat } from 'node:fs/promises';
import { dirname, join } from 'node:path';
import { pathToFileURL } from 'node:url';

const dist = join(dirname(new URL(import.meta.url).pathname), '..', 'dist');
const jsPath = join(dist, 'occt-kernel.js');
const wasmPath = join(dist, 'occt-kernel.wasm');
const libsPath = join(dist, 'link-libs.txt');

const { readFile } = await import('node:fs/promises');
const libs = await readFile(libsPath, 'utf8');
if (/TKOpenGl/i.test(libs)) {
  console.error('link-libs.txt contains TKOpenGl');
  process.exit(1);
}

const started = Date.now();
const moduleFactory = (await import(pathToFileURL(jsPath).href)).default;
const occt = await moduleFactory({
  locateFile: (path) => (path.endsWith('.wasm') ? wasmPath : path),
});
const initMs = Date.now() - started;

const required = [
  '_occt_make_box',
  '_occt_make_cylinder',
  '_occt_make_sphere',
  '_occt_make_cone',
  '_occt_tessellate',
  '_occt_shape_release',
  '_occt_arena_clear',
  '_occt_free',
];
for (const name of required) {
  if (typeof occt[name] !== 'function') {
    console.error(`occt-kernel.js does not export ${name}`);
    process.exit(1);
  }
}

const gaps = [];
const box = occt._occt_make_box(10, 20, 30);
if (!box) gaps.push('make_box returned 0');

const preset = new TextEncoder().encode('normal');
const presetPtr = occt._malloc(preset.length + 1);
occt.HEAPU8.set(preset, presetPtr);
occt.HEAPU8[presetPtr + preset.length] = 0;
const outPtr = occt._occt_tessellate(box, presetPtr);
occt._free(presetPtr);
const json = JSON.parse(occt.UTF8ToString(outPtr));
occt._occt_free(outPtr);

console.log('box tessellate', {
  handle: box,
  ok: json.ok,
  triangleCount: json.triangleCount,
  meshes: json.meshes?.length ?? 0,
});

if (!json.ok || !(json.triangleCount > 0)) {
  gaps.push(`tessellate failed (${json.error ?? 'triangleCount=0'}, code=${json.code ?? '?'})`);
}

const cyl = occt._occt_make_cylinder(5, 12);
if (!cyl) gaps.push('make_cylinder returned 0');
else occt._occt_shape_release(cyl);

occt._occt_shape_release(box);
occt._occt_arena_clear();

const wasmStat = await stat(wasmPath);
const report = [
  `# Smoke OCCT Kernel WASM`,
  ``,
  `- init: ${initMs} ms`,
  `- wasm: ${wasmStat.size} octets`,
  ``,
  `## Écarts`,
  ``,
  gaps.length ? gaps.map((gap) => `- ${gap}`).join('\n') : `- aucun`,
  ``,
].join('\n');
await writeFile(join(dist, 'GAPS-kernel.md'), report);
console.log(report);
if (gaps.length > 0) process.exit(1);
