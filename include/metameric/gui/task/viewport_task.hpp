#pragma once

#include <metameric/gui/detail/linear_scheduler/task.hpp>
#include <vector>

namespace met {
  class ViewportTask : public detail::AbstractTask {
    std::vector<glm::vec2> m_click_positions;

  public:
    ViewportTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met