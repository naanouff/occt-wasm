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

add_executable(occt-step src/occt_step_reader.cpp)
target_link_libraries(occt-step PRIVATE ${OCCT_LINK})
target_include_directories(occt-step PRIVATE ${OpenCASCADE_INCLUDE_DIR})
target_compile_options(occt-step PRIVATE -fwasm-exceptions -O3)

if(NOT OCCT_RES)
  message(FATAL_ERROR "OCCT_RES must point at XSTEPResource and SHMessage")
endif()

file(WRITE "${CMAKE_BINARY_DIR}/link-libs.txt" "${OCCT_LINK}")

target_link_options(occt-step PRIVATE
  "-fwasm-exceptions"
  "-sMODULARIZE=1"
  "-sEXPORT_ES6=1"
  "-sEXPORT_NAME=createOcctStep"
  "-sALLOW_MEMORY_GROWTH=1"
  "-sENVIRONMENT=worker,node"
  "-sFILESYSTEM=1"
  "-sEXPORTED_FUNCTIONS=['_occt_read_step','_occt_free','_malloc','_free']"
  "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString','HEAPU8']"
  "-sERROR_ON_UNDEFINED_SYMBOLS=0"
  "SHELL:--embed-file ${OCCT_RES}/XSTEPResource@/occt-res/XSTEPResource"
  "SHELL:--embed-file ${OCCT_RES}/SHMessage@/occt-res/SHMessage"
)
