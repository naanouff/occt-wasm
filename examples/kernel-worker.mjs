/**
 * Minimal Worker hosting occt-kernel + runBatch.
 *
 * Main thread:
 *   const worker = new Worker(new URL('./kernel-worker.mjs', import.meta.url), { type: 'module' });
 *   worker.postMessage({ type: 'init', wasmUrl: '/occt-kernel.wasm' });
 *   worker.postMessage({ type: 'batch', id: 1, ops: [...] });
 *
 * One Module instance per Worker — do not share handles across workers.
 */
import { runBatch } from '../js/kernel-batch.mjs';

let occt = null;
let initMs = null;

async function ensureKernel(wasmUrl) {
  if (occt) return occt;
  const started = Date.now();
  const { default: createOcctKernel } = await import('../dist/occt-kernel.js');
  occt = await createOcctKernel({
    locateFile: (path) => (path.endsWith('.wasm') ? wasmUrl : path),
  });
  initMs = Date.now() - started;
  return occt;
}

self.onmessage = async (event) => {
  const msg = event.data ?? {};
  try {
    if (msg.type === 'init') {
      await ensureKernel(msg.wasmUrl ?? './occt-kernel.wasm');
      self.postMessage({ type: 'ready', initMs });
      return;
    }
    if (msg.type === 'batch') {
      await ensureKernel(msg.wasmUrl ?? './occt-kernel.wasm');
      const result = runBatch(occt, msg.ops ?? []);
      self.postMessage({ type: 'batch-result', id: msg.id, initMs, ...result });
      return;
    }
    if (msg.type === 'stats') {
      self.postMessage({
        type: 'stats',
        initMs,
        ready: Boolean(occt),
        // HEAP length is a rough resident size signal for the wasm linear memory.
        heapBytes: occt?.HEAPU8?.length ?? null,
      });
      return;
    }
    self.postMessage({ type: 'error', error: `unknown message type: ${msg.type}` });
  } catch (error) {
    self.postMessage({
      type: 'error',
      id: msg.id,
      error: error instanceof Error ? error.message : String(error),
    });
  }
};
