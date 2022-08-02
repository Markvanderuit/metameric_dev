#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  class SpectraEditorTask : public detail::AbstractTask {
    // ...

  public:
    SpectraEditorTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met
