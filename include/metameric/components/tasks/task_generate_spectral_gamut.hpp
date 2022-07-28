#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  struct GenerateSpectralGamutTask : public detail::AbstractTask {
    GenerateSpectralGamutTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met