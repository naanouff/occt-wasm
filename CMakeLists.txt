cmake_minimum_required(VERSION 3.16)
project(occt_step_wasm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT OpenCASCADE_DIR AND DEFINED ENV{OpenCASCADE_DIR})
  set(OpenCASCADE_DIR "$ENV{OpenCASCADE_DIR}")
endif()

find_package(OpenCASCADE REQUIRED)

set(OCCT_LINK "")
foreach(lib ${OpenCASCADE_LIBRARIES})
  if(lib MATCHES "TKOpenGl|TKOpenGles|TKDraw|DRAWEXE")
    message(FATAL_ERROR "Refusing to link ${lib}")
  endif()
  list(APPEND OCCT_LINK "${lib}")
endforeach()

file(WRITE "${CMAKE_BINARY_DIR}/link-libs.txt" "${OCCT_LINK}")

function(occt_wasm_link_common target)
  target_link_libraries(${target} PRIVATE ${OCCT_LINK})
  target_include_directories(${target} PRIVATE ${OpenCASCADE_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/src")
  target_compile_options(${target} PRIVATE -fwasm-exceptions -O3)
  target_link_options(${target} PRIVATE
    "-fwasm-exceptions"
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sENVIRONMENT=worker,node"
    "-sERROR_ON_UNDEFINED_SYMBOLS=0"
  )
endfunction()

# --- occt-step: STEPCAF reader + PMI ---
if(NOT OCCT_RES)
  message(FATAL_ERROR "OCCT_RES must point at XSTEPResource and SHMessage")
endif()

add_executable(occt-step src/occt_step_reader.cpp)
occt_wasm_link_common(occt-step)
target_link_options(occt-step PRIVATE
  "-sEXPORT_NAME=createOcctStep"
  "-sFILESYSTEM=1"
  "-sEXPORTED_FUNCTIONS=['_occt_read_step','_occt_free','_malloc','_free']"
  "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString','HEAPU8']"
  "SHELL:--embed-file ${OCCT_RES}/XSTEPResource@/occt-res/XSTEPResource"
  "SHELL:--embed-file ${OCCT_RES}/SHMessage@/occt-res/SHMessage"
)

# --- occt-kernel: modeling arena (primitives, solids, tessellate) ---
add_executable(occt-kernel
  src/occt_kernel.cpp
  src/kernel/arena.cpp
  src/kernel/primitives.cpp
  src/kernel/solids.cpp
  src/kernel/tessellate.cpp
)
occt_wasm_link_common(occt-kernel)
target_link_options(occt-kernel PRIVATE
  "-sEXPORT_NAME=createOcctKernel"
  "-sFILESYSTEM=0"
  "-sEXPORTED_FUNCTIONS=['_occt_shape_release','_occt_arena_clear','_occt_free','_occt_make_box','_occt_make_cylinder','_occt_make_sphere','_occt_make_cone','_occt_make_wire_polyline','_occt_make_face_from_wire','_occt_extrude','_occt_revolve','_occt_boolean_fuse','_occt_boolean_cut','_occt_boolean_common','_occt_tessellate','_malloc','_free']"
  "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString','HEAPU8','cwrap','ccall']"
)
