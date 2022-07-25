#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  struct GenerateGamutTask : public detail::AbstractTask {
    GenerateGamutTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met