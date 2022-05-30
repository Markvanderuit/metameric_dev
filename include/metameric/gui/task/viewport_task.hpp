#pragma once

#include <metameric/gui/detail/linear_scheduler/task.hpp>
#include <glm/vec3.hpp>
#include <ranges>
#include <vector>

namespace met {
  class ViewportTask : public detail::AbstractTask {
    std::vector<uint> m_gamut_selection_indices;
    glm::vec3 m_gamut_anchor_pos;

  public:
    ViewportTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met