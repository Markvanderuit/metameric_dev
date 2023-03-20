#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/data.hpp>

namespace met {
  class StateTask : public detail::TaskBase {
    // Copies of project data to check for and report state changes
    std::vector<ProjectData::Vert>            m_verts;
    std::vector<ProjectData::CSys>            m_csys;
    std::vector<std::pair<std::string, CMFS>> m_cmfs;
    std::vector<std::pair<std::string, Spec>> m_illuminants;

    // Copies of viewport camera data to check for and report state changes
    eig::Matrix4f m_camera_matrix;
    float         m_camera_aspect;

    // Copies of view selection data to check for and report state changes
    std::vector<uint> m_vert_selct;
    std::vector<uint> m_vert_mover;
    std::vector<uint> m_samp_selct;
    std::vector<uint> m_samp_mover;
    int               m_cstr_selct;

  public:
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };
} // namespace met