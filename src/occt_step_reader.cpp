/**
 * STEP AP242 reader. Calls STEPCAFControl and XCAF DimTol / notes so wasm-ld keeps them.
 * Mesh-only STEPControl_Reader is not this API.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <CDM_Document.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Poly_Triangulation.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <Standard_Failure.hxx>
#include <TCollection_AsciiString.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TDF_Label.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDimTolObjects_DatumObject.hxx>
#include <XCAFDimTolObjects_DimensionObject.hxx>
#include <XCAFDimTolObjects_GeomToleranceObject.hxx>
#include <XCAFDoc_Datum.hxx>
#include <XCAFDoc_DimTolTool.hxx>
#include <XCAFDoc_Dimension.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_GeomTolerance.hxx>
#include <XCAFDoc_LengthUnit.hxx>
#include <XCAFDoc_Note.hxx>
#include <XCAFDoc_NoteComment.hxx>
#include <XCAFDoc_NotesTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>

namespace {

std::string extToUtf8(const TCollection_ExtendedString& value) {
  std::string out;
  const int n = value.Length();
  out.reserve(static_cast<size_t>(n));
  for (int i = 1; i <= n; ++i) {
    const unsigned int c = static_cast<unsigned int>(value.Value(i));
    if (c < 0x80) {
      out.push_back(static_cast<char>(c));
    } else if (c < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (c >> 6)));
      out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xE0 | (c >> 12)));
      out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
  }
  return out;
}

std::string jsonEscape(const std::string& raw) {
  std::string out;
  out.reserve(raw.size() + 8);
  for (unsigned char c : raw) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out.push_back(static_cast<char>(c));
        }
    }
  }
  return out;
}

std::string labelName(const TDF_Label& label) {
  occ::handle<TDataStd_Name> name;
  if (!label.FindAttribute(TDataStd_Name::GetID(), name) || name.IsNull()) return {};
  return extToUtf8(name->Get());
}

void initStepResources() {
  static bool ready = false;
  if (ready) return;
  setenv("CSF_STEPDefaults", "/occt-res/XSTEPResource", 1);
  setenv("CSF_XSTEPResource", "/occt-res/XSTEPResource", 1);
  setenv("CSF_SHMessage", "/occt-res/SHMessage", 1);
  ready = true;
}

struct Quality {
  double linear;
  double angular;
};

Quality qualityFor(const char* preset) {
  const std::string name = preset ? preset : "normal";
  if (name == "coarse") return {1e-2, 0.35};
  if (name == "fine") return {5e-4, 0.17};
  return {2.5e-3, 0.26};
}

struct MeshBuffers {
  std::string name;
  std::vector<double> positions;
  std::vector<double> normals;
  std::vector<int> indices;
};

void appendShapeMesh(const TopoDS_Shape& shape, const std::string& name, double scale, MeshBuffers& mesh) {
  mesh.name = name;
  for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next()) {
    const TopoDS_Face face = TopoDS::Face(exp.Current());
    TopLoc_Location loc;
    occ::handle<Poly_Triangulation> tri = BRep_Tool::Triangulation(face, loc);
    if (tri.IsNull()) continue;
    const gp_Trsf trsf = loc.Transformation();
    const int base = static_cast<int>(mesh.positions.size() / 3);
    const int nbNodes = tri->NbNodes();
    std::vector<gp_Pnt> nodes(static_cast<size_t>(nbNodes));
    for (int i = 1; i <= nbNodes; ++i) {
      gp_Pnt p = tri->Node(i);
      if (!loc.IsIdentity()) p.Transform(trsf);
      nodes[static_cast<size_t>(i - 1)] = p;
      mesh.positions.push_back(p.X() * scale);
      mesh.positions.push_back(p.Y() * scale);
      mesh.positions.push_back(p.Z() * scale);
      mesh.normals.push_back(0);
      mesh.normals.push_back(0);
      mesh.normals.push_back(1);
    }
    for (int i = 1; i <= tri->NbTriangles(); ++i) {
      int n1 = 0, n2 = 0, n3 = 0;
      tri->Triangle(i).Get(n1, n2, n3);
      const gp_Pnt& a = nodes[static_cast<size_t>(n1 - 1)];
      const gp_Pnt& b = nodes[static_cast<size_t>(n2 - 1)];
      const gp_Pnt& c = nodes[static_cast<size_t>(n3 - 1)];
      gp_Vec normal(gp_Vec(a, b).Crossed(gp_Vec(a, c)));
      if (normal.SquareMagnitude() > 1e-20) normal.Normalize();
      else normal = gp_Vec(0, 0, 1);
      for (int nodeIndex : {n1, n2, n3}) {
        const int slot = (base + nodeIndex - 1) * 3;
        mesh.normals[static_cast<size_t>(slot)] = normal.X();
        mesh.normals[static_cast<size_t>(slot + 1)] = normal.Y();
        mesh.normals[static_cast<size_t>(slot + 2)] = normal.Z();
        mesh.indices.push_back(base + nodeIndex - 1);
      }
    }
  }
}

void writeAssembly(std::ostringstream& out, const TDF_Label& label) {
  out << "{\"name\":\"" << jsonEscape(labelName(label)) << "\",\"children\":[";
  NCollection_Sequence<TDF_Label> components;
  bool first = true;
  if (XCAFDoc_ShapeTool::IsAssembly(label) &&
      XCAFDoc_ShapeTool::GetComponents(label, components, false)) {
    for (int i = 1; i <= components.Length(); ++i) {
      if (!first) out << ',';
      first = false;
      writeAssembly(out, components.Value(i));
    }
  }
  out << "]}";
}

template <typename T>
std::string asciiOf(const occ::handle<T>& value) {
  if (value.IsNull()) return {};
  return value->ToCString();
}

std::string readDocument(const std::string& path, const char* preset) {
  initStepResources();
  occ::handle<CDM_Document> cdoc;
  XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", cdoc);
  occ::handle<TDocStd_Document> doc = occ::down_cast<TDocStd_Document>(cdoc);
  if (doc.IsNull()) return "{\"ok\":false,\"error\":\"NewDocument failed\"}";

  STEPCAFControl_Reader reader;
  reader.SetColorMode(true);
  reader.SetNameMode(true);
  reader.SetLayerMode(true);
  reader.SetPropsMode(true);
  reader.SetGDTMode(true);
  reader.SetMatMode(true);
  const IFSelect_ReturnStatus status = reader.ReadFile(path.c_str());
  if (status != IFSelect_RetDone) return "{\"ok\":false,\"error\":\"ReadFile failed\"}";
  if (!reader.Transfer(doc)) return "{\"ok\":false,\"error\":\"Transfer failed\"}";

  double scale = 1.0;
  std::string unitName = "file";
  occ::handle<XCAFDoc_LengthUnit> unit;
  if (doc->Main().FindAttribute(XCAFDoc_LengthUnit::GetID(), unit) && !unit.IsNull() &&
      unit->GetUnitValue() > 0) {
    scale = unit->GetUnitValue();
    unitName = unit->GetUnitName().ToCString();
  }

  const Quality quality = qualityFor(preset);
  occ::handle<XCAFDoc_ShapeTool> shapes = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
  occ::handle<XCAFDoc_DimTolTool> dimTol = XCAFDoc_DocumentTool::DimTolTool(doc->Main());
  occ::handle<XCAFDoc_NotesTool> notes = XCAFDoc_DocumentTool::NotesTool(doc->Main());

  NCollection_Sequence<TDF_Label> freeShapes;
  shapes->GetFreeShapes(freeShapes);

  std::vector<MeshBuffers> meshes;
  int triangleCount = 0;
  for (int i = 1; i <= freeShapes.Length(); ++i) {
    const TDF_Label label = freeShapes.Value(i);
    TopoDS_Shape shape;
    if (!XCAFDoc_ShapeTool::GetShape(label, shape) || shape.IsNull()) continue;
    BRepMesh_IncrementalMesh mesher(shape, quality.linear, false, quality.angular, false);
    mesher.Perform();
    MeshBuffers mesh;
    appendShapeMesh(shape, labelName(label), scale, mesh);
    triangleCount += static_cast<int>(mesh.indices.size() / 3);
    meshes.push_back(std::move(mesh));
  }

  std::ostringstream out;
  out << "{\"ok\":true,\"triangleCount\":" << triangleCount
      << ",\"lengthScaleToMeters\":" << scale
      << ",\"lengthUnit\":\"" << jsonEscape(unitName) << "\""
      << ",\"preset\":\"" << jsonEscape(preset ? preset : "normal") << "\""
      << ",\"assembly\":[";
  for (int i = 1; i <= freeShapes.Length(); ++i) {
    if (i > 1) out << ',';
    writeAssembly(out, freeShapes.Value(i));
  }
  out << "],\"datums\":[";
  NCollection_Sequence<TDF_Label> datumLabels;
  dimTol->GetDatumLabels(datumLabels);
  for (int i = 1; i <= datumLabels.Length(); ++i) {
    if (i > 1) out << ',';
    occ::handle<XCAFDoc_Datum> attr;
    std::string name = labelName(datumLabels.Value(i));
    if (datumLabels.Value(i).FindAttribute(XCAFDoc_Datum::GetID(), attr) && !attr.IsNull()) {
      const occ::handle<XCAFDimTolObjects_DatumObject> obj = attr->GetObject();
      if (!obj.IsNull() && !obj->GetName().IsNull()) name = asciiOf(obj->GetName());
    }
    out << "{\"name\":\"" << jsonEscape(name) << "\"}";
  }
  out << "],\"tolerances\":[";
  NCollection_Sequence<TDF_Label> tolLabels;
  dimTol->GetGeomToleranceLabels(tolLabels);
  for (int i = 1; i <= tolLabels.Length(); ++i) {
    if (i > 1) out << ',';
    occ::handle<XCAFDoc_GeomTolerance> attr;
    std::string name = labelName(tolLabels.Value(i));
    int type = -1;
    double value = 0;
    if (tolLabels.Value(i).FindAttribute(XCAFDoc_GeomTolerance::GetID(), attr) && !attr.IsNull()) {
      const occ::handle<XCAFDimTolObjects_GeomToleranceObject> obj = attr->GetObject();
      if (!obj.IsNull()) {
        if (!obj->GetSemanticName().IsNull()) name = asciiOf(obj->GetSemanticName());
        type = static_cast<int>(obj->GetType());
        value = obj->GetValue();
      }
    }
    out << "{\"name\":\"" << jsonEscape(name) << "\",\"type\":" << type << ",\"value\":" << value << "}";
  }
  out << "],\"dimensions\":[";
  NCollection_Sequence<TDF_Label> dimLabels;
  dimTol->GetDimensionLabels(dimLabels);
  for (int i = 1; i <= dimLabels.Length(); ++i) {
    if (i > 1) out << ',';
    occ::handle<XCAFDoc_Dimension> attr;
    std::string name = labelName(dimLabels.Value(i));
    int type = -1;
    double value = 0;
    if (dimLabels.Value(i).FindAttribute(XCAFDoc_Dimension::GetID(), attr) && !attr.IsNull()) {
      const occ::handle<XCAFDimTolObjects_DimensionObject> obj = attr->GetObject();
      if (!obj.IsNull()) {
        if (!obj->GetSemanticName().IsNull()) name = asciiOf(obj->GetSemanticName());
        type = static_cast<int>(obj->GetType());
        value = obj->GetValue();
      }
    }
    out << "{\"name\":\"" << jsonEscape(name) << "\",\"type\":" << type << ",\"value\":" << value << "}";
  }
  out << "],\"annotations\":[";
  NCollection_Sequence<TDF_Label> noteLabels;
  notes->GetNotes(noteLabels);
  for (int i = 1; i <= noteLabels.Length(); ++i) {
    if (i > 1) out << ',';
    const occ::handle<XCAFDoc_Note> note = XCAFDoc_Note::Get(noteLabels.Value(i));
    std::string user;
    std::string text;
    if (!note.IsNull()) user = extToUtf8(note->UserName());
    const occ::handle<XCAFDoc_NoteComment> comment = XCAFDoc_NoteComment::Get(noteLabels.Value(i));
    if (!comment.IsNull()) text = extToUtf8(comment->Comment());
    out << "{\"user\":\"" << jsonEscape(user) << "\",\"text\":\"" << jsonEscape(text) << "\"}";
  }
  out << "],\"meshes\":[";
  for (size_t m = 0; m < meshes.size(); ++m) {
    if (m > 0) out << ',';
    const MeshBuffers& mesh = meshes[m];
    out << "{\"name\":\"" << jsonEscape(mesh.name) << "\",\"triangleCount\":"
        << (mesh.indices.size() / 3) << ",\"positions\":[";
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
  out << "],\"gaps\":[\"MODEL_GEOMETRIC_VIEW non lu par cette API\"]}";
  return out.str();
}

char* duplicate(const std::string& text) {
  char* out = static_cast<char*>(std::malloc(text.size() + 1));
  if (!out) return nullptr;
  std::memcpy(out, text.c_str(), text.size() + 1);
  return out;
}

} // namespace

extern "C" {

char* occt_read_step(const uint8_t* bytes, int length, const char* preset) {
  if (!bytes || length <= 0) return duplicate("{\"ok\":false,\"error\":\"empty buffer\"}");
  try {
    const std::string path = "/tmp/occt-in.stp";
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes), length);
    file.close();
    if (!file) return duplicate("{\"ok\":false,\"error\":\"MEMFS write failed\"}");
    return duplicate(readDocument(path, preset));
  } catch (const Standard_Failure& failure) {
    const std::string message = failure.GetMessageString() ? failure.GetMessageString() : "Standard_Failure";
    return duplicate("{\"ok\":false,\"error\":\"" + jsonEscape(message) + "\"}");
  } catch (const std::exception& error) {
    return duplicate("{\"ok\":false,\"error\":\"" + jsonEscape(error.what()) + "\"}");
  } catch (...) {
    return duplicate("{\"ok\":false,\"error\":\"unknown\"}");
  }
}

void occt_free(void* pointer) { std::free(pointer); }

} // extern "C"
