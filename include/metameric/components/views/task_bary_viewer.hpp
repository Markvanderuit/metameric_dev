#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/components/pipeline/task_gen_bary_mapping.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <array>
#include <variant>

namespace met {
  class BaryViewerTask : public detail::TaskNode {
    using BaryVariant    = std::variant<std::span<Bary>, std::span<eig::Array4f>>;

    // Wrapper objects to hold three buffers and attached maps 
    struct TooltipBuffer { gl::Buffer in_a, in_b, out; };
    struct TooltipMap { std::span<AlColr> in_a, in_b, out; };

    // Set of rolling buffers for continuous data copy, so tooltip wait time is minimized
    std::array<gl::Buffer,      6> m_tooltip_buffers;
    std::array<gl::sync::Fence, 6> m_tooltip_fences;
    std::array<BaryVariant,     6> m_tooltip_maps;
    uint                           m_tooltip_cycle_i;

    // Local state
    eig::Array2i m_tooltip_pixel; // Selected pixel in tooltip

    // Delegating functions
    void eval_tooltip_copy(SchedulerHandle &info);
    void eval_tooltip(SchedulerHandle &info);

  public:
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };
} // namespace met