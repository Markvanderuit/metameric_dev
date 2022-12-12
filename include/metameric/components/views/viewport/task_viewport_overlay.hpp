#pragma once

#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/detail/scheduler_task.hpp>

namespace met {
  class ViewportOverlayTask : public detail::AbstractTask {
    std::vector<Colr> m_offs_prev;
    bool              m_is_gizmo_used;
    
  public:
    ViewportOverlayTask(const std::string &name);

    void init(detail::TaskInitInfo &info) override;
    void dstr(detail::TaskDstrInfo &info) override;
    void eval(detail::TaskEvalInfo &info) override;

    void eval_overlay_vertex(detail::TaskEvalInfo &info, uint i);
    void eval_overlay_color_solid(detail::TaskEvalInfo &info);
    void eval_overlay_weights(detail::TaskEvalInfo &info);
  };
} // namespace met