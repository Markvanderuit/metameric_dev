#pragma once

#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <metameric/components/views/viewport/task_draw_color_solid.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <ImGuizmo.h>
#include <numeric>
#include <ranges>

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