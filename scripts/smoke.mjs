#!/usr/bin/env node
/**
 * Smoke the built module. NIST / cfaobenchmark paths are arguments.
 * A file whose name contains "nist" or "cfaobenchmark" must yield triangles
 * plus a datum, a geometric tolerance, and an annotation when the STEP has them.
 * Missing semantic objects are written to dist/GAPS.md and fail the run.
 */
import { readFile, writeFile, stat } from 'node:fs/promises';
import { basename, dirname, join } from 'node:path';
import { pathToFileURL } from 'node:url';

const dist = join(dirname(new URL(import.meta.url).pathname), '..', 'dist');
const jsPath = join(dist, 'occt-step.js');
const wasmPath = join(dist, 'occt-step.wasm');
const libsPath = join(dist, 'link-libs.txt');

const stepArgs = process.argv.slice(2).filter((arg) => !arg.startsWith('--'));
if (stepArgs.length === 0) {
  console.error('usage: node scripts/smoke.mjs <file.stp> [...]');
  process.exit(2);
}

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
if (typeof occt._occt_read_step !== 'function' || typeof occt._occt_free !== 'function') {
  console.error('occt-step.js does not export _occt_read_step and _occt_free');
  process.exit(1);
}

const gaps = [];
for (const stepPath of stepArgs) {
  const bytes = new Uint8Array(await readFile(stepPath));
  const encoded = new TextEncoder().encode('normal');
  const ptr = occt._malloc(bytes.length);
  const presetPtr = occt._malloc(encoded.length + 1);
  occt.HEAPU8.set(bytes, ptr);
  occt.HEAPU8.set(encoded, presetPtr);
  occt.HEAPU8[presetPtr + encoded.length] = 0;
  const outPtr = occt._occt_read_step(ptr, bytes.length, presetPtr);
  occt._free(ptr);
  occt._free(presetPtr);
  const json = JSON.parse(occt.UTF8ToString(outPtr));
  occt._occt_free(outPtr);
  const name = basename(stepPath);
  console.log(name, {
    ok: json.ok,
    triangleCount: json.triangleCount,
    datums: json.datums?.length ?? 0,
    tolerances: json.tolerances?.length ?? 0,
    annotations: json.annotations?.length ?? 0,
  });
  if (!json.ok || !json.triangleCount) {
    gaps.push(`${name}: pas de triangles (${json.error ?? 'triangleCount=0'})`);
    continue;
  }
  const semantic = /nist|cfaobenchmark/i.test(name);
  if (semantic && !(json.datums?.length > 0)) gaps.push(`${name}: datum absent`);
  if (semantic && !(json.tolerances?.length > 0)) gaps.push(`${name}: tolérance géométrique absente`);
  if (semantic && !(json.annotations?.length > 0)) gaps.push(`${name}: annotation absente`);
  if (Array.isArray(json.gaps)) {
    for (const gap of json.gaps) gaps.push(`${name}: ${gap}`);
  }
}

const wasmStat = await stat(wasmPath);
const report = [
  `# Smoke OCCT WASM`,
  ``,
  `- init: ${initMs} ms`,
  `- wasm: ${wasmStat.size} octets`,
  ``,
  `## Écarts`,
  ``,
  gaps.length ? gaps.map((gap) => `- ${gap}`).join('\n') : `- aucun`,
  ``,
].join('\n');
await writeFile(join(dist, 'GAPS.md'), report);
console.log(report);
if (gaps.length > 0) process.exit(1);
