#include "arena.hpp"

namespace occt_kernel {

uint32_t Arena::insert(const TopoDS_Shape& shape) {
  if (shape.IsNull()) return 0;
  const uint32_t handle = next_++;
  if (next_ == 0) next_ = 1;
  shapes_[handle] = shape;
  return handle;
}

bool Arena::get(uint32_t handle, TopoDS_Shape& out) const {
  const auto it = shapes_.find(handle);
  if (it == shapes_.end() || it->second.IsNull()) return false;
  out = it->second;
  return true;
}

void Arena::release(uint32_t handle) { shapes_.erase(handle); }

void Arena::clear() {
  shapes_.clear();
  next_ = 1;
}

Arena& arena() {
  static Arena instance;
  return instance;
}

}  // namespace occt_kernel
