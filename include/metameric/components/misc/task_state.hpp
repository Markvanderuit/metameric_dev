#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/data.hpp>

namespace met {
  class StateTask : public detail::AbstractTask {
    // Copies of project data to check for and report state changes
    std::vector<ProjectData::Elem>            m_elems;
    std::vector<ProjectData::Vert>            m_verts;
    std::vector<ProjectData::Mapp>            m_mapps;
    std::vector<std::pair<std::string, CMFS>> m_cmfs;
    std::vector<std::pair<std::string, Spec>> m_illuminants;

    // Copies of view selection data to check for and report state changes
    std::vector<uint> m_vert_selct;
    std::vector<uint> m_vert_mover;
    std::vector<uint> m_elem_selct;
    std::vector<uint> m_elem_mover;
    int               m_cstr_selct;

  public:
    StateTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met