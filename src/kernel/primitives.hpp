#pragma once

#include <cstdint>

namespace occt_kernel {

uint32_t makeBox(double dx, double dy, double dz);
uint32_t makeCylinder(double radius, double height);
uint32_t makeSphere(double radius);
uint32_t makeCone(double r1, double r2, double height);

}  // namespace occt_kernel
