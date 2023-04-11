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
    template <typename T>
    bool compare_object(const T &in, T &out) {
      guard(out != in, false);
      out = in;
      return true;
    }

    template <typename T, typename T_>
    bool compare_object(const std::pair<T, T_> &in, std::pair<T, T_> &out) {
      return compare_object(in.first, out.first) || compare_object(in.second, out.second);
    }

    template <>
    bool compare_object<ProjectData::CSys>(const ProjectData::CSys &in, ProjectData::CSys &out) {
      guard(out != in, false);
      out = in;
      return true;
    }

    // Overrides for eigen types
    #define decl_compare_object_eig(T)              \
      template <>                                   \
      bool compare_object<T>(const T &in, T &out) { \
        guard (!out.isApprox(in), false);           \
        out = in;                                   \
        return true;                                \
      }
    decl_compare_object_eig(CMFS);
    decl_compare_object_eig(Spec);
    decl_compare_object_eig(eig::Array3u);
    decl_compare_object_eig(eig::Array4u);
    decl_compare_object_eig(eig::Array3f);
    decl_compare_object_eig(eig::Matrix4f);
    decl_compare_object_eig(eig::Vector2f);

    template <typename ReturnObject, typename T>
    ReturnObject compare_vector(const std::vector<T> &in, std::vector<T> &out) {
      met_trace();
      
      ReturnObject object;
      object.is_stale.resize(in.size(), true);

      if (in.size() == out.size()) {
        // Handle simplest (non-resize) case first
        for (uint i = 0; i < in.size(); ++i)
          object[i] = compare_object(in[i], out[i]);
        object.is_any_stale = std::reduce(range_iter(object.is_stale), false, std::logical_or<bool>());
      } else {
        // Handle potential resize
        size_t min_r = std::min(out.size(), in.size()), 
               max_r = std::max(out.size(), in.size());
        out.resize(in.size());
        
        // Only compare for smaller range of remaining elements
        for (uint i = 0; i < min_r; ++i)
          object[i] = compare_object(in[i], out[i]);
          
        // Potential added elements always have state as 'true'
        if (in.size() == max_r)
          for (uint i = min_r; i < max_r; ++i)
            out[i] = in[i];
        
        object.is_any_stale = true;
      }

      return object;
    }

    ProjectState::VertState compare_object(const ProjectData::Vert &in, ProjectData::Vert &out) {
      met_trace();
      
      ProjectState::VertState object;

      object.colr_i = compare_object(in.colr_i, out.colr_i);
      object.csys_i = compare_object(in.csys_i, out.csys_i);
      object.colr_j = compare_vector<VectorState<uint>>(in.colr_j, out.colr_j);
      object.csys_j = compare_vector<VectorState<uint>>(in.csys_j, out.csys_j);

      object.is_any_stale = object.colr_i || object.csys_i || object.colr_j || object.csys_j;

      return object;
    }

    template <>
    VectorState<ProjectState::VertState> compare_vector(const std::vector<ProjectData::Vert> &in, std::vector<ProjectData::Vert> &out) {
      met_trace();

      VectorState<ProjectState::VertState> object;
      object.is_stale.resize(in.size());

      // Handle potential resize
      if (in.size() != out.size())
        out.resize(in.size());

      // compare_object(Vert, Vert) handles internal resizes
      for (uint i = 0; i < in.size(); ++i)
        object[i] = compare_object(in[i], out[i]);
      object.is_any_stale = std::reduce(range_iter(object.is_stale), false, std::logical_or<bool>());

      return object;
    }
  }
  
  void StateTask::init(SchedulerHandle &info) {
    met_trace();
    info("proj_state").set<ProjectState>({ });
    info("view_state").set<ViewportState>({ });
  }

  void StateTask::eval(SchedulerHandle &info) {
    met_trace();

    // Get external resources
    const auto &e_appl_data  = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data  = e_appl_data.project_data;
    const auto &e_arcball    = info("viewport.input", "arcball").read_only<detail::Arcball>();
    const auto &e_vert_selct = info("viewport.input.vert", "selection").read_only<std::vector<uint>>();
    const auto &e_vert_mover = info("viewport.input.vert", "mouseover").read_only<std::vector<uint>>();
    const auto &e_cstr_selct = info("viewport.overlay", "constr_selection").read_only<int>();

    // Copies of resources
    ViewportState view_state;
    ProjectState  proj_state;

    // Iterate over project data
    proj_state.elems       = detail::compare_vector<VectorState<uint>>(e_proj_data.elems, m_elems);
    proj_state.cmfs        = detail::compare_vector<VectorState<uint>>(e_proj_data.cmfs, m_cmfs);
    proj_state.csys        = detail::compare_vector<VectorState<uint>>(e_proj_data.color_systems, m_csys);
    proj_state.illuminants = detail::compare_vector<VectorState<uint>>(e_proj_data.illuminants, m_illuminants);
    proj_state.verts       = detail::compare_vector<VectorState<ProjectState::VertState>>(e_proj_data.verts, m_verts);

    // Post-process; propagate state changes in vertex reference data to vertex state
    for (uint i = 0; i < proj_state.verts.size(); ++i) {
      const auto &vert_data  = e_proj_data.verts[i];
            auto &vert_state = proj_state.verts[i];
      
      // If mapping state has become stale, this influenced the flag inside of a vertex as well
      vert_state.csys_i = vert_state.csys_i || proj_state.csys[vert_data.csys_i];
      for (uint j = 0; j < vert_state.csys_j.size(); ++j) 
        vert_state.csys_j[j] = vert_state.csys_j[j] || proj_state.csys[vert_data.csys_j[j]];

      // Update summary flags per vertex
      vert_state.colr_j.is_any_stale |= std::reduce(range_iter(vert_state.colr_j.is_stale), false, std::logical_or<bool>());
      vert_state.csys_j.is_any_stale |= std::reduce(range_iter(vert_state.csys_j.is_stale), false, std::logical_or<bool>());
      vert_state.is_any_stale |= vert_state.colr_i || vert_state.csys_i || vert_state.colr_j || vert_state.csys_j;
    }

    // Update summary flag across vertices
    proj_state.verts.is_any_stale |= std::reduce(range_iter(proj_state.verts.is_stale), false, std::logical_or<bool>());

    // Iterate over input and selection data
    view_state.vert_selection = detail::compare_vector<VectorState<uint>>(e_vert_selct, m_vert_selct).is_any_stale;
    view_state.vert_mouseover = detail::compare_vector<VectorState<uint>>(e_vert_mover, m_vert_mover).is_any_stale;
    view_state.cstr_selection = detail::compare_object(e_cstr_selct, m_cstr_selct);
    view_state.camera_matrix  = detail::compare_object(e_arcball.full().matrix(), m_camera_matrix);
    view_state.camera_aspect  = detail::compare_object(e_arcball.m_aspect,        m_camera_aspect);

    // Set major summary flags
    proj_state.is_any_stale = proj_state.verts || proj_state.csys || proj_state.elems 
      || proj_state.cmfs || proj_state.illuminants;
    view_state.is_any_stale = view_state.vert_selection || view_state.vert_mouseover 
      || view_state.cstr_selection || view_state.camera_matrix || view_state.camera_aspect;

    // Submit state changes to scheduler objects
    if (auto rsrc = info("view_state"); view_state || rsrc.read_only<ViewportState>())
      rsrc.writeable<ViewportState>() = view_state;
    if (auto rsrc = info("proj_state"); proj_state || rsrc.read_only<ProjectState>())
      rsrc.writeable<ProjectState>() = proj_state;
  }
} // namespace met