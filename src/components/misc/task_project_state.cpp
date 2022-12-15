#include <metameric/core/detail/trace.hpp>
#include <metameric/components/misc/task_project_state.hpp>
#include <algorithm>
#include <numeric>
#include <ranges>

namespace met {
  namespace detail {
    using Vert      = ProjectData::Vert;
    using CacheVert = ProjectState::CacheVert;
    
    constexpr 
    CacheFlag compare_and_set(const auto &in, auto &out) {
      guard(out != in, CacheFlag::eFresh);
      out = in;
      return CacheFlag::eStale;
    }

    constexpr 
    CacheFlag compare_and_set_eig(const auto &in, auto &out) {
      guard(!out.isApprox(in), CacheFlag::eFresh);
      out = in;
      return CacheFlag::eStale;
    }

    constexpr
    std::vector<CacheFlag> compare_and_set_all(const auto &in,
                                                     auto &out) {
      std::vector<CacheFlag> state(in.size(), CacheFlag::eStale);
      if (in.size() != out.size()) {
        out = in;
        return state;
      }
      for (uint i = 0; i < in.size(); ++i)
        state[i] = compare_and_set(in[i], out[i]);
      return state;
    }

    constexpr
    std::vector<CacheFlag> compare_and_set_all_eig(const auto &in,
                                                         auto &out) {
      std::vector<CacheFlag> state(in.size(), CacheFlag::eStale);
      if (in.size() != out.size()) {
        out = in;
        return state;
      }
      for (uint i = 0; i < in.size(); ++i)
        state[i] = compare_and_set_eig(in[i], out[i]);
      return state;
    }

    constexpr
    CacheVert compare_and_set_vert(const Vert  &in,
                                         Vert  &out) {
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
      if (in.size() != out.size()) {
        out.resize(in.size());
      }
      for (uint i = 0; i < in.size(); ++i)
        state[i] = compare_and_set_vert(in[i], out[i]);
      return state;
    }
  }

  ProjectStateTask::ProjectStateTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void ProjectStateTask::init(detail::TaskInitInfo &info) {
    met_trace();
    
    // TODO: deprecate
    info.insert_resource<std::vector<CacheFlag>>("mappings",      { });
    info.insert_resource<std::vector<CacheFlag>>("gamut_elems",   { });
    info.insert_resource<std::vector<CacheFlag>>("gamut_colr_i",  { });
    info.insert_resource<std::vector<CacheFlag>>("gamut_offs_j",  { });
    info.insert_resource<std::vector<CacheFlag>>("gamut_mapp_i",  { });
    info.insert_resource<std::vector<CacheFlag>>("gamut_mapp_j",  { });
    info.insert_resource<std::vector<CacheFlag>>("gamut_summary", { });
  }

  void ProjectStateTask::eval(detail::TaskEvalInfo &info) {
    met_trace();

    constexpr auto reduce_stale = [](auto a, auto b) { return a & b; };

    // Get shared resources
    auto &e_app_data   = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data  = e_app_data.project_data;
    auto &e_proj_state = e_app_data.project_state;

    // Iterate over all project data
    e_proj_state.verts = detail::compare_and_set_all_vert(e_proj_data.gamut_verts, m_verts);
    e_proj_state.elems = detail::compare_and_set_all_eig(e_proj_data.gamut_elems, m_elems);
    e_proj_state.mapps = detail::compare_and_set_all(e_app_data.loaded_mappings, m_mapps);

    // Post-process fill in some gaps 
    for (uint i = 0; i < e_proj_state.verts.size(); ++i) {
      auto &state = e_proj_state.verts[i];
      auto &vert  = e_proj_data.gamut_verts[i];
      
      state.mapp_i &= e_proj_state.mapps[vert.mapp_i];
      for (uint j = 0; j < state.mapp_j.size(); ++j)
        state.mapp_j[j] &= e_proj_state.mapps[vert.mapp_j[j]];
      
      state.any_colr_j = std::reduce(range_iter(state.colr_j), CacheFlag::eFresh, reduce_stale);
      state.any_mapp_j = std::reduce(range_iter(state.mapp_j), CacheFlag::eFresh, reduce_stale);
      state.any = CacheFlag::eFresh & state.any_colr_j & state.any_mapp_j;
    }

    // Set summary flags
    e_proj_state.any_mapps = std::reduce(range_iter(e_proj_state.mapps), CacheFlag::eFresh, reduce_stale);
    e_proj_state.any_elems = std::reduce(range_iter(e_proj_state.elems), CacheFlag::eFresh, reduce_stale);
    e_proj_state.any_verts = std::reduce(range_iter(e_proj_state.verts), CacheFlag::eFresh, 
      [](const auto &a, const auto &b) { return a & b.any; });
    e_proj_state.any = e_proj_state.any_mapps | e_proj_state.any_elems | e_proj_state.any_verts;

    // TODO: deprecate
    auto &i_state_mapp          = info.get_resource<std::vector<CacheFlag>>("mappings");
    auto &i_state_gamut_elems   = info.get_resource<std::vector<CacheFlag>>("gamut_elems");
    auto &i_state_gamut_colr_i  = info.get_resource<std::vector<CacheFlag>>("gamut_colr_i");
    auto &i_state_gamut_offs_j  = info.get_resource<std::vector<CacheFlag>>("gamut_offs_j");
    auto &i_state_gamut_mapp_i  = info.get_resource<std::vector<CacheFlag>>("gamut_mapp_i");
    auto &i_state_gamut_mapp_j  = info.get_resource<std::vector<CacheFlag>>("gamut_mapp_j");
    auto &i_state_gamut         = info.get_resource<std::vector<CacheFlag>>("gamut_summary");

    // Check and set cache states for loaded mappings to either fresh or stale
    i_state_mapp         = detail::compare_and_set_all(e_app_data.loaded_mappings, m_mappings);
    i_state_gamut_elems  = detail::compare_and_set_all_eig(e_app_data.project_data.gamut_elems,  m_gamut_elems);
    i_state_gamut_colr_i = detail::compare_and_set_all_eig(e_app_data.project_data.gamut_colr_i, m_gamut_colr_i);
    i_state_gamut_offs_j = detail::compare_and_set_all_eig(e_app_data.project_data.gamut_offs_j, m_gamut_offs_j);
    i_state_gamut_mapp_i = detail::compare_and_set_all(e_app_data.project_data.gamut_mapp_i, m_gamut_mapp_i);
    i_state_gamut_mapp_j = detail::compare_and_set_all(e_app_data.project_data.gamut_mapp_j, m_gamut_mapp_j);
    
    // Flag vertex data for stale mappings as, well, stale
    for (uint i = 0; i < e_proj_data.gamut_mapp_i.size(); ++i) {
      i_state_gamut_mapp_i[i] &= i_state_mapp[e_proj_data.gamut_mapp_i[i]];
      i_state_gamut_mapp_j[i] &= i_state_mapp[e_proj_data.gamut_mapp_j[i]];
    }

    // Compute summary flag for parts of the application interested in "any" change to gamut data
    i_state_gamut = std::vector<CacheFlag>(i_state_gamut_colr_i.size());
    for (uint i = 0; i < i_state_gamut.size(); ++i) {
      const bool gamut_stale = i_state_gamut_colr_i[i] == CacheFlag::eStale 
                            || i_state_gamut_offs_j[i] == CacheFlag::eStale 
                            || i_state_gamut_mapp_i[i] == CacheFlag::eStale 
                            || i_state_gamut_mapp_j[i] == CacheFlag::eStale;
      i_state_gamut[i] = gamut_stale ? CacheFlag::eStale : CacheFlag::eFresh;
    }
  }
} // namespace met