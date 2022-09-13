#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  class GamutEditorTask : public detail::AbstractTask {
  public:
    GamutEditorTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met