#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/texture.hpp>

namespace met {
  class GenColorMappingTask : public detail::TaskNode {
    struct UniformBuffer {
      uint n;       // Nr. of points to dispatch computation for
      uint n_verts; // Nr. of vertices defining meshing structure
      uint n_elems; // Nr. of elements defining meshing structure
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
    bool is_active(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };

  class GenColorMappingResampledTask : public detail::TaskNode {
  public:
    using TextureType = gl::Texture2d3f;

  private:
    struct UniformBuffer {
      alignas(8) eig::Array2u in_size;  // Nr. of texels to sample from
      alignas(8) eig::Array2u out_size; // Nr. of texels to dispatch shader for
      alignas(4) uint         n_verts;  // Nr. of vertices defining meshing structure
      alignas(4) uint         n_elems;  // Nr. of elements defining meshing structure
    };

    bool              m_is_mutated;
    uint              m_mapping_i;
    gl::Buffer        m_uniform_buffer;
    gl::Buffer        m_gamut_buffer;
    gl::Program       m_program;
    gl::ComputeInfo   m_dispatch;

    UniformBuffer    *m_uniform_map;
    std::span<AlColr> m_gamut_map;

  public:
    GenColorMappingResampledTask(uint mapping_i);

    void init(SchedulerHandle &) override;
    bool is_active(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;

    void set_texture_info(SchedulerHandle &info, TextureType::InfoType texture_info);
  };

  class GenColorMappingsResampledTask : public detail::TaskNode {
    detail::Subtasks<GenColorMappingResampledTask> m_mapping_subtasks;

  public:
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };

  class GenColorMappingsTask : public detail::TaskNode {
    detail::Subtasks<GenColorMappingTask> m_mapping_subtasks;

  public:
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };
} // namespace met