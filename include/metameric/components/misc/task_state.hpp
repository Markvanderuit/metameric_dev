#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/data.hpp>

namespace met {
  class StateTask : public detail::AbstractTask {
    // Copies of data available in ProjectData and ApplicationData
    // to check for and report state changes
    std::vector<ProjectData::Elem> m_elems;
    std::vector<ProjectData::Vert> m_verts;
    std::vector<Mapp>              m_mapps;

  public:
    StateTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met