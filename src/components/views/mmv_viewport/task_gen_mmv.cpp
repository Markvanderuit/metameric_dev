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
  constexpr uint mmv_samples_per_iter = 32;
  constexpr uint mmv_samples_max      = 4096; // 256u;
  constexpr auto buffer_create_flags  = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags  = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;

  namespace detail {
    // Given a random vector in RN bounded to [-1, 1], return a vector
    // distributed over a gaussian distribution
    inline
    eig::ArrayXf inv_gaussian_cdf(const eig::ArrayXf &x) {
      auto y = (-(x * x) + 1.f).max(.0001f).log().eval();
      auto z = (0.5f * y + (2.f / std::numbers::pi_v<float>)).eval();
      return (((z * z - y).sqrt() - z).sqrt() * x.sign()).eval();
    }
    
    // Given a random vector in RN bounded to [-1, 1], return a uniformly
    // distributed point on the unit sphere
    inline
    eig::ArrayXf inv_unit_sphere_cdf(const eig::ArrayXf &x) {
      met_trace();
      return inv_gaussian_cdf(x).matrix().normalized().array().eval();
    }

    // Generate a set of random, uniformly distributed unit vectors in RN
    inline
    std::vector<eig::ArrayXf> gen_unit_dirs(uint n_samples, uint n_dims, uint seed_offs = 0) {
      met_trace();

      auto unit_dirs = std::vector<eig::ArrayXf>(n_samples);

      if (n_samples <= 128) {
        UniformSampler sampler(-1.f, 1.f, seed_offs);
        for (int i = 0; i < unit_dirs.size(); ++i)
          unit_dirs[i] = inv_unit_sphere_cdf(sampler.next_nd(n_dims));
      } else {
        UniformSampler sampler(-1.f, 1.f, seed_offs);
        #pragma omp parallel
        { // Draw samples for this thread's range with separate sampler per thread
          UniformSampler sampler(-1.f, 1.f, seed_offs + static_cast<uint>(omp_get_thread_num()));
          #pragma omp for
          for (int i = 0; i < unit_dirs.size(); ++i)
            unit_dirs[i] = inv_unit_sphere_cdf(sampler.next_nd(n_dims));
        }
      }

      return unit_dirs;
    }
  } // namespace detail

  bool GenMMVTask::is_active(SchedulerHandle &info) {
    met_trace();

    // Get shared resources
    const auto &e_scene             = info.global("scene").getr<Scene>();
    const auto &e_is                = info.parent()("selection").getr<InputSelection>();
    const auto &[e_object, e_state] = e_scene.components.upliftings[e_is.uplifting_i];

    // Stale on first run, or if specific uplifting data has changed
    bool is_stale = is_first_eval() 
      || e_state.basis_i 
      || e_state.csys_i 
      || e_state.verts[e_is.constraint_i]
      || e_scene.components.colr_systems[e_object.csys_i];

    // Reset samples if stale
    if (is_stale)  {
      m_iter = 0;
      m_points.clear();
    }
    
    // Only pass if metameric mismatching is possible and samples are required
    bool is_mmv = e_object.verts[e_is.constraint_i].has_mismatching() && m_iter < mmv_samples_max;
    
    return info.parent()("is_active").getr<bool>() && (is_stale || is_mmv);
  }

  void GenMMVTask::init(SchedulerHandle &info) {
    met_trace();

    // Prepare output point set for the maximum nr. of samples
    m_points.reserve(mmv_samples_max);
    m_points.clear();

    // Reset iteration and UI values
    m_iter   = 0;
    m_csys_j = 0;

    // Make vertex array object available, uninitialized
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
    const auto &e_is          = info.parent()("selection").getr<InputSelection>();
    const auto &[e_uplifting, 
                 e_state]     = e_scene.components.upliftings[e_is.uplifting_i];
    const auto &e_vert        = e_uplifting.verts[e_is.constraint_i];

    // Determine if a reset is in order
    bool should_clear = is_first_eval() 
      || e_state.basis_i 
      || e_state.csys_i 
      || e_state.verts[e_is.constraint_i]
      || e_scene.components.colr_systems[e_uplifting.csys_i];;
    if (should_clear) {
      info("chull_array").getw<gl::Array>() = {};
      m_points.clear();
      m_iter = 0;
    }

    // Only continue for valid and mismatch-supporting constraints
    if (std::visit(overloaded {
      [](const DirectColorConstraint &cstr)     { return !cstr.has_mismatching(); },
      [](const DirectSurfaceConstraint &cstr)   { return !cstr.is_valid() || !cstr.has_mismatching(); },
      [](const IndirectSurfaceConstraint &cstr) { return !cstr.is_valid() || !cstr.has_mismatching(); },
      [](const auto &) { return false; }
    }, e_vert.constraint)) {
      info("chull_array").getw<gl::Array>() = {};
      m_points.clear();
      m_iter = 0;
      return;
    }
    
    // Only continue if more samples are necessary
    if (m_iter >= mmv_samples_max)
      return;

    // Visit underlying constraint types one by one
    std::visit(overloaded {
      [&](const DirectColorConstraint &cstr) {
        // Generate 6D unit vector samples
        auto samples = detail::gen_unit_dirs(mmv_samples_per_iter, 6, m_iter);
        
        // Prepare input color systems and corresponding signals
        auto systems_i = { e_scene.get_csys(e_uplifting.csys_i).finalize() };
        auto signals_i = { cstr.colr_i };
        auto systems_j = cstr.csys_j
                       | vws::transform([&](uint i) { return e_scene.get_csys(i).finalize(); })
                       | rng::to<std::vector>();

        // Prepare data for MMV point generation
        GenerateMMVBoundaryInfo mmv_info = {
          .basis     = e_scene.resources.bases[e_uplifting.basis_i].value(),
          .systems_i = systems_i,
          .signals_i = signals_i,
          .systems_j = systems_j,
          .system_j  = systems_j[m_csys_j], // Current visualized color system
          .samples   = samples,
        };

        // Generate MMV points and append to current point set
        m_points.insert_range(generate_mmv_boundary_colr(mmv_info));
      },
      [&](const DirectSurfaceConstraint &cstr) {
        // Generate 6D unit vector samples
        auto samples = detail::gen_unit_dirs(mmv_samples_per_iter, 3, m_iter);
        
        // Prepare input color systems and corresponding signals
        auto systems_i = { e_scene.get_csys(e_uplifting.csys_i).finalize() };
        auto signals_i = { cstr.surface.diffuse };
        auto systems_j = vws::transform(cstr.csys_j, [&](uint i) { return e_scene.get_csys(i).finalize(); })
                       | rng::to<std::vector>();

        // Prepare data for MMV point generation
        GenerateMMVBoundaryInfo mmv_info = {
          .basis     = e_scene.resources.bases[e_uplifting.basis_i].value(),
          .systems_i = systems_i,
          .signals_i = signals_i,
          .systems_j = systems_j,
          .system_j  = systems_j[m_csys_j], // Current visualized color system
          .samples   = samples,
        };

        // Generate MMV points and append to current point set
        m_points.insert_range(generate_mmv_boundary_colr(mmv_info));
      },
      [&](const IndirectSurfaceConstraint &cstr) {
        // Generate 3D unit vector samples
        auto samples = detail::gen_unit_dirs(mmv_samples_per_iter, 3, m_iter);

        // Get camera cmfs
        CMFS cmfs = e_scene.resources.observers[e_scene.components.observer_i.value].value();
        cmfs = (cmfs.array())
             / (cmfs.array().col(1) * wavelength_ssize).sum();
        cmfs = (models::xyz_to_srgb_transform * cmfs.matrix().transpose()).transpose();

        // Get constraint components
        auto system_j  =  e_scene.get_csys(e_uplifting.csys_i).finalize();
        auto systems_i = { e_scene.get_csys(e_uplifting.csys_i).finalize() };
        auto signals_i = { cstr.surface.diffuse };

        ColrSystem csys_base = e_scene.get_csys(e_uplifting.csys_i);
        IndirectColrSystem csys_refl = {
          .cmfs   = e_scene.resources.observers[e_scene.components.observer_i.value].value(),
          .powers = cstr.powers
        };

        // Construct objective color system spectra from power series
        auto systems_j = cstr.powers
                       | vws::transform([&](Spec pwr) {
                          CMFS to_xyz = (cmfs.array().colwise() * pwr * wavelength_ssize);
                          CMFS to_rgb = (models::xyz_to_srgb_transform * to_xyz.matrix().transpose()).transpose();
                          return to_rgb; })
                       | rng::to<std::vector>();

        // Prepare data for MMV point generation
        GenerateIndirectMMVBoundaryInfo mmv_info = {
          .basis      = e_scene.resources.bases[e_uplifting.basis_i].value(),
          .systems_i  = systems_i,
          .signals_i  = signals_i,
          .components = cstr.powers,
          .systems_j  = systems_j,
          .system_j   = cmfs,
          .samples    = samples,
        };

        // Generate MMV points and append to current point set
        m_points.insert_range(generate_mmv_boundary_colr(mmv_info));
      },
      [&](const auto &) { }
    }, e_vert.constraint);

    // Increment iteration up to sample count
    m_iter += mmv_samples_per_iter;

    // Only continue if points are found
    guard(!m_points.empty());

    // Determine extents of generated point sets
    auto maxb = rng::fold_left_first(m_points, [](auto a, auto b) { return a.max(b).eval(); }).value();
    auto minb = rng::fold_left_first(m_points, [](auto a, auto b) { return a.min(b).eval(); }).value();

    // Generate convex hulls, if the minimum nr. of points is available and
    // the pointset does not collapse to a small position;
    // QHull is rather picky and will happily tear down the application :(
    if (m_points.size() >= 4 && (maxb - minb).minCoeff() >= 0.005f) {
      auto points = std::vector<Colr>(range_iter(m_points));
      m_chull = generate_convex_hull<AlMesh, Colr>(points);
    }

    // If a set of points is available, generate an approximate center for the camera, and update this
    // if the distance shift exceeds some threshold
    auto new_center = (minb + 0.5 * (maxb - minb)).eval();
    if (auto &curr_center = info("chull_center").getw<eig::Array3f>(); (curr_center - new_center).matrix().norm() > 0.1f) {
      curr_center = new_center;
      info.relative("viewport_camera")("arcball").getw<detail::Arcball>().set_center(curr_center);
    }
    
    // If a convex hull is available, generate a vertex array object
    // for rendering purposes
    auto &i_array        = info("chull_array").getw<gl::Array>();
    auto &i_draw         = info("chull_draw").getw<gl::DrawInfo>();
    auto &i_points_array = info("points_array").getw<gl::Array>();
    auto &i_points_draw  = info("points_draw").getw<gl::DrawInfo>();
    if (m_chull.elems.size() > 0) {
      std::vector<AlColr> points(range_iter(m_points));
      m_points_verts = {{ .data = cnt_span<const std::byte>(points) }};
      m_chull_verts  = {{ .data = cnt_span<const std::byte>(m_chull.verts) }};
      m_chull_elems  = {{ .data = cnt_span<const std::byte>(m_chull.elems) }};

      i_array = {{
        .buffers  = {{ .buffer = &m_chull_verts, .index = 0, .stride = sizeof(eig::Array4f)   }},
        .attribs  = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
        .elements = &m_chull_elems
      }};
      i_draw = { .type             = gl::PrimitiveType::eTriangles,
                 .vertex_count     = (uint) (m_chull_elems.size() / sizeof(uint)),
                 .capabilities     = {{ gl::DrawCapability::eCullOp, false   },
                                      { gl::DrawCapability::eDepthTest, true }},
                 .draw_op          = gl::DrawOp::eLine,
                 .bindable_array   = &i_array };
                 
      i_points_array = {{
        .buffers  = {{ .buffer = &m_points_verts, .index = 0, .stride = sizeof(eig::Array4f)   }},
        .attribs  = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
      }};
      i_points_draw = { .type             = gl::PrimitiveType::ePoints,
                        .vertex_count     = (uint) (m_chull_elems.size() / sizeof(uint)),
                        .capabilities     = {{ gl::DrawCapability::eCullOp, false    },
                                             { gl::DrawCapability::eDepthTest, false }},
                        .bindable_array   = &i_points_array };
    } else {
      // Deinitialize
      i_array = {};
    }
  }
} // namespace met