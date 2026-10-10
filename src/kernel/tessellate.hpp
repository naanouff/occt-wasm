#pragma once

#include <string>

#include <TopoDS_Shape.hxx>

namespace occt_kernel {

struct Quality {
  double linear;
  double angular;
};

Quality qualityFor(const char* preset);
std::string tessellateShape(const TopoDS_Shape& shape, const char* preset);

}  // namespace occt_kernel
