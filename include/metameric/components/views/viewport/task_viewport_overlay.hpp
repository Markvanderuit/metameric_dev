#pragma once

#include <metameric/core/data.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/spectrum.hpp>

namespace met {
  class ViewportOverlayTask : public detail::TaskBase {
    Colr m_colr_prev;
    bool m_is_gizmo_used;     // Gizmo use state
    bool m_is_vert_edit_used; // Color edit use state
    bool m_is_cstr_edit_used; // Cnstr edit use state
    
  public:
    void init(detail::SchedulerHandle &info) override;
    void eval(detail::SchedulerHandle &info) override;

    void eval_overlay_vertex(detail::SchedulerHandle &info, uint i);
    void eval_overlay_sample(detail::SchedulerHandle &info, uint i);
    void eval_overlay_color_solid(detail::SchedulerHandle &info);
    void eval_overlay_plot(detail::SchedulerHandle &info);
  };
} // namespace met