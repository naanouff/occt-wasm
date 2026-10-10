#include "io.hpp"

#include "arena.hpp"

#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Shape.hxx>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace occt_kernel {
namespace {

void initStepResources() {
  static bool ready = false;
  if (ready) return;
  setenv("CSF_STEPDefaults", "/occt-res/XSTEPResource", 1);
  setenv("CSF_XSTEPResource", "/occt-res/XSTEPResource", 1);
  setenv("CSF_SHMessage", "/occt-res/SHMessage", 1);
  ready = true;
}

uint8_t* copyBuffer(const std::string& data, int* outLength) {
  if (outLength) *outLength = 0;
  if (data.empty()) return nullptr;
  auto* out = static_cast<uint8_t*>(std::malloc(data.size()));
  if (!out) return nullptr;
  std::memcpy(out, data.data(), data.size());
  if (outLength) *outLength = static_cast<int>(data.size());
  return out;
}

uint32_t insertOrZero(const TopoDS_Shape& shape) {
  if (shape.IsNull()) return 0;
  return arena().insert(shape);
}

}  // namespace

uint32_t importBrep(const uint8_t* bytes, int length) {
  if (!bytes || length <= 0) return 0;
  try {
    const std::string text(reinterpret_cast<const char*>(bytes), static_cast<size_t>(length));
    std::istringstream stream(text);
    TopoDS_Shape shape;
    BRep_Builder builder;
    BRepTools::Read(shape, stream, builder);
    return insertOrZero(shape);
  } catch (const Standard_Failure&) {
    return 0;
  } catch (const std::exception&) {
    return 0;
  }
}

uint8_t* exportBrep(uint32_t handle, int* outLength) {
  if (outLength) *outLength = 0;
  TopoDS_Shape shape;
  if (!arena().get(handle, shape) || shape.IsNull()) return nullptr;
  try {
    std::ostringstream stream;
    BRepTools::Write(shape, stream);
    return copyBuffer(stream.str(), outLength);
  } catch (const Standard_Failure&) {
    return nullptr;
  } catch (const std::exception&) {
    return nullptr;
  }
}

uint32_t importStep(const uint8_t* bytes, int length) {
  if (!bytes || length <= 0) return 0;
  initStepResources();
  try {
    const std::string path = "/tmp/occt-kernel-in.stp";
    {
      std::ofstream file(path, std::ios::binary | std::ios::trunc);
      file.write(reinterpret_cast<const char*>(bytes), length);
      if (!file) return 0;
    }
    STEPControl_Reader reader;
    if (reader.ReadFile(path.c_str()) != IFSelect_RetDone) return 0;
    reader.TransferRoots();
    const TopoDS_Shape shape = reader.OneShape();
    return insertOrZero(shape);
  } catch (const Standard_Failure&) {
    return 0;
  } catch (const std::exception&) {
    return 0;
  }
}

uint8_t* exportStep(uint32_t handle, int* outLength) {
  if (outLength) *outLength = 0;
  TopoDS_Shape shape;
  if (!arena().get(handle, shape) || shape.IsNull()) return nullptr;
  initStepResources();
  try {
    const std::string path = "/tmp/occt-kernel-out.stp";
    STEPControl_Writer writer;
    if (writer.Transfer(shape, STEPControl_AsIs) != IFSelect_RetDone) return nullptr;
    if (writer.Write(path.c_str()) != IFSelect_RetDone) return nullptr;
    std::ifstream file(path, std::ios::binary);
    if (!file) return nullptr;
    std::ostringstream stream;
    stream << file.rdbuf();
    return copyBuffer(stream.str(), outLength);
  } catch (const Standard_Failure&) {
    return nullptr;
  } catch (const std::exception&) {
    return nullptr;
  }
}

}  // namespace occt_kernel
