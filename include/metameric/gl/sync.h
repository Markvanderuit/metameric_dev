#pragma once

#include <metameric/gl/detail/fwd.h>
#include <metameric/gl/detail/handle.h>

namespace metameric::gl {
  void memory_barrier(gl::BarrierFlags flags);
  
  class Fence : public Handle<void *> {
    using Base = Handle<void *>;

  public:
    Fence();
    ~Fence();

    void cpu_wait_sync(); // blocking
    void gpu_wait_sync();
  };
} // namespace metameric::gl