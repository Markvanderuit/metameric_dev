#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/state.hpp>
#include <array>
#include <vector>

namespace met {
  class ProjectStateTask : public detail::AbstractTask {
    // Copies of data available in ProjectData and ApplicationData
    // to check for and report state changes
    std::vector<ProjectData::Elem> m_elems;
    std::vector<ProjectData::Vert> m_verts;
    std::vector<Mapp>              m_mapps;

  public:
    ProjectStateTask(const std::string &name);
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met