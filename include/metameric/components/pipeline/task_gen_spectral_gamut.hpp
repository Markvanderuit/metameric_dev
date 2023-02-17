#pragma once

#include <metameric/core/spectrum.hpp>
#include <metameric/core/detail/scheduler_task.hpp>

namespace met {
  class GenSpectralGamutTask : public detail::AbstractTask {
    std::span<Spec>           m_spec_map;
    std::span<AlColr>         m_vert_map;
    std::span<eig::AlArray3u> m_elem_map;

  public:
    GenSpectralGamutTask(const std::string &);
    void init(detail::TaskInitInfo &) override;
    void dstr(detail::TaskDstrInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met