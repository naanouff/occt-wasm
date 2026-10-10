#include "topology.hpp"

#include "arena.hpp"
#include "json_util.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>

#include <cmath>
#include <sstream>
#include <vector>

namespace occt_kernel {
namespace {

bool takeShape(uint32_t handle, TopoDS_Shape& out) {
  return arena().get(handle, out) && !out.IsNull();
}

uint32_t insertOrZero(const TopoDS_Shape& shape) {
  if (shape.IsNull()) return 0;
  return arena().insert(shape);
}

void collectEdges(const TopoDS_Shape& shape, TopTools_IndexedMapOfShape& map) {
  TopExp::MapShapes(shape, TopAbs_EDGE, map);
}

bool selectEdges(const TopTools_IndexedMapOfShape& map, const int32_t* indices, int indexCount,
                 std::vector<TopoDS_Edge>& out) {
  out.clear();
  if (indices == nullptr || indexCount < 0) {
    for (int i = 1; i <= map.Extent(); ++i) {
      const TopoDS_Edge edge = TopoDS::Edge(map(i));
      if (!BRep_Tool::Degenerated(edge)) out.push_back(edge);
    }
    return !out.empty();
  }
  if (indexCount == 0) return false;
  for (int i = 0; i < indexCount; ++i) {
    const int id = indices[i];
    if (id < 0 || id >= map.Extent()) return false;
    const TopoDS_Edge edge = TopoDS::Edge(map(id + 1));
    if (BRep_Tool::Degenerated(edge)) continue;
    out.push_back(edge);
  }
  return !out.empty();
}

}  // namespace

std::string listEdgesJson(const TopoDS_Shape& shape) {
  if (shape.IsNull()) {
    return "{\"ok\":false,\"error\":\"null shape\",\"code\":\"InvalidArgument\"}";
  }
  TopTools_IndexedMapOfShape map;
  collectEdges(shape, map);
  std::ostringstream out;
  out << "{\"ok\":true,\"edges\":[";
  bool first = true;
  for (int i = 1; i <= map.Extent(); ++i) {
    const TopoDS_Edge edge = TopoDS::Edge(map(i));
    if (BRep_Tool::Degenerated(edge)) continue;
    double firstParam = 0;
    double lastParam = 0;
    const auto curve = BRep_Tool::Curve(edge, firstParam, lastParam);
    double length = 0;
    gp_Pnt mid(0, 0, 0);
    if (!curve.IsNull()) {
      BRepAdaptor_Curve adaptor(edge);
      GProp_GProps props;
      // Length via GC - use adaptor mid parameter
      const double midParam = 0.5 * (adaptor.FirstParameter() + adaptor.LastParameter());
      mid = adaptor.Value(midParam);
      try {
        BRepGProp::LinearProperties(edge, props);
        length = props.Mass();
      } catch (const Standard_Failure&) {
        length = mid.Distance(adaptor.Value(adaptor.FirstParameter())) +
                 mid.Distance(adaptor.Value(adaptor.LastParameter()));
      }
    }
    if (!first) out << ',';
    first = false;
    out << "{\"id\":" << (i - 1) << ",\"length\":" << length << ",\"midPoint\":[" << mid.X() << ','
        << mid.Y() << ',' << mid.Z() << "]}";
  }
  out << "]}";
  return out.str();
}

std::string listFacesJson(const TopoDS_Shape& shape) {
  if (shape.IsNull()) {
    return "{\"ok\":false,\"error\":\"null shape\",\"code\":\"InvalidArgument\"}";
  }
  TopTools_IndexedMapOfShape map;
  TopExp::MapShapes(shape, TopAbs_FACE, map);
  std::ostringstream out;
  out << "{\"ok\":true,\"faces\":[";
  bool first = true;
  for (int i = 1; i <= map.Extent(); ++i) {
    const TopoDS_Face face = TopoDS::Face(map(i));
    GProp_GProps props;
    double area = 0;
    gp_Pnt mid(0, 0, 0);
    try {
      BRepGProp::SurfaceProperties(face, props);
      area = props.Mass();
      mid = props.CentreOfMass();
    } catch (const Standard_Failure&) {
    }
    if (!first) out << ',';
    first = false;
    out << "{\"id\":" << (i - 1) << ",\"area\":" << area << ",\"midPoint\":[" << mid.X() << ','
        << mid.Y() << ',' << mid.Z() << "]}";
  }
  out << "]}";
  return out.str();
}

uint32_t filletEdges(uint32_t shapeHandle, double radius, const int32_t* indices, int indexCount) {
  if (!(radius > 0) || !std::isfinite(radius)) return 0;
  TopoDS_Shape shape;
  if (!takeShape(shapeHandle, shape)) return 0;
  try {
    TopTools_IndexedMapOfShape map;
    collectEdges(shape, map);
    std::vector<TopoDS_Edge> edges;
    if (!selectEdges(map, indices, indexCount, edges)) return 0;
    BRepFilletAPI_MakeFillet fillet(shape);
    for (const TopoDS_Edge& edge : edges) fillet.Add(radius, edge);
    fillet.Build();
    if (!fillet.IsDone()) return 0;
    const uint32_t handle = insertOrZero(fillet.Shape());
    if (!handle) return 0;
    arena().release(shapeHandle);
    return handle;
  } catch (const Standard_Failure&) {
    return 0;
  }
}

uint32_t chamferEdges(uint32_t shapeHandle, double distance, const int32_t* indices, int indexCount) {
  if (!(distance > 0) || !std::isfinite(distance)) return 0;
  TopoDS_Shape shape;
  if (!takeShape(shapeHandle, shape)) return 0;
  try {
    TopTools_IndexedMapOfShape map;
    collectEdges(shape, map);
    std::vector<TopoDS_Edge> edges;
    if (!selectEdges(map, indices, indexCount, edges)) return 0;
    BRepFilletAPI_MakeChamfer chamfer(shape);
    for (const TopoDS_Edge& edge : edges) chamfer.Add(distance, edge);
    chamfer.Build();
    if (!chamfer.IsDone()) return 0;
    const uint32_t handle = insertOrZero(chamfer.Shape());
    if (!handle) return 0;
    arena().release(shapeHandle);
    return handle;
  } catch (const Standard_Failure&) {
    return 0;
  }
}

}  // namespace occt_kernel
