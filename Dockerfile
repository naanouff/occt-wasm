# syntax=docker/dockerfile:1
# Pin matches versions.env (emscripten/emsdk:3.1.64).
FROM emscripten/emsdk:3.1.64@sha256:8847dad4171ebc8a53d9ae5cda86a2546ef5b2e68834c14dc1ba2b2962e125cc

SHELL ["/bin/bash", "-lc"]

RUN apt-get update \
  && apt-get install -y --no-install-recommends git ca-certificates python3 \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY versions.env CMakeLists.txt package.json /src/
COPY scripts /src/scripts
COPY src /src/src

RUN set -euo pipefail \
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
       -DOpenCASCADE_DIR="$(dirname "${cmake_dir}")" \
       -DOCCT_RES=/opt/occt-res \
  && cmake --build /build/api -j2 \
  && mkdir -p /opt/dist \
  && cp /build/api/occt-step.js /build/api/occt-step.wasm /build/api/link-libs.txt /opt/dist/ \
  && cp /src/OCCT/LICENSE_LGPL_21.txt /src/OCCT/OCCT_LGPL_EXCEPTION.txt /opt/dist/ \
  && wasm-opt -O3 /opt/dist/occt-step.wasm -o /opt/dist/occt-step.wasm \
  && ! grep -q TKOpenGl /opt/dist/link-libs.txt
