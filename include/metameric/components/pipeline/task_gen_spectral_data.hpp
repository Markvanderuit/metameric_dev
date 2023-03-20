#pragma once

#include <metameric/core/spectrum.hpp>
#include <metameric/core/scheduler.hpp>

namespace met {
  class GenSpectralDataTask : public detail::TaskBase {
    std::span<AlColr>       m_vert_map;
    std::span<eig::Array4u> m_tetr_map;

  public:
    void init(detail::SchedulerHandle &) override;
    void eval(detail::SchedulerHandle &) override;
  };
} // namespace met