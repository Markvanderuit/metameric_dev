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
    std::vector<Colr> m_gamut_colr_i;
    std::vector<Colr> m_gamut_offs_j;
    std::vector<uint> m_gamut_mapp_i;
    std::vector<uint> m_gamut_mapp_j;
    std::vector<Mapp> m_mappings;

  public:
    ProjectStateTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met