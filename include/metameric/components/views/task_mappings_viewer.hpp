#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>
#include <metameric/components/pipeline/detail/task_texture_resample.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/utility.hpp>
#include <array>

namespace met {
  class MappingsViewerTask : public detail::AbstractTask {
    using ResampleTaskType = detail::TextureResampleTask<gl::Texture2d4f>;

    // Resample subtask state
    eig::Array2u                       m_resample_size;
    detail::Subtasks<ResampleTaskType> m_resample_tasks;

    // Set of rolling buffers for continuous data copy, so wait time is minimized
    std::array<gl::Buffer,      6>     m_tooltip_buffers;
    std::array<gl::sync::Fence, 6>     m_tooltip_fences;
    std::array<std::span<Bary>, 6>     m_tooltip_maps;
    uint                               m_tooltip_cycle_i;

    // Information about what is currently visible in the tooltip
    eig::Array2i                       m_tooltip_pixel;
    int                                m_tooltip_mapping_i;

    // Delegating functions
    void eval_tooltip_copy(detail::TaskEvalInfo &info, uint texture_i);
    void eval_tooltip(detail::TaskEvalInfo &info, uint texture_i);
    void eval_popout(detail::TaskEvalInfo &info, uint texture_i);
    void eval_save(detail::TaskEvalInfo &info, uint texture_i);

  public:
    MappingsViewerTask(const std::string &name);
    
    void init(detail::TaskInitInfo &) override;
    void dstr(detail::TaskDstrInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met
