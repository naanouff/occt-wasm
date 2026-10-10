/** Factory for the occt-kernel WASM module (`createOcctKernel`). */
export type OcctKernelFactory = (
  options?: EmscriptenModuleOptions,
) => Promise<OcctKernelModule>;

export interface EmscriptenModuleOptions {
  locateFile?: (path: string, prefix?: string) => string;
  [key: string]: unknown;
}

export interface OcctKernelModule {
  HEAPU8: Uint8Array;
  HEAP32: Int32Array;
  UTF8ToString(ptr: number): string;
  _malloc(size: number): number;
  _free(ptr: number): void;
  _occt_free(ptr: number): void;
  _occt_shape_release(handle: number): void;
  _occt_arena_clear(): void;
  _occt_make_box(dx: number, dy: number, dz: number): number;
  _occt_make_cylinder(radius: number, height: number): number;
  _occt_make_sphere(radius: number): number;
  _occt_make_cone(r1: number, r2: number, height: number): number;
  _occt_make_wire_polyline(xyz: number, pointCount: number): number;
  _occt_make_face_from_wire(wireHandle: number): number;
  _occt_extrude(profile: number, dx: number, dy: number, dz: number): number;
  _occt_revolve(
    profile: number,
    ox: number,
    oy: number,
    oz: number,
    ax: number,
    ay: number,
    az: number,
    angleRad: number,
  ): number;
  _occt_boolean_fuse(a: number, b: number): number;
  _occt_boolean_cut(a: number, b: number): number;
  _occt_boolean_common(a: number, b: number): number;
  _occt_list_edges(handle: number): number;
  _occt_list_faces(handle: number): number;
  _occt_fillet_edges(
    handle: number,
    radius: number,
    indices: number,
    indexCount: number,
  ): number;
  _occt_chamfer_edges(
    handle: number,
    distance: number,
    indices: number,
    indexCount: number,
  ): number;
  _occt_import_brep(bytes: number, length: number): number;
  _occt_export_brep(handle: number, outLength: number): number;
  _occt_import_step(bytes: number, length: number): number;
  _occt_export_step(handle: number, outLength: number): number;
  _occt_tessellate(handle: number, preset: number): number;
  cwrap?: (...args: unknown[]) => unknown;
  ccall?: (...args: unknown[]) => unknown;
}

export type KernelBatchOpName =
  | 'make_box'
  | 'make_cylinder'
  | 'make_sphere'
  | 'make_cone'
  | 'make_wire_polyline'
  | 'make_face_from_wire'
  | 'extrude'
  | 'revolve'
  | 'boolean_fuse'
  | 'boolean_cut'
  | 'boolean_common'
  | 'fillet_edges'
  | 'chamfer_edges'
  | 'list_edges'
  | 'list_faces'
  | 'tessellate'
  | 'shape_release'
  | 'arena_clear'
  | 'export_brep'
  | 'export_step'
  | 'import_brep'
  | 'import_step';

export interface KernelHandleRef {
  handle: string | number;
}

export interface KernelBatchOp {
  op: KernelBatchOpName;
  args?: unknown[];
  /** Store result under this alias for later `{ handle: "name" }` refs. */
  as?: string;
}

export interface KernelBatchResult {
  ok: boolean;
  error?: string;
  code?: string;
  results: Array<{ op: string; as: string | null; result: unknown }>;
  aliases?: Record<string, unknown>;
}

declare const createOcctKernel: OcctKernelFactory;
export default createOcctKernel;
