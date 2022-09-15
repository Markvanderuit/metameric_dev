#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>
#include <metameric/components/pipeline/detail/task_texture_from_buffer.hpp>
#include <metameric/components/pipeline/detail/task_texture_resample.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <array>

namespace met {
  class ErrorViewerTask : public detail::AbstractTask {
    using TextureSubtask  = detail::TextureFromBufferTask<gl::Texture2d4f>;
    using ResampleSubtask = detail::TextureResampleTask<gl::Texture2d4f>;

    // Wrapper objects to hold three buffers and attached maps 
    struct TooltipBuffer { gl::Buffer in_a, in_b, out; };
    struct TooltipMap { std::span<AlColr> in_a, in_b, out; };

    // Set of rolling buffers for continuous data copy, so tooltip wait time is minimized
    std::array<TooltipBuffer,   6> m_tooltip_buffers;
    std::array<gl::sync::Fence, 6> m_tooltip_fences;
    std::array<TooltipMap,      6> m_tooltip_maps;
    uint                           m_tooltip_cycle_i;

    // Information about what is currently visible in the tooltip

    // Local state
    uint         m_mapping_i;     // Current selected mapping
    eig::Array2i m_tooltip_pixel; // Selected pixel in tooltip
    eig::Array2u m_resample_size; // Current output size of texture

    // Components for error computation
    gl::Program     m_error_program;
    gl::ComputeInfo m_error_dispatch;

    // Delegating functions
    void eval_error(detail::TaskEvalInfo &info);
    void eval_tooltip_copy(detail::TaskEvalInfo &info);
    void eval_tooltip(detail::TaskEvalInfo &info);

  public:
    ErrorViewerTask(const std::string &name);
    
    void init(detail::TaskInitInfo &) override;
    void dstr(detail::TaskDstrInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };

} // namespace met