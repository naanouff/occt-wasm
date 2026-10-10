#include "tessellate.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

#include <BRepAdaptor_Curve.hxx>
#include <BRepBndLib.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <Poly_Triangulation.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include "json_util.hpp"

namespace occt_kernel {
namespace {

struct MeshBuffers {
  int face = 0;
  std::vector<double> positions;
  std::vector<double> normals;
  std::vector<int> indices;
};

double edgeDeflection(const TopoDS_Shape& shape, double relative) {
  Bnd_Box box;
  BRepBndLib::Add(shape, box);
  if (box.IsVoid()) return std::max(relative, 1e-4);
  double xmin = 0, ymin = 0, zmin = 0, xmax = 0, ymax = 0, zmax = 0;
  box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
  const double diag = std::sqrt((xmax - xmin) * (xmax - xmin) + (ymax - ymin) * (ymax - ymin) +
                                (zmax - zmin) * (zmax - zmin));
  return std::max(relative * (diag > 1e-9 ? diag : 1.0), 1e-6);
}

void pushPoint(std::vector<double>& wire, const gp_Pnt& point) {
  wire.push_back(point.X());
  wire.push_back(point.Y());
  wire.push_back(point.Z());
}

void appendEdgePolyline(const TopoDS_Edge& edge, double deflection, std::vector<double>& wire) {
  if (BRep_Tool::Degenerated(edge)) return;
  BRepAdaptor_Curve curve(edge);
  GCPnts_QuasiUniformDeflection sampler(curve, deflection);
  if (sampler.IsDone() && sampler.NbPoints() >= 2) {
    for (int i = 1; i < sampler.NbPoints(); ++i) {
      pushPoint(wire, sampler.Value(i));
      pushPoint(wire, sampler.Value(i + 1));
    }
    return;
  }
  const gp_Pnt start = curve.Value(curve.FirstParameter());
  const gp_Pnt end = curve.Value(curve.LastParameter());
  if (start.Distance(end) < 1e-9) return;
  pushPoint(wire, start);
  pushPoint(wire, end);
}

void appendFaceMesh(const TopoDS_Face& face, int faceId, MeshBuffers& mesh) {
  TopLoc_Location loc;
  const occ::handle<Poly_Triangulation> tri = BRep_Tool::Triangulation(face, loc);
  if (tri.IsNull() || tri->NbTriangles() < 1 || tri->NbNodes() < 3) return;
  const gp_Trsf trsf = loc.Transformation();
  const bool reversed = face.Orientation() == TopAbs_REVERSED;
  const int nbNodes = tri->NbNodes();
  std::vector<gp_Pnt> nodes(static_cast<size_t>(nbNodes));
  std::vector<double> normalSum(static_cast<size_t>(nbNodes) * 3, 0.0);
  std::vector<int> normalCount(static_cast<size_t>(nbNodes), 0);
  mesh.face = faceId;
  for (int i = 1; i <= nbNodes; ++i) {
    gp_Pnt point = tri->Node(i);
    if (!loc.IsIdentity()) point.Transform(trsf);
    nodes[static_cast<size_t>(i - 1)] = point;
    mesh.positions.push_back(point.X());
    mesh.positions.push_back(point.Y());
    mesh.positions.push_back(point.Z());
  }
  for (int i = 1; i <= tri->NbTriangles(); ++i) {
    int n1 = 0, n2 = 0, n3 = 0;
    tri->Triangle(i).Get(n1, n2, n3);
    if (reversed) std::swap(n2, n3);
    if (n1 < 1 || n2 < 1 || n3 < 1 || n1 > nbNodes || n2 > nbNodes || n3 > nbNodes) continue;
    const gp_Pnt& a = nodes[static_cast<size_t>(n1 - 1)];
    const gp_Pnt& b = nodes[static_cast<size_t>(n2 - 1)];
    const gp_Pnt& c = nodes[static_cast<size_t>(n3 - 1)];
    gp_Vec normal(gp_Vec(a, b).Crossed(gp_Vec(a, c)));
    if (normal.SquareMagnitude() > 1e-20) normal.Normalize();
    else normal = gp_Vec(0, 0, 1);
    for (int nodeIndex : {n1, n2, n3}) {
      const size_t slot = static_cast<size_t>(nodeIndex - 1) * 3;
      normalSum[slot] += normal.X();
      normalSum[slot + 1] += normal.Y();
      normalSum[slot + 2] += normal.Z();
      normalCount[static_cast<size_t>(nodeIndex - 1)] += 1;
      mesh.indices.push_back(nodeIndex - 1);
    }
  }
  mesh.normals.assign(static_cast<size_t>(nbNodes) * 3, 0.0);
  for (int i = 0; i < nbNodes; ++i) {
    gp_Vec normal(normalSum[static_cast<size_t>(i) * 3], normalSum[static_cast<size_t>(i) * 3 + 1],
                  normalSum[static_cast<size_t>(i) * 3 + 2]);
    if (normalCount[static_cast<size_t>(i)] > 0 && normal.SquareMagnitude() > 1e-20) normal.Normalize();
    else normal = gp_Vec(0, 0, 1);
    mesh.normals[static_cast<size_t>(i) * 3] = normal.X();
    mesh.normals[static_cast<size_t>(i) * 3 + 1] = normal.Y();
    mesh.normals[static_cast<size_t>(i) * 3 + 2] = normal.Z();
  }
}

}  // namespace

Quality qualityFor(const char* preset) {
  const std::string name = preset ? preset : "normal";
  if (name == "coarse") return {1e-2, 0.35};
  if (name == "fine") return {5e-4, 0.17};
  return {2.5e-3, 0.26};
}

std::string tessellateShape(const TopoDS_Shape& shape, const char* preset) {
  if (shape.IsNull()) {
    return "{\"ok\":false,\"error\":\"null shape\",\"code\":\"TessellateFailed\"}";
  }
  const Quality quality = qualityFor(preset);
  const std::string presetName = preset ? preset : "normal";
  try {
    BRepMesh_IncrementalMesh mesher(shape, quality.linear, false, quality.angular, false);
    mesher.Perform();
  } catch (const Standard_Failure& failure) {
    const char* msg = failure.GetMessageString();
    return std::string("{\"ok\":false,\"error\":\"") + jsonEscape(msg ? msg : "mesh failed") +
           "\",\"code\":\"TessellateFailed\"}";
  }

  std::vector<MeshBuffers> meshes;
  std::vector<double> wireframe;
  int nextFace = 0;
  int triangleCount = 0;
  for (TopExp_Explorer faces(shape, TopAbs_FACE); faces.More(); faces.Next()) {
    MeshBuffers mesh;
    appendFaceMesh(TopoDS::Face(faces.Current()), nextFace, mesh);
    if (mesh.indices.size() < 3) continue;
    triangleCount += static_cast<int>(mesh.indices.size() / 3);
    meshes.push_back(std::move(mesh));
    nextFace += 1;
  }
  const double deflection = edgeDeflection(shape, quality.linear);
  for (TopExp_Explorer edges(shape, TopAbs_EDGE); edges.More(); edges.Next()) {
    try {
      appendEdgePolyline(TopoDS::Edge(edges.Current()), deflection, wireframe);
    } catch (const Standard_Failure&) {
    } catch (const std::exception&) {
    }
  }

  if (triangleCount < 1) {
    return "{\"ok\":false,\"error\":\"no triangles\",\"code\":\"TessellateFailed\"}";
  }

  std::ostringstream out;
  out << "{\"ok\":true,\"triangleCount\":" << triangleCount << ",\"preset\":\"" << jsonEscape(presetName)
      << "\",\"meshes\":[";
  for (size_t m = 0; m < meshes.size(); ++m) {
    if (m) out << ',';
    const MeshBuffers& mesh = meshes[m];
    out << "{\"face\":" << mesh.face << ",\"triangleCount\":" << (mesh.indices.size() / 3)
        << ",\"positions\":[";
    for (size_t i = 0; i < mesh.positions.size(); ++i) {
      if (i) out << ',';
      out << mesh.positions[i];
    }
    out << "],\"normals\":[";
    for (size_t i = 0; i < mesh.normals.size(); ++i) {
      if (i) out << ',';
      out << mesh.normals[i];
    }
    out << "],\"indices\":[";
    for (size_t i = 0; i < mesh.indices.size(); ++i) {
      if (i) out << ',';
      out << mesh.indices[i];
    }
    out << "]}";
  }
  out << "],\"wireframe\":[";
  for (size_t i = 0; i < wireframe.size(); ++i) {
    if (i) out << ',';
    out << wireframe[i];
  }
  out << "]}";
  return out.str();
}

}  // namespace occt_kernel
