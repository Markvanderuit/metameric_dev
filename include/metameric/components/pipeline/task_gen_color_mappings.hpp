#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>
#include <metameric/components/pipeline/detail/task_texture_from_buffer.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/texture.hpp>

namespace met {
  class GenColorMappingTask : public detail::AbstractTask {
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
    GenColorMappingTask(const std::string &name, uint mapping_i);

    void init(detail::TaskInitInfo &) override;
    void dstr(detail::TaskDstrInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };

  class GenColorMappingsTask : public detail::AbstractTask {
    using MappingSubTask = GenColorMappingTask;
    using TextureSubTask = detail::TextureFromBufferTask<gl::Texture2d4f>;

    detail::Subtasks<MappingSubTask> m_mapping_subtasks;
    detail::Subtasks<TextureSubTask> m_texture_subtasks;

  public:
    GenColorMappingsTask(const std::string &name);

    void init(detail::TaskInitInfo &) override;
    void dstr(detail::TaskDstrInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met