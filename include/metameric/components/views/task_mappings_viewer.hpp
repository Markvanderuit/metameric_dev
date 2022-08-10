#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>
#include <metameric/components/tasks/detail/task_texture_resample.hpp>
#include <small_gl/texture.hpp>
#include <vector>

namespace met {
  class MappingsViewerTask : public detail::AbstractTask {
    using ResampleTaskType = detail::TextureResampleTask<gl::Texture2d4f>;

    eig::Array2u                       m_texture_size = 1;
    detail::Subtasks<ResampleTaskType> m_resample_subtasks;

    void handle_tooltip(detail::TaskEvalInfo &info, uint texture_i);
    void handle_popout(detail::TaskEvalInfo &info, uint texture_i);

  public:
    MappingsViewerTask(const std::string &name);
    
    void dstr(detail::TaskDstrInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met
