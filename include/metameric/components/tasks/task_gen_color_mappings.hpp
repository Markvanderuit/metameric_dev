#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>
#include <metameric/components/tasks/detail/task_texture_from_buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/texture.hpp>

namespace met {
  class GenColorMappingTask : public detail::AbstractTask {
    uint            m_mapping_i;
    gl::Program     m_program;
    gl::ComputeInfo m_dispatch;
    gl::Program     m_program_sg;
    gl::ComputeInfo m_dispatch_sg;

  public:
    GenColorMappingTask(const std::string &name, uint mapping_i);

    void init(detail::TaskInitInfo &) override;
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