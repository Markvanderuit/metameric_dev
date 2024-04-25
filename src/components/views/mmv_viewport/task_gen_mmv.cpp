#include <metameric/core/distribution.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/matching.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/mmv_viewport/task_gen_mmv.hpp>
#include <small_gl/array.hpp>
#include <small_gl/dispatch.hpp>
#include <algorithm>
#include <execution>

namespace met {
  // Constants
  constexpr uint mmv_samples_per_iter = 16;
  constexpr uint mmv_samples_max      = 256;
  constexpr auto buffer_create_flags  = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags  = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;

  bool GenMMVTask::is_active(SchedulerHandle &info) {
    met_trace();

    // // Get shared resources
    // const auto &e_gizmo_active = info.relative("viewport_guizmo")("is_active").getr<bool>();
    // return info.parent()("is_active").getr<bool>() && !e_gizmo_active;
    return true;
  }

  void GenMMVTask::init(SchedulerHandle &info) {
    met_trace();

    // Prepare output point set for the maximum nr. of samples
    m_colr_set.reserve(mmv_samples_max);
    m_colr_set.clear();

    // Reset iteration and UI values
    m_iter            = 0;
    m_csys_j          = 0;
    m_curr_deque_size = 0;

    // Make vertex array object available, uninitialized
    info("chull_array").set<gl::Array>({ });
    info("chull_draw").set<gl::DrawInfo>({ });
    info("chull_center").set<eig::Array3f>(0.f);
  }

  void GenMMVTask::eval(SchedulerHandle &info) {
    met_trace();

    // Get shared resources
    const auto &e_scene = info.global("scene").getr<Scene>();
    const auto &e_cs    = info.parent()("selection").getr<ConstraintRecord>();
    auto uplf_handle    = info.task(std::format("gen_upliftings.gen_uplifting_{}", e_cs.uplifting_i)).mask(info);

    // Exit early unless inputs have changed somehow
    guard(is_first_eval() || uplf_handle("mismatch_hulls").is_mutated());

    // Get list of convex hulls for this uplifting; exit early if it doesn't exist
    auto &i_array       = info("chull_array").getw<gl::Array>();
    auto &i_draw        = info("chull_draw").getw<gl::DrawInfo>();
    const auto &e_hulls = uplf_handle("mismatch_hulls").getr<std::vector<ConvexHull>>();
    const auto &e_hull  = e_hulls[e_cs.vertex_i];
    
    // Get specific convex hull; exit early and reset if it is empty
    if (e_hull.hull.empty()) {
      i_array = {};
      i_draw  = {};
      return;
    }

    // Determine extents of current point sets
    auto maxb = rng::fold_left_first(e_hull.hull.verts, [](auto a, auto b) { return a.max(b).eval(); }).value();
    auto minb = rng::fold_left_first(e_hull.hull.verts, [](auto a, auto b) { return a.min(b).eval(); }).value();

    // If a set of points is available, generate an approximate center for the camera
    auto new_center = (minb + 0.5 * (maxb - minb)).eval();
    info.relative("viewport_camera")("arcball").getw<detail::Arcball>().set_center(new_center);
    
    // If a convex hull is available, generate a vertex array object and corresponding draw obj
    // for rendering purposes
    std::vector<eig::AlArray3f> hull_verts_aligned(range_iter(e_hull.hull.verts));
    m_chull_verts = {{ .data = cnt_span<const std::byte>(hull_verts_aligned) }};
    m_chull_elems = {{ .data = cnt_span<const std::byte>(e_hull.hull.elems) }};
    i_array = {{
      .buffers  = {{ .buffer = &m_chull_verts, .index = 0, .stride = sizeof(eig::Array4f)   }},
      .attribs  = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
      .elements = &m_chull_elems
    }};
    i_draw = { .type           = gl::PrimitiveType::eTriangles,
               .vertex_count   = (uint) (m_chull_elems.size() / sizeof(uint)),
               .bindable_array = &i_array };
    fmt::print("Pushed mismatch data to gpu\n");
  }
} // namespace met