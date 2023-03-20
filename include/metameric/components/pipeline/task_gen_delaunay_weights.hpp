#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class GenDelaunayWeightsTask : public detail::TaskBase {
    struct UniformBuffer {
      uint n;       // Nr. of points to dispatch computation for
      uint n_verts; // Nr. of vertices defining surrounding hull
      uint n_elems; // Nr. of elements defining surrounding hull
    };

    gl::ComputeInfo m_dispatch;
    gl::Program     m_program;
    gl::Buffer      m_uniform_buffer;
    UniformBuffer  *m_uniform_map;

  public:
    void init(detail::SchedulerHandle &) override;
    void eval(detail::SchedulerHandle &) override;
  };
} // namespace met