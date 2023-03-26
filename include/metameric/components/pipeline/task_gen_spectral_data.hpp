#pragma once

#include <metameric/core/spectrum.hpp>
#include <metameric/core/scheduler.hpp>

namespace met {
  class GenSpectralDataTask : public detail::TaskNode {
    std::span<AlColr>       m_vert_map;
    std::span<eig::Array4u> m_tetr_map;

  public:
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
    bool is_active(SchedulerHandle &) override;
  };
} // namespace met