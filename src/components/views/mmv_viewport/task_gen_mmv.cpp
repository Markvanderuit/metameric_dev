#include <metameric/core/distribution.hpp>
#include <metameric/core/mesh.hpp>
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

    // Get shared resources
    const auto &e_scene             = info.global("scene").getr<Scene>();
    const auto &e_cs                = info.parent()("selection").getr<ConstraintSelection>();
    const auto &[e_object, e_state] = e_scene.components.upliftings[e_cs.uplifting_i];

    // Stale on first run, or if specific uplifting data has changed
    bool is_stale = is_first_eval() 
      || e_state.basis_i 
      || e_state.csys_i 
      || e_state.verts[e_cs.constraint_i]
      || e_scene.components.colr_systems[e_object.csys_i];

    // Reset samples if stale
    if (is_stale)  {
      m_iter = 0;
      m_colr_set.clear();
    }
    
    // Only pass if metameric mismatching is possible and samples are required
    bool is_mmv = e_object.verts[e_cs.constraint_i].has_mismatching() && m_colr_set.size() < mmv_samples_max;
    
    return info.parent()("is_active").getr<bool>() && (is_stale || is_mmv);
  }

  void GenMMVTask::init(SchedulerHandle &info) {
    met_trace();

    // Prepare output point set for the maximum nr. of samples
    m_colr_set.reserve(mmv_samples_max);
    m_colr_set.clear();

    // Reset iteration and UI values
    m_iter = 0;
    m_csys_j = 0;
    m_curr_deque_size = 0;

    // Make vertex array object available, uninitialized
    info("converged").set(false);
    info("chull").set<AlMesh>({ });
    info("chull_array").set<gl::Array>({ });
    info("chull_draw").set<gl::DrawInfo>({ });
    info("points_array").set<gl::Array>({ });
    info("points_draw").set<gl::DrawInfo>({ });
    info("chull_center").set<eig::Array3f>(0.f);
  }

  void GenMMVTask::eval(SchedulerHandle &info) {
    met_trace();

    // Get shared resources
    const auto &e_scene       = info.global("scene").getr<Scene>();
    const auto &e_cs          = info.parent()("selection").getr<ConstraintSelection>();
    const auto &[e_uplifting, 
                 e_state]     = e_scene.components.upliftings[e_cs.uplifting_i];
    const auto &e_vert        = e_uplifting.verts[e_cs.constraint_i];

    // Determine if a reset is in order
    bool should_clear = is_first_eval() 
      || e_state.basis_i 
      || e_state.csys_i 
      || e_state.verts[e_cs.constraint_i]
      || e_scene.components.colr_systems[e_uplifting.csys_i];
    
    // Reset necessary data
    if (should_clear) {
      info("chull_array").getw<gl::Array>() = {};
      m_colr_set.clear();
      m_iter = 0;
      m_curr_deque_size = m_colr_deque.size();;
    }

    // Only continue for valid and mismatch-supporting constraints
    if (std::visit(overloaded {
      [](const DirectColorConstraint &cstr)     { return !cstr.has_mismatching(); },
      [](const DirectSurfaceConstraint &cstr)   { return !cstr.is_valid() || !cstr.has_mismatching(); },
      [](const IndirectSurfaceConstraint &cstr) { return !cstr.is_valid() || !cstr.has_mismatching(); },
      [](const auto &) { return false; }
    }, e_vert.constraint)) {
      info("converged").set(true);
      info("chull_array").getw<gl::Array>() = {};
      m_colr_set.clear();
      m_iter = 0;
      return;
    }
    
    // Only continue if more samples are necessary
    if (m_colr_set.size() >= mmv_samples_max) {
      info("converged").set(true);
      return;
    }

    // Visit underlying constraint types one by one
    auto new_points = std::visit(overloaded {
      [&](const DirectColorConstraint &cstr) {
        // Prepare input color systems and corresponding signals
        auto systems_i = { e_scene.csys(e_uplifting.csys_i).finalize() };
        auto signals_i = { cstr.colr_i };
        auto systems_j = cstr.csys_j
                       | vws::transform([&](uint i) { return e_scene.csys(i).finalize(); })
                       | rng::to<std::vector>();

        // Prepare data for MMV point generation
        GenerateMismatchingOCSInfo mmv_info = {
          .basis     = e_scene.resources.bases[e_uplifting.basis_i].value(),
          .systems_i = systems_i,
          .signals_i = signals_i,
          .systems_j = systems_j,
          .seed      = m_iter,
          .n_samples = mmv_samples_per_iter
        };

        // Generate MMV spectra and append corresponding colors to current point set
        auto csys = e_scene.csys(cstr.csys_j[m_csys_j]);
        return csys(generate_mismatching_ocs(mmv_info));
      },
      [&](const DirectSurfaceConstraint &cstr) {
        // Prepare input color systems and corresponding signals;
        // note, we keep search space in XYZ
        auto csys_base_v = { e_scene.csys(e_uplifting.csys_i).finalize(false) };
        auto sign_base_v = { lrgb_to_xyz(cstr.surface.diffuse) };
        auto csys_refl_v = vws::transform(cstr.csys_j, [&](uint i) { return e_scene.csys(i).finalize(false); })
                         | rng::to<std::vector>();

        // Assemble info object for generating MMV spectra
        GenerateMismatchingOCSInfo mmv_info = {
          .basis     = e_scene.resources.bases[e_uplifting.basis_i].value(),
          .systems_i = csys_base_v,
          .signals_i = sign_base_v,
          .systems_j = csys_refl_v,
          .seed      = m_iter,
          .n_samples = mmv_samples_per_iter
        };

        // Generate MMV spectra, convert to view color system, and append to current point set
        auto csys_view = e_scene.csys(cstr.csys_j[m_csys_j]);
        return csys_view(generate_mismatching_ocs(mmv_info));
      },
      [&](const IndirectSurfaceConstraint &cstr) {
        // Construct objective color system spectra from power series
        IndirectColrSystem csys = {
          .cmfs   = e_scene.resources.observers[e_scene.components.observer_i.value].value(),
          .powers = cstr.powers
        };

        // Prepare input color systems and corresponding signals;
        // note, we keep search space in XYZ
        auto csys_base_v = { e_scene.csys(e_uplifting.csys_i).finalize(false) };
        auto sign_base_v = { lrgb_to_xyz(cstr.surface.diffuse) };
        auto csys_refl_v = csys.finalize(false);

        // Assemble info object for generating MMV spectra
        GenerateIndirectMismatchingOCSInfo mmv_info = {
          .basis     = e_scene.resources.bases[e_uplifting.basis_i].value(),
          .systems_i = csys_base_v,
          .signals_i = sign_base_v,
          .systems_j = csys_refl_v,
          .seed      = m_iter,
          .n_samples = mmv_samples_per_iter
        };

        // Generate MMV spectra, convert to view color system, and append to current point set
        return csys(generate_mismatching_ocs(mmv_info));
      },
      [&](const auto &) { return std::vector<Colr>(); }
    }, e_vert.constraint);

    // Insert newly gathered points, and roll them into end of deque while removing front
    m_colr_set.insert_range(std::vector(new_points));
    if (m_curr_deque_size > 0) {
      auto reduce_size = std::min({ static_cast<uint>(new_points.size()),
                                    static_cast<uint>(m_colr_deque.size()), 
                                    m_curr_deque_size });
      m_curr_deque_size -= reduce_size;
      m_colr_deque.erase(m_colr_deque.begin(), m_colr_deque.begin() + reduce_size);
    }
    m_colr_deque.append_range(new_points); // invalidates new_points

    // Increment iteration up to sample count
    m_iter += mmv_samples_per_iter;

    // Only continue if points are found
    guard(!m_colr_set.empty());

    // Determine extents of current point sets
    auto maxb = rng::fold_left_first(m_colr_deque, [](auto a, auto b) { return a.max(b).eval(); }).value();
    auto minb = rng::fold_left_first(m_colr_deque, [](auto a, auto b) { return a.min(b).eval(); }).value();

    // Generate convex hulls, if the minimum nr. of points is available and
    // the pointset does not collapse to a small position;
    // QHull is rather picky and will happily tear down the application :(
    auto &i_chull = info("chull").getw<AlMesh>();
    if (m_colr_set.size() >= 4 && (maxb - minb).minCoeff() >= 0.005f) {
      auto points = std::vector<Colr>(range_iter(m_colr_deque));
      i_chull = generate_convex_hull<AlMesh, Colr>(points);
    }

    // If a set of points is available, generate an approximate center for the camera, and update this
    // if the distance shift exceeds some threshold
    auto new_center = (minb + 0.5 * (maxb - minb)).eval();
    if (auto &curr_center = info("chull_center").getw<eig::Array3f>(); (curr_center - new_center).matrix().norm() > 0.05f) {
      curr_center = new_center;
      fmt::print("new center: {}\n", new_center);
      info.relative("viewport_camera")("arcball").getw<detail::Arcball>().set_center(curr_center);
    }
    
    // If a convex hull is available, generate a vertex array object
    // for rendering purposes
    auto &i_array        = info("chull_array").getw<gl::Array>();
    auto &i_draw         = info("chull_draw").getw<gl::DrawInfo>();
    auto &i_points_array = info("points_array").getw<gl::Array>();
    auto &i_points_draw  = info("points_draw").getw<gl::DrawInfo>();
    if (i_chull.elems.size() > 0) {
      std::vector<eig::AlArray3f> points(range_iter(m_colr_deque));

      m_colr_verts  = {{ .data = cnt_span<const std::byte>(points) }};
      m_chull_verts = {{ .data = cnt_span<const std::byte>(i_chull.verts) }};
      m_chull_elems = {{ .data = cnt_span<const std::byte>(i_chull.elems) }};

      i_array = {{
        .buffers  = {{ .buffer = &m_chull_verts, .index = 0, .stride = sizeof(eig::Array4f)   }},
        .attribs  = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
        .elements = &m_chull_elems
      }};
      i_draw = { .type           = gl::PrimitiveType::eTriangles,
                 .vertex_count   = (uint) (m_chull_elems.size() / sizeof(uint)),
                 .capabilities   = {{ gl::DrawCapability::eCullOp, false   },
                                    { gl::DrawCapability::eDepthTest, true }},
                 .draw_op        = gl::DrawOp::eLine,
                 .bindable_array = &i_array };
                 
      i_points_array = {{
        .buffers  = {{ .buffer = &m_colr_verts, .index = 0, .stride = sizeof(eig::Array4f)   }},
        .attribs  = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
      }};
      i_points_draw = { .type           = gl::PrimitiveType::ePoints,
                        .vertex_count   = (uint) (m_chull_elems.size() / sizeof(uint)),
                        .capabilities   = {{ gl::DrawCapability::eCullOp, false    },
                                           { gl::DrawCapability::eDepthTest, false }},
                        .bindable_array = &i_points_array };
    } else {
      // Deinitialize
      i_array = {};
    }
    
    // Flag convergence for following tasks
    if (m_colr_set.size() >= mmv_samples_max) {
      info("converged").set(true);
    }
  }
} // namespace met