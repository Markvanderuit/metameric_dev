#pragma once

#include <metameric/core/scheduler.hpp>
#include <vector>

namespace met {
  class ViewportTask : public detail::AbstractTask {
    std::vector<uint> m_gamut_selection_indices;

  public:
    ViewportTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met