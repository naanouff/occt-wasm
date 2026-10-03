#!/usr/bin/env bash
# Configure official OCCT sources for a static wasm32 build.
# TKXCAF and TKDESTEP live in DataExchange / ApplicationFramework.
# Visualization stays off so FreeType and TKOpenGl are not required.
# Release objects use -O2. emscripten 3.1.64 clang either crashes in wasm
# instruction selection (-O1 and above) or emits a br_table the engine
# rejects (-O0). The image uses a newer emsdk.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck disable=SC1091
source "${ROOT}/versions.env"

OCCT_SRC="${OCCT_SRC:-/src/OCCT}"
OCCT_BUILD="${OCCT_BUILD:-/build/occt}"
OCCT_PREFIX="${OCCT_PREFIX:-/opt/occt}"

# Unix builds define OCC_CONVERT_SIGNALS, which places setjmp inside try.
# Clang's wasm exception lowering then emits a br_table the engine rejects.
flags="${OCCT_SRC}/adm/cmake/occt_defs_flags.cmake"
if grep -q 'add_definitions(-DOCC_CONVERT_SIGNALS)' "${flags}"; then
  python3 - "${flags}" <<'PY'
import pathlib, sys
path = pathlib.Path(sys.argv[1])
text = path.read_text(encoding="utf-8")
old = "  add_definitions(-DOCC_CONVERT_SIGNALS)\n"
new = "  # Wasm: setjmp inside try emits an invalid br_table.\n  # add_definitions(-DOCC_CONVERT_SIGNALS)\n"
if old not in text:
    raise SystemExit("OCC_CONVERT_SIGNALS line not found")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
PY
fi

emcmake cmake -S "${OCCT_SRC}" -B "${OCCT_BUILD}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${OCCT_PREFIX}" \
  -DINSTALL_DIR="${OCCT_PREFIX}" \
  -DBUILD_LIBRARY_TYPE=Static \
  -DBUILD_MODULE_FoundationClasses=ON \
  -DBUILD_MODULE_ModelingData=ON \
  -DBUILD_MODULE_ModelingAlgorithms=ON \
  -DBUILD_MODULE_Visualization=OFF \
  -DBUILD_MODULE_ApplicationFramework=ON \
  -DBUILD_MODULE_DataExchange=ON \
  -DBUILD_MODULE_Draw=OFF \
  -DBUILD_DOC_Overview=OFF \
  -DBUILD_YACCLEX=OFF \
  -DBUILD_USE_PCH=OFF \
  -DBUILD_USE_VCPKG=OFF \
  -DUSE_FREETYPE=OFF \
  -DUSE_OPENGL=OFF \
  -DUSE_GLES2=OFF \
  -DUSE_TBB=OFF \
  -DUSE_FREEIMAGE=OFF \
  -DUSE_RAPIDJSON=OFF \
  -DUSE_DRACO=OFF \
  -DUSE_VTK=OFF \
  -DUSE_TCL=OFF \
  -DUSE_TK=OFF \
  -DCMAKE_CXX_FLAGS="-fwasm-exceptions" \
  -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG"
