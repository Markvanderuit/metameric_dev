#include <metameric/core/state.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/misc/task_state.hpp>
#include <functional>
#include <numeric>
#include <tuple>
#include <type_traits>

namespace met {
  namespace detail {
    using CacheVert = ProjectState::CacheVert;
    using CompareTuple = std::tuple<std::vector<bool>, bool>;

    template <typename T>
    bool compare_func(const T &in, T &out) {
      guard(out != in, false);
      out = in;
      return true;
    }

    template <typename T, typename T_>
    bool compare_func(const std::pair<T, T_> &in, std::pair<T, T_> &out) {
      return compare_func(in.first, out.first) || compare_func(in.second, out.second);
    }

    template <>
    bool compare_func<ProjectData::Mapp>(const ProjectData::Mapp &in, ProjectData::Mapp &out) {
      guard(in.cmfs != out.cmfs || in.illuminant != out.illuminant, false);
      out = in;
      return true;
    }

    template <>
    bool compare_func<CMFS>(const CMFS &in, CMFS &out) {
      guard(!out.isApprox(in), false);
      out = in;
      return true;
    }

    template <>
    bool compare_func<Spec>(const Spec &in, Spec &out) {
      guard(!out.isApprox(in), false);
      out = in;
      return true;
    }

    template <>
    bool compare_func<eig::Array3f>(const eig::Array3f &in, eig::Array3f &out) {
      guard(!out.isApprox(in), false);
      out = in;
      return true;
    }

    template <>
    bool compare_func<eig::Array3u>(const eig::Array3u &in, eig::Array3u &out) {
      guard(!out.isApprox(in), false);
      out = in;
      return true;
    }

    template <typename T>
    CompareTuple compare_state(const std::vector<T> &in,
                                          std::vector<T> &out) {
      std::vector<bool> state(in.size(), true);
      if (in.size() != out.size()) {
        out = in;
        return { state, true };
      }
      for (uint i = 0; i < in.size(); ++i)
        state[i] = compare_func(in[i], out[i]);
      return { state,  std::reduce(range_iter(state), false, std::logical_or<bool>()) };
    }
    
    constexpr 
    bool compare_and_set(const auto &in, auto &out) {
      guard(out != in, false);
      out = in;
      return true;
    }

    constexpr 
    bool compare_and_set_eig(const auto &in, auto &out) {
      guard(!out.isApprox(in), false);
      out = in;
      return true;
    }

    constexpr
    bool compare_and_set_reduce(const auto &in, auto &out) {
      if (in.size() != out.size()) {
        out = in;
        return true;
      }
      bool state = false;
      for (uint i = 0; i < in.size(); ++i)
        state |= compare_and_set(in[i], out[i]);
      return state;
    }

    constexpr
    std::vector<bool> compare_and_set_all(const auto &in, auto &out) {
      std::vector<bool> state(in.size(), true);
      if (in.size() != out.size()) {
        out = in;
        return state;
      }
      for (uint i = 0; i < in.size(); ++i)
        state[i] = compare_and_set(in[i], out[i]);
      return state;
    }

    constexpr
    std::vector<bool> compare_and_set_all_eig(const auto &in, auto &out) {
      std::vector<bool> state(in.size(), true);
      if (in.size() != out.size()) {
        out = in;
        return state;
      }
      for (uint i = 0; i < in.size(); ++i)
        state[i] = compare_and_set_eig(in[i], out[i]);
      return state;
    }

    CacheVert compare_and_set_vert(const ProjectData::Vert &in, ProjectData::Vert &out) {
      CacheVert state;
      state.colr_i = compare_func(in.colr_i, out.colr_i);
      state.mapp_i = compare_func(in.mapp_i, out.mapp_i);
      state.colr_j = std::get<0>(compare_state(in.colr_j, out.colr_j));
      state.mapp_j = std::get<0>(compare_state(in.mapp_j, out.mapp_j));
      return state;
    }

    std::vector<CacheVert> compare_and_set_all_vert(const std::vector<ProjectData::Vert> &in,
                                                          std::vector<ProjectData::Vert> &out) {
      std::vector<CacheVert> state(in.size());
      if (in.size() != out.size())
        out.resize(in.size());
      for (uint i = 0; i < in.size(); ++i)
        state[i] = compare_and_set_vert(in[i], out[i]);
      return state;
    }
  }
  
  StateTask::StateTask(const std::string &name)
  : detail::AbstractTask(name) { }
  
  void StateTask::init(detail::TaskInitInfo &info) {
    met_trace();
    info.insert_resource<ProjectState>("pipeline_state",  { });
    info.insert_resource<ViewportState>("viewport_state", { });
  }

  void StateTask::eval(detail::TaskEvalInfo &info) {
    met_trace();

    constexpr auto reduce_stale = [](auto a, auto b) { return a | b; };

    // Get shared resources
    auto &i_pipe_state = info.get_resource<ProjectState>("pipeline_state");
    auto &i_view_state = info.get_resource<ViewportState>("viewport_state");
    auto &e_appl_data  = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data  = e_appl_data.project_data;
    auto &e_vert_selct = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection");
    auto &e_vert_mover = info.get_resource<std::vector<uint>>("viewport_input_vert", "mouseover");
    auto &e_elem_selct = info.get_resource<std::vector<uint>>("viewport_input_elem", "selection");
    auto &e_elem_mover = info.get_resource<std::vector<uint>>("viewport_input_elem", "mouseover");
    auto &e_cstr_selct = info.get_resource<int>("viewport_overlay", "constr_selection");

    // Iterate over all project data
    i_pipe_state.verts = detail::compare_and_set_all_vert(e_proj_data.gamut_verts, m_verts);
    std::tie(i_pipe_state.illuminants, i_pipe_state.any_illuminants) = detail::compare_state(e_proj_data.illuminants, m_illuminants);
    std::tie(i_pipe_state.cmfs,  i_pipe_state.any_cmfs)  = detail::compare_state(e_proj_data.cmfs, m_cmfs);
    std::tie(i_pipe_state.elems, i_pipe_state.any_elems) = detail::compare_state(e_proj_data.gamut_elems, m_elems);
    std::tie(i_pipe_state.mapps, i_pipe_state.any_mapps) = detail::compare_state(e_proj_data.mappings, m_mapps);

    // Post-process fill in some gaps in project state
    for (uint i = 0; i < i_pipe_state.verts.size(); ++i) {
      auto &vert_state = i_pipe_state.verts[i];
      auto &vert_data  = e_proj_data.gamut_verts[i];
      
      // If mapping state has become stale, this influenced the flag inside of a vertex as well
      vert_state.mapp_i |= i_pipe_state.mapps[vert_data.mapp_i];
      for (uint j = 0; j < vert_state.mapp_j.size(); ++j)
        vert_state.mapp_j[j] = vert_state.mapp_j[j] | i_pipe_state.mapps[vert_data.mapp_j[j]];
      
      // Set summary flags per vertex
      vert_state.any_colr_j = std::reduce(range_iter(vert_state.colr_j), false, reduce_stale);
      vert_state.any_mapp_j = std::reduce(range_iter(vert_state.mapp_j), false, reduce_stale);
      vert_state.any = vert_state.colr_i | vert_state.mapp_i | vert_state.any_colr_j | vert_state.any_mapp_j;
    }

    // Set summary flags over all vertices/elements in project state
    i_pipe_state.any_verts = std::reduce(range_iter(i_pipe_state.verts), false, 
      [](const auto &a, const auto &b) { return a | b.any; });
    i_pipe_state.any = i_pipe_state.any_mapps | 
                       i_pipe_state.any_elems | 
                       i_pipe_state.any_verts |
                       i_pipe_state.any_cmfs  |
                       i_pipe_state.any_illuminants;

    // Iterate over all selection data
    i_view_state.vert_selection = std::get<1>(detail::compare_state(e_vert_selct, m_vert_selct));
    i_view_state.vert_mouseover = std::get<1>(detail::compare_state(e_vert_mover, m_vert_mover));
    i_view_state.elem_selection = std::get<1>(detail::compare_state(e_elem_selct, m_elem_selct));
    i_view_state.elem_mouseover = std::get<1>(detail::compare_state(e_elem_mover, m_elem_mover));
    i_view_state.cstr_selection = detail::compare_func(e_cstr_selct, m_cstr_selct);
  }
} // namespace met