#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  struct GenSpectralGamutTask : public detail::AbstractTask {
    GenSpectralGamutTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void dstr(detail::TaskDstrInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met