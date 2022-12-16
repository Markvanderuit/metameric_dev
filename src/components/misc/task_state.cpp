#include <metameric/core/state.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/misc/task_state.hpp>
#include <numeric>

namespace met {
  namespace detail {
    using Vert      = ProjectData::Vert;
    using CacheVert = ProjectState::CacheVert;
    
    constexpr 
    ProjectState::CacheStale compare_and_set(const auto &in, auto &out) {
      guard(out != in, false);
      out = in;
      return true;
    }

    constexpr 
    ProjectState::CacheStale compare_and_set_eig(const auto &in, auto &out) {
      guard(!out.isApprox(in), false);
      out = in;
      return true;
    }

    constexpr
    std::vector<ProjectState::CacheStale> compare_and_set_all(const auto &in,
                                                     auto &out) {
      std::vector<ProjectState::CacheStale> state(in.size(), true);
      if (in.size() != out.size()) {
        out = in;
        return state;
      }
      for (uint i = 0; i < in.size(); ++i)
        state[i] = compare_and_set(in[i], out[i]);
      return state;
    }

    constexpr
    std::vector<ProjectState::CacheStale> compare_and_set_all_eig(const auto &in,
                                                         auto &out) {
      std::vector<ProjectState::CacheStale> state(in.size(), true);
      if (in.size() != out.size()) {
        out = in;
        return state;
      }
      for (uint i = 0; i < in.size(); ++i)
        state[i] = compare_and_set_eig(in[i], out[i]);
      return state;
    }

    constexpr
    CacheVert compare_and_set_vert(const Vert &in,
                                         Vert &out) {
      CacheVert state;
      state.colr_i = compare_and_set_eig(in.colr_i, out.colr_i);
      state.mapp_i = compare_and_set(in.mapp_i, out.mapp_i);
      state.colr_j = compare_and_set_all_eig(in.colr_j, out.colr_j);
      state.mapp_j = compare_and_set_all(in.mapp_j, out.mapp_j);
      return state;
    }

    constexpr
    std::vector<CacheVert> compare_and_set_all_vert(const std::vector<Vert> &in,
                                                          std::vector<Vert> &out) {
      std::vector<CacheVert> state(in.size());
      if (in.size() != out.size())
        out.resize(in.size());
      for (uint i = 0; i < in.size(); ++i)
        state[i] = compare_and_set_vert(in[i], out[i]);
      return state;
    }
  }
  
  PipelineStateTask::PipelineStateTask(const std::string &name)
  : detail::AbstractTask(name) { }
  
  void PipelineStateTask::init(detail::TaskInitInfo &info) {
    met_trace();
    info.insert_resource<ProjectState>("pipeline_state", { });
  }

  void PipelineStateTask::eval(detail::TaskEvalInfo &info) {
    met_trace();

    constexpr auto reduce_stale = [](auto a, auto b) { return a | b; };

    // Get shared resources
    auto &i_pipe_state = info.get_resource<ProjectState>("pipeline_state");
    auto &e_appl_data  = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data  = e_appl_data.project_data;

    // Iterate over all project data
    i_pipe_state.verts = detail::compare_and_set_all_vert(e_proj_data.gamut_verts, m_verts);
    i_pipe_state.elems = detail::compare_and_set_all_eig(e_proj_data.gamut_elems, m_elems);
    i_pipe_state.mapps = detail::compare_and_set_all(e_appl_data.loaded_mappings, m_mapps);

    // Post-process fill in some gaps 
    for (uint i = 0; i < i_pipe_state.verts.size(); ++i) {
      auto &vert_state = i_pipe_state.verts[i];
      auto &vert_data  = e_proj_data.gamut_verts[i];
      
      // If mapping state has become stale, this influenced the flag inside of a vertex as well
      vert_state.mapp_i |= i_pipe_state.mapps[vert_data.mapp_i];
      for (uint j = 0; j < vert_state.mapp_j.size(); ++j) {
        vert_state.mapp_j[j] = vert_state.mapp_j[j] | i_pipe_state.mapps[vert_data.mapp_j[j]];
      }
      
      // Set summary flags per vertex
      vert_state.any_colr_j = std::reduce(range_iter(vert_state.colr_j), false, reduce_stale);
      vert_state.any_mapp_j = std::reduce(range_iter(vert_state.mapp_j), false, reduce_stale);
      vert_state.any = vert_state.colr_i | vert_state.mapp_i | vert_state.any_colr_j | vert_state.any_mapp_j;
    }

    // Set summary flags over all vertices/elements
    i_pipe_state.any_mapps = std::reduce(range_iter(i_pipe_state.mapps), false, reduce_stale);
    i_pipe_state.any_elems = std::reduce(range_iter(i_pipe_state.elems), false, reduce_stale);
    i_pipe_state.any_verts = std::reduce(range_iter(i_pipe_state.verts), false, 
      [](const auto &a, const auto &b) { return a | b.any; });
    i_pipe_state.any = i_pipe_state.any_mapps | i_pipe_state.any_elems | i_pipe_state.any_verts;
  }
} // namespace met