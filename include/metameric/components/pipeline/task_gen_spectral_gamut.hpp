#pragma once

#include <metameric/core/spectrum.hpp>
#include <metameric/core/scheduler.hpp>

namespace met {
  struct GenSpectralGamutTask : public detail::AbstractTask {
    Colr c = 0.f;

    GenSpectralGamutTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void dstr(detail::TaskDstrInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met