#include <metameric/core/detail/trace.hpp>
#include <metameric/components/misc/task_project_state.hpp>

namespace met {
  namespace detail {
    constexpr 
    auto compare_and_set(const auto &a, auto &b, auto &state) {
      if (b != a) {
        b = a;
        state = CacheState::eStale;
      } else {
        state = CacheState::eFresh;
      }
    }

    constexpr 
    auto compare_and_set_eig(const auto &a, auto &b, auto &state) {
      if (!b.isApprox(a)) {
        b = a;
        state = CacheState::eStale;
      } else {
        state = CacheState::eFresh;
      }
    }

    constexpr
    auto compare_and_set_all(auto &v_state, auto &v_out, const auto &v_in) {
      if (v_out.size() != v_in.size()) {
        v_out = v_in;
        v_state = std::vector<CacheState>(v_out.size(), CacheState::eStale);
      } else {
        for (uint i = 0; i < v_out.size(); ++i) {
          compare_and_set(v_in[i], v_out[i], v_state[i]);
        }
      }
    }

    constexpr
    auto compare_and_set_all_eig(auto &v_state, auto &v_out, const auto &v_in) {
      if (v_out.size() != v_in.size()) {
        v_out = v_in;
        v_state = std::vector<CacheState>(v_out.size(), CacheState::eStale);
      } else {
        for (uint i = 0; i < v_out.size(); ++i) {
          compare_and_set_eig(v_in[i], v_out[i], v_state[i]);
        }
      }
    }
  }

  ProjectStateTask::ProjectStateTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void ProjectStateTask::init(detail::TaskInitInfo &info) {
    met_trace();
    
    // Submit shared resource values as initially stale
    info.insert_resource<std::vector<CacheState>>("mappings",      { });
    info.insert_resource<std::vector<CacheState>>("gamut_elems",   { });
    info.insert_resource<std::vector<CacheState>>("gamut_colr_i",  { });
    info.insert_resource<std::vector<CacheState>>("gamut_offs_j",  { });
    info.insert_resource<std::vector<CacheState>>("gamut_mapp_i",  { });
    info.insert_resource<std::vector<CacheState>>("gamut_mapp_j",  { });
    info.insert_resource<std::vector<CacheState>>("gamut_summary", { });
  }

  void ProjectStateTask::eval(detail::TaskEvalInfo &info) {
    met_trace();
    
    // Get shared resources
    auto &e_app_data            = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data           = e_app_data.project_data;
    auto &i_state_mapp          = info.get_resource<std::vector<CacheState>>("mappings");
    auto &i_state_gamut_elems   = info.get_resource<std::vector<CacheState>>("gamut_elems");
    auto &i_state_gamut_colr_i  = info.get_resource<std::vector<CacheState>>("gamut_colr_i");
    auto &i_state_gamut_offs_j  = info.get_resource<std::vector<CacheState>>("gamut_offs_j");
    auto &i_state_gamut_mapp_i  = info.get_resource<std::vector<CacheState>>("gamut_mapp_i");
    auto &i_state_gamut_mapp_j  = info.get_resource<std::vector<CacheState>>("gamut_mapp_j");
    auto &i_state_gamut         = info.get_resource<std::vector<CacheState>>("gamut_summary");

    // Check and set cache states for loaded mappings to either fresh or stale
    detail::compare_and_set_all(i_state_mapp, m_mappings, e_app_data.loaded_mappings);
    detail::compare_and_set_all_eig(i_state_gamut_elems, m_gamut_elems, e_app_data.project_data.gamut_elems);
    detail::compare_and_set_all_eig(i_state_gamut_colr_i, m_gamut_colr_i, e_app_data.project_data.gamut_colr_i);
    detail::compare_and_set_all_eig(i_state_gamut_offs_j, m_gamut_offs_j, e_app_data.project_data.gamut_offs_j);
    detail::compare_and_set_all(i_state_gamut_mapp_i, m_gamut_mapp_i, e_app_data.project_data.gamut_mapp_i);
    detail::compare_and_set_all(i_state_gamut_mapp_j, m_gamut_mapp_j, e_app_data.project_data.gamut_mapp_j);
    
    i_state_gamut = std::vector<CacheState>(i_state_gamut_colr_i.size());
    // fmt::print("i_state_gamut.size() = {}\n", i_state_gamut.size());
    for (uint i = 0; i < i_state_gamut.size(); ++i) {
      // Summary data for parts of the application interested in "any" change to gamut data
      const bool gamut_stale = i_state_gamut_colr_i[i] == CacheState::eStale || i_state_gamut_offs_j[i] == CacheState::eStale 
                            || i_state_gamut_mapp_i[i] == CacheState::eStale || i_state_gamut_mapp_j[i] == CacheState::eStale
                            || i_state_mapp[e_proj_data.gamut_mapp_i[i]] == CacheState::eStale
                            || i_state_mapp[e_proj_data.gamut_mapp_j[i]] == CacheState::eStale;
      i_state_gamut[i] = gamut_stale ? CacheState::eStale : CacheState::eFresh;
      // fmt::print("i_state_gamut[{}] = {}\n", i, gamut_stale ? "stale" : "fresh");
    }

    /* if (m_mappings.size() != e_app_data.loaded_mappings.size()) {
      // Size changed; just invalidate the whole thing
      m_mappings = e_app_data.loaded_mappings;
      i_state_mapp = std::vector<CacheState>(m_mappings.size(), CacheState::eStale);
    } else {
      // Size is the same; check mappings it on a case by case basis
      for (uint i = 0; i < m_mappings.size(); ++i) {
        detail::compare_and_set(e_app_data.loaded_mappings[i], m_mappings[i], i_state_mapp[i]);
      }
    } */

    // Check and set cache states for gamut vertex data to either fresh or stale
    // #pragma omp parallel for
    /* for (int i = 0; i < e_proj_data.gamut_colr_i.size(); ++i) {
      detail::compare_and_set_eig(e_proj_data.gamut_colr_i[i], m_gamut_colr_i[i], i_state_gamut_colr_i[i]);
      detail::compare_and_set_eig(e_proj_data.gamut_offs_j[i], m_gamut_offs_j[i], i_state_gamut_offs_j[i]);
      detail::compare_and_set(e_proj_data.gamut_mapp_i[i], m_gamut_mapp_i[i], i_state_gamut_mapp_i[i]);
      detail::compare_and_set(e_proj_data.gamut_mapp_j[i], m_gamut_mapp_j[i], i_state_gamut_mapp_j[i]);

      // Summary data for parts of the application interested in "any" change to gamut data
      const bool gamut_stale = i_state_gamut_colr_i[i] == CacheState::eStale 
                            || i_state_gamut_offs_j[i] == CacheState::eStale 
                            || i_state_gamut_mapp_i[i] == CacheState::eStale 
                            || i_state_gamut_mapp_j[i] == CacheState::eStale
                            || i_state_mapp[e_proj_data.gamut_mapp_i[i]] == CacheState::eStale
                            || i_state_mapp[e_proj_data.gamut_mapp_j[i]] == CacheState::eStale;
      i_state_gamut[i] = gamut_stale ? CacheState::eStale : CacheState::eFresh;
    } */
  }
} // namespace met