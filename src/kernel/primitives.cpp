#include "arena.hpp"

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <Standard_Failure.hxx>

namespace occt_kernel {
namespace {

uint32_t insertOrZero(const TopoDS_Shape& shape) {
  if (shape.IsNull()) return 0;
  return arena().insert(shape);
}

}  // namespace

uint32_t makeBox(double dx, double dy, double dz) {
  if (!(dx > 0) || !(dy > 0) || !(dz > 0)) return 0;
  try {
    return insertOrZero(BRepPrimAPI_MakeBox(dx, dy, dz).Shape());
  } catch (const Standard_Failure&) {
    return 0;
  }
}

uint32_t makeCylinder(double radius, double height) {
  if (!(radius > 0) || !(height > 0)) return 0;
  try {
    return insertOrZero(BRepPrimAPI_MakeCylinder(radius, height).Shape());
  } catch (const Standard_Failure&) {
    return 0;
  }
}

uint32_t makeSphere(double radius) {
  if (!(radius > 0)) return 0;
  try {
    return insertOrZero(BRepPrimAPI_MakeSphere(radius).Shape());
  } catch (const Standard_Failure&) {
    return 0;
  }
}

uint32_t makeCone(double r1, double r2, double height) {
  if (!(height > 0) || !(r1 >= 0) || !(r2 >= 0) || (r1 <= 0 && r2 <= 0)) return 0;
  try {
    return insertOrZero(BRepPrimAPI_MakeCone(r1, r2, height).Shape());
  } catch (const Standard_Failure&) {
    return 0;
  }
}

}  // namespace occt_kernel
