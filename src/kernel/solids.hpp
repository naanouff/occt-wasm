#pragma once

#include <cstdint>

namespace occt_kernel {

uint32_t makeWirePolyline(const double* xyz, int pointCount);
/**
 * Closed wire of lines and three-point arcs. kinds[i] == 1 marks point i as the middle
 * of the arc from point i − 1 to point i + 1; any other value is a polyline corner.
 */
uint32_t makeWireArcs(const double* xyz, int pointCount, const int32_t* kinds);
uint32_t makeFaceFromWire(uint32_t wireHandle);
uint32_t extrude(uint32_t profileHandle, double dx, double dy, double dz);
uint32_t revolve(uint32_t profileHandle, double ox, double oy, double oz, double ax, double ay,
                 double az, double angleRad);
/**
 * Sweeps a closed wire along a helix around axis (o, a), starting at o + radius · x.
 * The wire lies in the plane spanned by x and a. pitch > 0 gives a right-hand helix.
 */
uint32_t sweepHelix(uint32_t profileHandle, double ox, double oy, double oz, double ax, double ay,
                    double az, double xx, double xy, double xz, double radius, double pitch,
                    double height);
uint32_t booleanFuse(uint32_t a, uint32_t b);
uint32_t booleanCut(uint32_t a, uint32_t b);
uint32_t booleanCommon(uint32_t a, uint32_t b);

}  // namespace occt_kernel
