#!/usr/bin/env node
/**
 * Smoke occt-kernel: primitives + profil→extrude→cut → tessellate.
 * Writes dist/GAPS-kernel.md with init time and wasm size.
 */
import { readFile, writeFile, stat } from 'node:fs/promises';
import { dirname, join } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const dist = join(dirname(fileURLToPath(import.meta.url)), '..', 'dist');
const jsPath = join(dist, 'occt-kernel.js');
const wasmPath = join(dist, 'occt-kernel.wasm');
const libsPath = join(dist, 'link-libs.txt');

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
  '_occt_make_wire_polyline',
  '_occt_make_face_from_wire',
  '_occt_extrude',
  '_occt_revolve',
  '_occt_boolean_fuse',
  '_occt_boolean_cut',
  '_occt_boolean_common',
  '_occt_list_edges',
  '_occt_list_faces',
  '_occt_fillet_edges',
  '_occt_chamfer_edges',
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

function tessellate(handle, label) {
  const preset = new TextEncoder().encode('normal');
  const presetPtr = occt._malloc(preset.length + 1);
  occt.HEAPU8.set(preset, presetPtr);
  occt.HEAPU8[presetPtr + preset.length] = 0;
  const outPtr = occt._occt_tessellate(handle, presetPtr);
  occt._free(presetPtr);
  const json = JSON.parse(occt.UTF8ToString(outPtr));
  occt._occt_free(outPtr);
  console.log(label, {
    handle,
    ok: json.ok,
    triangleCount: json.triangleCount,
    meshes: json.meshes?.length ?? 0,
  });
  if (!json.ok || !(json.triangleCount > 0)) {
    gaps.push(`${label}: tessellate failed (${json.error ?? 'triangleCount=0'})`);
  }
  return json;
}

const box = occt._occt_make_box(10, 20, 30);
if (!box) gaps.push('make_box returned 0');
else tessellate(box, 'box');

// Rectangle in XY → face → extrude → cut cylinder
const rect = new Float64Array([0, 0, 0, 40, 0, 0, 40, 30, 0, 0, 30, 0]);
const xyzPtr = occt._malloc(rect.byteLength);
occt.HEAPU8.set(new Uint8Array(rect.buffer, rect.byteOffset, rect.byteLength), xyzPtr);
const wire = occt._occt_make_wire_polyline(xyzPtr, 4);
occt._free(xyzPtr);
if (!wire) gaps.push('make_wire_polyline returned 0');

const face = wire ? occt._occt_make_face_from_wire(wire) : 0;
if (!face) gaps.push('make_face_from_wire returned 0');

const solid = face ? occt._occt_extrude(face, 0, 0, 8) : 0;
if (!solid) gaps.push('extrude returned 0');

const tool = occt._occt_make_cylinder(4, 20);
if (!tool) gaps.push('make_cylinder (cut tool) returned 0');

const cut = solid && tool ? occt._occt_boolean_cut(solid, tool) : 0;
if (!cut) gaps.push('boolean_cut returned 0');
else tessellate(cut, 'extrude-cut');

// Revolve: half-disk rectangle around Y
const profilePts = new Float64Array([10, 0, 0, 20, 0, 0, 20, 0, 5, 10, 0, 5]);
const profilePtr = occt._malloc(profilePts.byteLength);
occt.HEAPU8.set(
  new Uint8Array(profilePts.buffer, profilePts.byteOffset, profilePts.byteLength),
  profilePtr,
);
const revWire = occt._occt_make_wire_polyline(profilePtr, 4);
occt._free(profilePtr);
const revFace = revWire ? occt._occt_make_face_from_wire(revWire) : 0;
const revolved = revFace
  ? occt._occt_revolve(revFace, 0, 0, 0, 0, 1, 0, Math.PI * 2)
  : 0;
if (!revolved) gaps.push('revolve returned 0');
else {
  tessellate(revolved, 'revolve');
  occt._occt_shape_release(revolved);
}

// Fillet all edges of a box
const filletBox = occt._occt_make_box(20, 20, 20);
const edgePtr = filletBox ? occt._occt_list_edges(filletBox) : 0;
if (edgePtr) {
  const edgeJson = JSON.parse(occt.UTF8ToString(edgePtr));
  occt._occt_free(edgePtr);
  console.log('list_edges', { ok: edgeJson.ok, count: edgeJson.edges?.length ?? 0 });
  if (!edgeJson.ok || !(edgeJson.edges?.length > 0)) gaps.push('list_edges failed');
}
const facePtr = filletBox ? occt._occt_list_faces(filletBox) : 0;
if (facePtr) {
  const faceJson = JSON.parse(occt.UTF8ToString(facePtr));
  occt._occt_free(facePtr);
  console.log('list_faces', { ok: faceJson.ok, count: faceJson.faces?.length ?? 0 });
  if (!faceJson.ok || !(faceJson.faces?.length > 0)) gaps.push('list_faces failed');
}
const filleted = filletBox ? occt._occt_fillet_edges(filletBox, 1.0, 0, -1) : 0;
if (!filleted) gaps.push('fillet_edges (all) returned 0');
else {
  tessellate(filleted, 'fillet-box');
  occt._occt_shape_release(filleted);
}

if (box) occt._occt_shape_release(box);
if (cut) occt._occt_shape_release(cut);
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
