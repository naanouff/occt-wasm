#include "solids.hpp"

#include "arena.hpp"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <cmath>

namespace occt_kernel {
namespace {

uint32_t insertOrZero(const TopoDS_Shape& shape) {
  if (shape.IsNull()) return 0;
  return arena().insert(shape);
}

bool takeShape(uint32_t handle, TopoDS_Shape& out) {
  if (!arena().get(handle, out) || out.IsNull()) return false;
  return true;
}

void consume(uint32_t handle) { arena().release(handle); }

TopoDS_Shape profileForSolid(const TopoDS_Shape& profile) {
  if (profile.ShapeType() == TopAbs_FACE || profile.ShapeType() == TopAbs_WIRE) return profile;
  return TopoDS_Shape();
}

uint32_t booleanOp(uint32_t a, uint32_t b, char kind) {
  TopoDS_Shape shapeA;
  TopoDS_Shape shapeB;
  if (!takeShape(a, shapeA) || !takeShape(b, shapeB)) return 0;
  try {
    TopoDS_Shape result;
    bool done = false;
    if (kind == 'f') {
      BRepAlgoAPI_Fuse fuse(shapeA, shapeB);
      fuse.Build();
      done = fuse.IsDone();
      if (done) result = fuse.Shape();
    } else if (kind == 'c') {
      BRepAlgoAPI_Cut cut(shapeA, shapeB);
      cut.Build();
      done = cut.IsDone();
      if (done) result = cut.Shape();
    } else {
      BRepAlgoAPI_Common common(shapeA, shapeB);
      common.Build();
      done = common.IsDone();
      if (done) result = common.Shape();
    }
    if (!done || result.IsNull()) return 0;
    const uint32_t handle = insertOrZero(result);
    if (!handle) return 0;
    consume(a);
    consume(b);
    return handle;
  } catch (const Standard_Failure&) {
    return 0;
  }
}

}  // namespace

uint32_t makeWirePolyline(const double* xyz, int pointCount) {
  if (!xyz || pointCount < 3) return 0;
  try {
    BRepBuilderAPI_MakePolygon polygon;
    for (int i = 0; i < pointCount; ++i) {
      const double x = xyz[i * 3];
      const double y = xyz[i * 3 + 1];
      const double z = xyz[i * 3 + 2];
      if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return 0;
      polygon.Add(gp_Pnt(x, y, z));
    }
    const gp_Pnt first(xyz[0], xyz[1], xyz[2]);
    const gp_Pnt last(xyz[(pointCount - 1) * 3], xyz[(pointCount - 1) * 3 + 1],
                      xyz[(pointCount - 1) * 3 + 2]);
    if (first.Distance(last) > 1e-9) polygon.Close();
    if (!polygon.IsDone()) return 0;
    const TopoDS_Wire wire = polygon.Wire();
    if (wire.IsNull()) return 0;
    return insertOrZero(wire);
  } catch (const Standard_Failure&) {
    return 0;
  }
}

uint32_t makeFaceFromWire(uint32_t wireHandle) {
  TopoDS_Shape shape;
  if (!takeShape(wireHandle, shape)) return 0;
  if (shape.ShapeType() != TopAbs_WIRE) return 0;
  try {
    BRepBuilderAPI_MakeFace maker(TopoDS::Wire(shape), true);
    if (!maker.IsDone()) return 0;
    const TopoDS_Face face = maker.Face();
    if (face.IsNull()) return 0;
    const uint32_t handle = insertOrZero(face);
    if (!handle) return 0;
    consume(wireHandle);
    return handle;
  } catch (const Standard_Failure&) {
    return 0;
  }
}

uint32_t extrude(uint32_t profileHandle, double dx, double dy, double dz) {
  if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dz)) return 0;
  if (std::abs(dx) + std::abs(dy) + std::abs(dz) < 1e-12) return 0;
  TopoDS_Shape profile;
  if (!takeShape(profileHandle, profile)) return 0;
  profile = profileForSolid(profile);
  if (profile.IsNull()) return 0;
  try {
    BRepPrimAPI_MakePrism prism(profile, gp_Vec(dx, dy, dz));
    if (!prism.IsDone()) return 0;
    const uint32_t handle = insertOrZero(prism.Shape());
    if (!handle) return 0;
    consume(profileHandle);
    return handle;
  } catch (const Standard_Failure&) {
    return 0;
  }
}

uint32_t revolve(uint32_t profileHandle, double ox, double oy, double oz, double ax, double ay,
                 double az, double angleRad) {
  if (!std::isfinite(angleRad) || std::abs(angleRad) < 1e-12) return 0;
  const double len2 = ax * ax + ay * ay + az * az;
  if (!(len2 > 1e-24)) return 0;
  TopoDS_Shape profile;
  if (!takeShape(profileHandle, profile)) return 0;
  profile = profileForSolid(profile);
  if (profile.IsNull()) return 0;
  try {
    const gp_Ax1 axis(gp_Pnt(ox, oy, oz), gp_Dir(ax, ay, az));
    BRepPrimAPI_MakeRevol revol(profile, axis, angleRad);
    if (!revol.IsDone()) return 0;
    const uint32_t handle = insertOrZero(revol.Shape());
    if (!handle) return 0;
    consume(profileHandle);
    return handle;
  } catch (const Standard_Failure&) {
    return 0;
  }
}

uint32_t booleanFuse(uint32_t a, uint32_t b) { return booleanOp(a, b, 'f'); }
uint32_t booleanCut(uint32_t a, uint32_t b) { return booleanOp(a, b, 'c'); }
uint32_t booleanCommon(uint32_t a, uint32_t b) { return booleanOp(a, b, 'm'); }

}  // namespace occt_kernel
