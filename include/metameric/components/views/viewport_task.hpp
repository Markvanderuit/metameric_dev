#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/spectrum.hpp>
#include <vector>
#include <array>

namespace met {
  class ViewportTask : public detail::AbstractTask {
    
    bool                 m_is_gizmo_used;
    std::array<Color, 4> m_gamut_prev;
    Color                m_gamut_average;
    std::vector<uint>    m_gamut_selection_indices;

    void eval_camera(detail::TaskEvalInfo &);
    void eval_select(detail::TaskEvalInfo &);
    void draw_gizmo(detail::TaskEvalInfo &);
    void eval_gizmo(detail::TaskEvalInfo &);

  public:
    ViewportTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void dstr(detail::TaskDstrInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met