# syntax=docker/dockerfile:1
# Emscripten 3.1.64 crashes in wasm instruction selection or emits a br_table
# the engine rejects. This image installs emsdk 6.0.11 from its git tag.
# OCCT is cloned here and checked against OCCT_SHA in versions.env.
FROM debian:bookworm-slim

SHELL ["/bin/bash", "-lc"]

RUN apt-get update \
  && apt-get install -y --no-install-recommends \
       git ca-certificates python3 cmake ninja-build xz-utils \
  && rm -rf /var/lib/apt/lists/* \
  && git clone --depth 1 --branch 6.0.11 https://github.com/emscripten-core/emsdk.git /emsdk \
  && /emsdk/emsdk install 6.0.11 \
  && /emsdk/emsdk activate 6.0.11

ENV EMSDK=/emsdk
ENV EM_CONFIG=/emsdk/.emscripten
ENV PATH="/emsdk:/emsdk/upstream/emscripten:/emsdk/upstream/bin:${PATH}"

WORKDIR /src
COPY versions.env CMakeLists.txt package.json /src/
COPY scripts /src/scripts
COPY src /src/src

RUN set -eo pipefail \
  && source /emsdk/emsdk_env.sh \
  && export PATH="/emsdk/upstream/bin:${PATH}" \
  && set -u \
  && source /src/versions.env \
  && git clone --depth 1 --branch "${OCCT_TAG}" "${OCCT_GIT_URL}" /src/OCCT \
  && test "$(git -C /src/OCCT rev-parse HEAD)" = "${OCCT_SHA}" \
  && bash /src/scripts/configure-occt.sh \
  && cmake --build /build/occt --target install -j2 \
  && cmake_dir="$(find /opt/occt -name OpenCASCADEConfig.cmake | head -n 1)" \
  && test -n "${cmake_dir}" \
  && mkdir -p /opt/occt-res \
  && cp -a /src/OCCT/resources/XSTEPResource /opt/occt-res/XSTEPResource \
  && cp -a /src/OCCT/resources/SHMessage /opt/occt-res/SHMessage \
  && emcmake cmake -S /src -B /build/api -G Ninja \
       -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_CXX_FLAGS_RELEASE="-O1 -DNDEBUG" \
       -DCMAKE_EXE_LINKER_FLAGS_RELEASE="-O1" \
       -DOpenCASCADE_DIR="$(dirname "${cmake_dir}")" \
       -DOCCT_RES=/opt/occt-res \
  && cmake --build /build/api -j2 \
  && mkdir -p /opt/dist \
  && cp /build/api/occt-step.js /build/api/occt-step.wasm \
       /build/api/occt-kernel.js /build/api/occt-kernel.wasm \
       /build/api/link-libs.txt /opt/dist/ \
  && cp /src/OCCT/LICENSE_LGPL_21.txt /src/OCCT/OCCT_LGPL_EXCEPTION.txt /opt/dist/ \
  && wasm-opt -O1 /opt/dist/occt-step.wasm -o /tmp/occt-step.opt.wasm \
  && mv /tmp/occt-step.opt.wasm /opt/dist/occt-step.wasm \
  && wasm-opt -O1 /opt/dist/occt-kernel.wasm -o /tmp/occt-kernel.opt.wasm \
  && mv /tmp/occt-kernel.opt.wasm /opt/dist/occt-kernel.wasm \
  && ! grep -q TKOpenGl /opt/dist/link-libs.txt
