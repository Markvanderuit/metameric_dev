#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/texture.hpp>

namespace met {
  class GenColorMappingTask : public detail::TaskBase {
    struct UniformBuffer {
      uint n;       // Nr. of points to dispatch computation for
      uint n_verts; // Nr. of vertices defining delaunay
      uint n_elems; // Nr. of elements defining delaunay
    };

    bool              m_init_stale;
    uint              m_mapping_i;
    gl::Buffer        m_uniform_buffer;
    gl::Buffer        m_gamut_buffer;
    gl::Program       m_program;
    gl::ComputeInfo   m_dispatch;

    UniformBuffer    *m_uniform_map;
    std::span<AlColr> m_gamut_map;

  public:
    GenColorMappingTask(uint mapping_i);

    void init(SchedulerHandle &) override;
    bool eval_state(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };

  class GenColorMappingsTask : public detail::TaskBase {
    detail::Subtasks<GenColorMappingTask> m_mapping_subtasks;

  public:
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };
} // namespace met