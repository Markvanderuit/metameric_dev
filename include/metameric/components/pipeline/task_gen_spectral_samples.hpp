#pragma once

#include <metameric/core/spectrum.hpp>
#include <metameric/core/scheduler.hpp>

namespace met {
  class GenSpectralSamplesTask : public detail::AbstractTask {
  public:
    GenSpectralSamplesTask(const std::string &);
    void init(detail::TaskInitInfo &) override;
    void dstr(detail::TaskDstrInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met