#pragma once

#include <metameric/core/spectrum.hpp>
#include <metameric/core/detail/scheduler_task.hpp>

namespace met {
  class GenSpectralDataTask : public detail::AbstractTask {
    std::span<AlColr>       m_vert_map;
    std::span<eig::Array4u> m_tetr_map;

  public:
    GenSpectralDataTask(const std::string &);
    void init(detail::TaskInitInfo &) override;
    void dstr(detail::TaskDstrInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met