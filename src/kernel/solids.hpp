#pragma once

#include <cstdint>

namespace occt_kernel {

uint32_t makeWirePolyline(const double* xyz, int pointCount);
uint32_t makeFaceFromWire(uint32_t wireHandle);
uint32_t extrude(uint32_t profileHandle, double dx, double dy, double dz);
uint32_t revolve(uint32_t profileHandle, double ox, double oy, double oz, double ax, double ay,
                 double az, double angleRad);
uint32_t booleanFuse(uint32_t a, uint32_t b);
uint32_t booleanCut(uint32_t a, uint32_t b);
uint32_t booleanCommon(uint32_t a, uint32_t b);

}  // namespace occt_kernel
