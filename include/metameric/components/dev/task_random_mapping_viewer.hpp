#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>
#include <metameric/components/views/detail/task_texture_from_buffer.hpp>
#include <small_gl/texture.hpp>

namespace met {
  class RandomMappingsViewerTask : public detail::TaskNode {
    using TextureSubTask  = detail::TextureFromBufferTask<gl::Texture2d4f>;

    // Texture generation subtasks
    detail::Subtasks<TextureSubTask> m_texture_subtasks;

  public:
    void init(SchedulerHandle &) override;
    bool is_active(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };
} // namespace met