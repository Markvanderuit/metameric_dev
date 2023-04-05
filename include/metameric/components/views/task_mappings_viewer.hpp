#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>
#include <metameric/components/views/detail/task_texture_resample.hpp>
#include <metameric/components/views/detail/task_texture_from_buffer.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <array>
#include <variant>

namespace met {
  class MappingsViewerTask : public detail::TaskNode {
    using TextureSubTask  = detail::TextureFromBufferTask<gl::Texture2d4f>;
    using BaryVariant     = std::variant<std::span<Bary>, std::span<eig::Array4f>>;

    // Texture generation subtasks
    detail::Subtasks<TextureSubTask> m_texture_subtasks;

    // Set of rolling buffers for continuous data copy, so wait time is minimized
    std::array<gl::Buffer,      6>   m_tooltip_buffers;
    std::array<gl::sync::Fence, 6>   m_tooltip_fences;
    std::array<BaryVariant, 6>       m_tooltip_maps;
    uint                             m_tooltip_cycle_i;

    // Information about what is currently visible in the tooltip
    eig::Array2i                     m_tooltip_pixel;
    int                              m_tooltip_mapping_i;

    // Delegating functions
    void eval_tooltip_copy(SchedulerHandle &info, uint texture_i);
    void eval_tooltip(SchedulerHandle &info, uint texture_i);
    void eval_popout(SchedulerHandle &info, uint texture_i);
    void eval_save(SchedulerHandle &info, uint texture_i);

  public:
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };
} // namespace met
