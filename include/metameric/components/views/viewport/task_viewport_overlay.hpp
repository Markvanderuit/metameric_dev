#pragma once

#include <metameric/core/spectrum.hpp>
#include <metameric/core/data.hpp>
#include <metameric/core/detail/scheduler_task.hpp>

namespace met {
  class ViewportOverlayTask : public detail::AbstractTask {
    Colr m_colr_prev;
    bool m_is_gizmo_used;     // Gizmo use state
    bool m_is_vert_edit_used; // Color edit use state
    bool m_is_cstr_edit_used; // Cnstr edit use state
    
  public:
    ViewportOverlayTask(const std::string &name);

    void init(detail::TaskInfo &info) override;
    void dstr(detail::TaskInfo &info) override;
    void eval(detail::TaskInfo &info) override;

    void eval_overlay_vertex(detail::TaskInfo &info, uint i);
    void eval_overlay_sample(detail::TaskInfo &info, uint i);
    void eval_overlay_color_solid(detail::TaskInfo &info);
    void eval_overlay_plot(detail::TaskInfo &info);
  };
} // namespace met