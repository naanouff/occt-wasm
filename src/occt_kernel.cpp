/**
 * Modeling kernel: shape arena, primitives, tessellation.
 * STEP PMI stays on occt-step; parametric history stays out of band.
 */

#include "kernel/arena.hpp"
#include "kernel/json_util.hpp"
#include "kernel/primitives.hpp"
#include "kernel/tessellate.hpp"

#include <Standard_Failure.hxx>
#include <TopoDS_Shape.hxx>

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <string>

extern "C" {

void occt_shape_release(uint32_t handle) { occt_kernel::arena().release(handle); }

void occt_arena_clear() { occt_kernel::arena().clear(); }

void occt_free(void* pointer) { std::free(pointer); }

uint32_t occt_make_box(double dx, double dy, double dz) { return occt_kernel::makeBox(dx, dy, dz); }

uint32_t occt_make_cylinder(double radius, double height) {
  return occt_kernel::makeCylinder(radius, height);
}

uint32_t occt_make_sphere(double radius) { return occt_kernel::makeSphere(radius); }

uint32_t occt_make_cone(double r1, double r2, double height) {
  return occt_kernel::makeCone(r1, r2, height);
}

char* occt_tessellate(uint32_t handle, const char* preset) {
  try {
    TopoDS_Shape shape;
    if (!occt_kernel::arena().get(handle, shape)) {
      return occt_kernel::errorJson("InvalidHandle", "unknown shape handle");
    }
    return occt_kernel::duplicate(occt_kernel::tessellateShape(shape, preset));
  } catch (const Standard_Failure& failure) {
    const char* msg = failure.GetMessageString();
    return occt_kernel::errorJson("InternalError", msg ? msg : "Standard_Failure");
  } catch (const std::exception& error) {
    return occt_kernel::errorJson("InternalError", error.what());
  } catch (...) {
    return occt_kernel::errorJson("InternalError", "unknown");
  }
}

}  // extern "C"
