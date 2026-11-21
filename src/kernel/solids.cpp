#include "solids.hpp"

#include "arena.hpp"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <GProp_GProps.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GC_MakeSegment2d.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <gp_Ax3.hxx>
#include <gp_Pnt2d.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
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
      BRepAlgoAPI_Fuse fuse;
      TopTools_ListOfShape arguments;
      TopTools_ListOfShape tools;
      arguments.Append(shapeA);
      tools.Append(shapeB);
      fuse.SetArguments(arguments);
      fuse.SetTools(tools);
      fuse.SetUseOBB(true);
      fuse.Build();
      done = fuse.IsDone();
      if (done) result = fuse.Shape();
    } else if (kind == 'c') {
      BRepAlgoAPI_Cut cut;
      TopTools_ListOfShape arguments;
      TopTools_ListOfShape tools;
      arguments.Append(shapeA);
      tools.Append(shapeB);
      cut.SetArguments(arguments);
      cut.SetTools(tools);
      cut.SetUseOBB(true);
      cut.Build();
      done = cut.IsDone();
      if (done) result = cut.Shape();
    } else {
      BRepAlgoAPI_Common common;
      TopTools_ListOfShape arguments;
      TopTools_ListOfShape tools;
      arguments.Append(shapeA);
      tools.Append(shapeB);
      common.SetArguments(arguments);
      common.SetTools(tools);
      common.SetUseOBB(true);
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

uint32_t makeWireArcs(const double* xyz, int pointCount, const int32_t* kinds) {
  if (!xyz || !kinds || pointCount < 3) return 0;
  try {
    auto point = [xyz](int i) { return gp_Pnt(xyz[i * 3], xyz[i * 3 + 1], xyz[i * 3 + 2]); };
    for (int i = 0; i < pointCount * 3; ++i) {
      if (!std::isfinite(xyz[i])) return 0;
    }
    const bool ferme = point(0).Distance(point(pointCount - 1)) <= 1e-9;
    const int fin = ferme ? pointCount - 1 : pointCount;
    BRepBuilderAPI_MakeWire fil;
    int i = 0;
    while (i < fin) {
      const int j = (i + 1) % fin;
      if (kinds[j] == 1) {
        // Point j : milieu de l'arc qui va de i à j + 1.
        const int k = (j + 1) % fin;
        if (kinds[i] == 1 || kinds[k] == 1) return 0;
        Handle(Geom_TrimmedCurve) arc = GC_MakeArcOfCircle(point(i), point(j), point(k)).Value();
        BRepBuilderAPI_MakeEdge arete(arc);
        if (!arete.IsDone()) return 0;
        fil.Add(arete.Edge());
        i += 2;
      } else {
        if (point(i).Distance(point(j)) <= 1e-9) return 0;
        BRepBuilderAPI_MakeEdge arete(point(i), point(j));
        if (!arete.IsDone()) return 0;
        fil.Add(arete.Edge());
        i += 1;
      }
      if (!fil.IsDone()) return 0;
    }
    if (i != fin) return 0;
    const TopoDS_Wire wire = fil.Wire();
    if (wire.IsNull() || !wire.Closed()) return 0;
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

uint32_t sweepHelix(uint32_t profileHandle, double ox, double oy, double oz, double ax, double ay,
                    double az, double xx, double xy, double xz, double radius, double pitch,
                    double height) {
  if (!std::isfinite(radius) || !(radius > 1e-9)) return 0;
  if (!std::isfinite(pitch) || std::abs(pitch) < 1e-9) return 0;
  if (!std::isfinite(height) || !(height > 1e-9)) return 0;
  TopoDS_Shape profile;
  if (!takeShape(profileHandle, profile)) return 0;
  if (profile.ShapeType() != TopAbs_WIRE) return 0;
  try {
    // Hélice : droite (u, v) = (2π t, pas · t) sur le cylindre, à droite si pas > 0.
    const gp_Ax3 repere(gp_Pnt(ox, oy, oz), gp_Dir(ax, ay, az), gp_Dir(xx, xy, xz));
    Handle(Geom_CylindricalSurface) cylindre = new Geom_CylindricalSurface(repere, radius);
    // Une arête par tour : des faces courtes, que les booléens trient par boîte.
    const double tours = height / std::abs(pitch);
    const double tau = 6.283185307179586;
    const double sens = pitch > 0 ? 1.0 : -1.0;
    BRepBuilderAPI_MakeWire fil;
    for (double t0 = 0.0; t0 < tours - 1e-9; t0 += 1.0) {
      const double t1 = std::min(tours, t0 + 1.0);
      const gp_Pnt2d debut(tau * t0 * sens, std::abs(pitch) * t0);
      const gp_Pnt2d fin(tau * t1 * sens, std::abs(pitch) * t1);
      Handle(Geom2d_TrimmedCurve) trace = GC_MakeSegment2d(debut, fin).Value();
      BRepBuilderAPI_MakeEdge arete(trace, cylindre);
      if (!arete.IsDone()) return 0;
      TopoDS_Edge edge = arete.Edge();
      BRepLib::BuildCurves3d(edge, 1e-4, GeomAbs_C1, 8, 0);
      fil.Add(edge);
      if (!fil.IsDone()) return 0;
    }
    const TopoDS_Wire spine = fil.Wire();

    BRepOffsetAPI_MakePipeShell pipe(spine);
    // Binormale fixée sur l'axe : le profil reste dans un plan qui contient l'axe.
    pipe.SetMode(gp_Dir(ax, ay, az));
    pipe.Add(TopoDS::Wire(profile), false, false);
    pipe.Build();
    if (!pipe.IsDone()) return 0;
    if (!pipe.MakeSolid()) return 0;
    // Un solide retourné vide les booléens sans erreur : on le remet à l'endroit.
    TopoDS_Shape solide = pipe.Shape();
    GProp_GProps proprietes;
    BRepGProp::VolumeProperties(solide, proprietes);
    if (std::abs(proprietes.Mass()) < 1e-9) return 0;
    if (proprietes.Mass() < 0) solide.Reverse();
    const uint32_t handle = insertOrZero(solide);
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
