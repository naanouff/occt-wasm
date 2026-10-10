#pragma once

#include <cstdint>
#include <unordered_map>

#include <TopoDS_Shape.hxx>

namespace occt_kernel {

class Arena {
 public:
  uint32_t insert(const TopoDS_Shape& shape);
  bool get(uint32_t handle, TopoDS_Shape& out) const;
  void release(uint32_t handle);
  void clear();

 private:
  uint32_t next_ = 1;
  std::unordered_map<uint32_t, TopoDS_Shape> shapes_;
};

Arena& arena();

}  // namespace occt_kernel
