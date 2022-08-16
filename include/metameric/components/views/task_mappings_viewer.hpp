#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>
#include <metameric/components/tasks/detail/task_texture_resample.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <vector>

namespace met {
  class MappingsViewerTask : public detail::AbstractTask {
    using ResampleTaskType = detail::TextureResampleTask<gl::Texture2d4f>;

    eig::Array2u                       m_resample_size;
    detail::Subtasks<ResampleTaskType> m_resample_tasks;

    gl::Buffer                         m_tooltip_buffer;
    std::span<Spec>                    m_tooltip_map;
    gl::sync::Fence                    m_tooltip_fence;
    eig::Array2i                       m_tooltip_pixel;
    int                                m_tooltip_i;


    void eval_tooltip_copy(detail::TaskEvalInfo &info, uint texture_i);
    void eval_tooltip(detail::TaskEvalInfo &info, uint texture_i);
    void eval_popout(detail::TaskEvalInfo &info, uint texture_i);

  public:
    MappingsViewerTask(const std::string &name);
    
    void init(detail::TaskInitInfo &) override;
    void dstr(detail::TaskDstrInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met
