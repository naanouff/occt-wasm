#pragma once

#include <cstddef>
#include <cstdint>

namespace occt_kernel {

uint32_t importBrep(const uint8_t* bytes, int length);
uint8_t* exportBrep(uint32_t handle, int* outLength);

uint32_t importStep(const uint8_t* bytes, int length);
uint8_t* exportStep(uint32_t handle, int* outLength);

}  // namespace occt_kernel
