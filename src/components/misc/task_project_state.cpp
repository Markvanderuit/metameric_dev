#include <metameric/core/detail/trace.hpp>
#include <metameric/components/misc/task_project_state.hpp>

namespace met {
  namespace detail {
    constexpr 
    auto compare_and_set(const auto &a, auto &b, auto &state) {
      met_trace();
      if (b != a) {
        b = a;
        state = CacheState::eStale;
      } else {
        state = CacheState::eFresh;
      }
    }

    constexpr 
    auto compare_and_set_v(const auto &a, auto &b, auto &state) {
      met_trace();
      if (!b.isApprox(a)) {
        b = a;
        state = CacheState::eStale;
      } else {
        state = CacheState::eFresh;
      }
    }
  }

  ProjectStateTask::ProjectStateTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void ProjectStateTask::init(detail::TaskInitInfo &info) {
    met_trace();

    GamutArray gamut_stale = { CacheState::eStale, CacheState::eStale, CacheState::eStale, CacheState::eStale };

    // Get shared resources
    const auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    const auto &e_mappings = e_app_data.loaded_mappings;
    
    // Submit shared resource values as initially stale
    info.insert_resource<GamutArray>("gamut_colr_i",  GamutArray(gamut_stale));
    info.insert_resource<GamutArray>("gamut_offs_j",  GamutArray(gamut_stale));
    info.insert_resource<GamutArray>("gamut_mapp_i",  GamutArray(gamut_stale));
    info.insert_resource<GamutArray>("gamut_mapp_j",  GamutArray(gamut_stale));
    info.insert_resource<GamutArray>("gamut_spec",    GamutArray(gamut_stale));
    info.insert_resource<GamutArray>("gamut_summary", GamutArray(gamut_stale));
    info.insert_resource<std::vector<CacheState>>("mappings", std::vector<CacheState>());
  }

  void ProjectStateTask::eval(detail::TaskEvalInfo &info) {
    met_trace();
    
    // Get shared resources
    const auto &e_app_data      = info.get_resource<ApplicationData>(global_key, "app_data");
    const auto &e_proj_data     = e_app_data.project_data;
    const auto &e_mappings      = e_app_data.loaded_mappings;
    auto &i_state_gamut_colr_i  = info.get_resource<GamutArray>("gamut_colr_i");
    auto &i_state_gamut_colr_j  = info.get_resource<GamutArray>("gamut_offs_j");
    auto &i_state_gamut_mapp_i  = info.get_resource<GamutArray>("gamut_mapp_i");
    auto &i_state_gamut_mapp_j  = info.get_resource<GamutArray>("gamut_mapp_j");
    auto &i_state_gamut_spec    = info.get_resource<GamutArray>("gamut_spec");
    auto &i_state_gamut_summary = info.get_resource<GamutArray>("gamut_summary");
    auto &i_state_mappings      = info.get_resource<std::vector<CacheState>>("mappings");

    // Check and set cache states for gamut vertex data to either fresh or stale
    // #pragma omp parallel for
    for (int i = 0; i < 4; ++i) {
      detail::compare_and_set_v(e_proj_data.gamut_colr_i[i], m_gamut_colr_i[i], i_state_gamut_colr_i[i]);
      detail::compare_and_set_v(e_proj_data.gamut_offs_j[i], m_gamut_offs_j[i], i_state_gamut_colr_j[i]);
      detail::compare_and_set_v(e_proj_data.gamut_spec[i], m_gamut_spec[i], i_state_gamut_spec[i]);
      detail::compare_and_set(e_proj_data.gamut_mapp_i[i], m_gamut_mapp_i[i], i_state_gamut_mapp_i[i]);
      detail::compare_and_set(e_proj_data.gamut_mapp_j[i], m_gamut_mapp_j[i], i_state_gamut_mapp_j[i]);

      // Summary data for parts of the application interested in "any" change to gamut data
      const bool gamut_stale = i_state_gamut_colr_i[i] == CacheState::eStale || i_state_gamut_colr_j[i] == CacheState::eStale 
                            || i_state_gamut_mapp_i[i] == CacheState::eStale || i_state_gamut_mapp_j[i] == CacheState::eStale;
      i_state_gamut_summary[i] = gamut_stale ? CacheState::eStale : CacheState::eFresh;
    }

    // Check and set cache states for loaded mappings to either fresh or stale
    if (m_mappings.size() != e_mappings.size()) {
      // Size changed; just invalidate the whole thing
      m_mappings = e_mappings;
      i_state_mappings = std::vector<CacheState>(e_mappings.size(), CacheState::eStale);
    } else {
      // Size is the same; check mappings it on a case by case basis
      for (uint i = 0; i < m_mappings.size(); ++i) {
        detail::compare_and_set(e_mappings[i], m_mappings[i], i_state_mappings[i]);
      }
    }
  }
} // namespace met