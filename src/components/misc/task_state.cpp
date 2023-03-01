#include <metameric/core/state.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/misc/task_state.hpp>
#include <metameric/components/views/detail/arcball.hpp>
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
    bool compare_func<ProjectData::CSys>(const ProjectData::CSys &in, ProjectData::CSys &out) {
      guard(in.cmfs != out.cmfs || in.illuminant != out.illuminant || in.n_scatters != out.n_scatters, false);
      out = in;
      return true;
    }

    #define decl_compare_func_eig(T)              \
      template <>                                 \
      bool compare_func<T>(const T &in, T &out) { \
        guard (!out.isApprox(in), false);         \
        out = in;                                 \
        return true;                              \
      }

    decl_compare_func_eig(CMFS);
    decl_compare_func_eig(Spec);
    decl_compare_func_eig(eig::Array3u);
    decl_compare_func_eig(eig::Array4u);
    decl_compare_func_eig(eig::Array3f);
    decl_compare_func_eig(eig::Matrix4f);
    decl_compare_func_eig(eig::Vector2f);

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

    CacheVert compare_and_set_vert(const ProjectData::Vert &in, ProjectData::Vert &out) {
      CacheVert state;
      state.colr_i = compare_func(in.colr_i, out.colr_i);
      state.csys_i = compare_func(in.csys_i, out.csys_i);
      std::tie(state.colr_j, state.any_colr_j) = compare_state(in.colr_j, out.colr_j);
      std::tie(state.csys_j, state.any_csys_j) = compare_state(in.csys_j, out.csys_j);
      state.any = state.colr_i || state.csys_i || state.any_colr_j || state.any_csys_j;
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

    constexpr auto reduce_or = [](auto a, auto b) { return a | b; };

    // Get shared resources
    auto &i_pipe_state = info.get_resource<ProjectState>("pipeline_state");
    auto &i_view_state = info.get_resource<ViewportState>("viewport_state");
    auto &e_appl_data  = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data  = e_appl_data.project_data;
    auto &e_arcball    = info.get_resource<detail::Arcball>("viewport_input", "arcball");
    auto &e_vert_selct = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection");
    auto &e_vert_mover = info.get_resource<std::vector<uint>>("viewport_input_vert", "mouseover");
    auto &e_cstr_selct = info.get_resource<int>("viewport_overlay", "constr_selection");

    // Iterate over all project data
    bool pre_verts_resize = e_proj_data.vertices.size() != m_verts.size();
    i_pipe_state.verts = detail::compare_and_set_all_vert(e_proj_data.vertices, m_verts);
    std::tie(i_pipe_state.illuminants, i_pipe_state.any_illuminants) = detail::compare_state(e_proj_data.illuminants, m_illuminants);
    std::tie(i_pipe_state.cmfs,        i_pipe_state.any_cmfs)        = detail::compare_state(e_proj_data.cmfs, m_cmfs);
    std::tie(i_pipe_state.csys,        i_pipe_state.any_csys)        = detail::compare_state(e_proj_data.color_systems, m_csys);

    // Post-process fill in some gaps in project state
    for (uint i = 0; i < i_pipe_state.verts.size(); ++i) {
      auto &vert_state = i_pipe_state.verts[i];
      auto &vert_data  = e_proj_data.vertices[i];
      
      // If mapping state has become stale, this influenced the flag inside of a vertex as well
      vert_state.csys_i |= i_pipe_state.csys[vert_data.csys_i];
      for (uint j = 0; j < vert_state.csys_j.size(); ++j)
        vert_state.csys_j[j] = vert_state.csys_j[j] | i_pipe_state.csys[vert_data.csys_j[j]];
      
      // Update summary flags per vertex
      vert_state.any_colr_j |= std::reduce(range_iter(vert_state.colr_j), false, reduce_or);
      vert_state.any_csys_j |= std::reduce(range_iter(vert_state.csys_j), false, reduce_or);
      vert_state.any        |= vert_state.colr_i || vert_state.csys_i || vert_state.any_colr_j || vert_state.any_csys_j;
    }

    // Set summary flags over all vertices in project state
    i_pipe_state.any_verts = pre_verts_resize | std::reduce(range_iter(i_pipe_state.verts), false, 
      [](const auto &a, const auto &b) { return a | b.any; });

    // Set giant summary flag
    i_pipe_state.any = i_pipe_state.any_csys  | 
                       i_pipe_state.any_verts |
                       i_pipe_state.any_cmfs  |
                       i_pipe_state.any_illuminants;

    // Iterate over all selection data
    i_view_state.vert_selection = std::get<1>(detail::compare_state(e_vert_selct, m_vert_selct));
    i_view_state.vert_mouseover = std::get<1>(detail::compare_state(e_vert_mover, m_vert_mover));
    i_view_state.cstr_selection = detail::compare_func(e_cstr_selct, m_cstr_selct);

    // Set summary flags over arcball camera state
    i_view_state.camera_matrix = detail::compare_func(e_arcball.full().matrix(), m_camera_matrix);
    i_view_state.camera_aspect = detail::compare_func(e_arcball.m_aspect,        m_camera_aspect);
  }
} // namespace met