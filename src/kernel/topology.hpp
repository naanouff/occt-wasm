#pragma once

#include <cstdint>
#include <string>

#include <TopoDS_Shape.hxx>

namespace occt_kernel {

std::string listEdgesJson(const TopoDS_Shape& shape);
std::string listFacesJson(const TopoDS_Shape& shape);

/** indices == nullptr and indexCount < 0 means all edges. Consumes shapeHandle on success. */
uint32_t filletEdges(uint32_t shapeHandle, double radius, const int32_t* indices, int indexCount);
uint32_t chamferEdges(uint32_t shapeHandle, double distance, const int32_t* indices, int indexCount);

}  // namespace occt_kernel
