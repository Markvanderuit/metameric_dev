#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/state.hpp>
#include <array>
#include <vector>

namespace met {
  class ProjectStateTask : public detail::AbstractTask {
    using GamutArray  = std::array<CacheState, 4>;

    // Copies of data available in ProjectData and ApplicationData
    // to check for and report state changes
    std::array<Colr, 4> m_gamut_colr_i;
    std::array<Colr, 4> m_gamut_colr_j;
    std::array<uint, 4> m_gamut_mapp_i;
    std::array<uint, 4> m_gamut_mapp_j;
    std::array<Spec, 4> m_gamut_spec;
    std::vector<Mapp>   m_mappings;

  public:
    ProjectStateTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met