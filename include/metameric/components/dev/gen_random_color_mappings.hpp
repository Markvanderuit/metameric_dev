#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class GenRandomColorMappingTask : public detail::TaskNode {
    struct UniformBuffer {
      uint n;       // Nr. of points to dispatch computation for
      uint n_verts; // Nr. of vertices defining meshing structure
      uint n_elems; // Nr. of elements defining meshing structure
    };

    bool              m_has_run_once;
    uint              m_constraint_i;
    uint              m_mapping_i;
    gl::Buffer        m_uniform_buffer;
    gl::Buffer        m_gamut_buffer;
    gl::Program       m_program;
    gl::ComputeInfo   m_dispatch;

    UniformBuffer    *m_uniform_map;
    std::span<AlColr> m_gamut_map;

  public:
    GenRandomColorMappingTask(uint constraint_i, uint mapping_i);

    void init(SchedulerHandle &) override;
    bool is_active(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };

  class GenRandomColorMappingsTask : public detail::TaskNode {
    detail::Subtasks<GenRandomColorMappingTask> m_mapping_subtasks;

  public:
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };
} // namespace met