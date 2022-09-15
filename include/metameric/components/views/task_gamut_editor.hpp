#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/spectrum.hpp>
#include <array>

namespace met {
  class GamutEditorTask : public detail::AbstractTask {
    bool m_is_gizmo_used;
    std::array<Colr, 4>  m_offs_prev;

    void eval_camera(detail::TaskEvalInfo &);
    void eval_gizmo(detail::TaskEvalInfo &);

  public:
    GamutEditorTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void dstr(detail::TaskDstrInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met