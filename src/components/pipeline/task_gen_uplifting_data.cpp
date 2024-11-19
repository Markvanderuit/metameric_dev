#include <metameric/core/distribution.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/pipeline/task_gen_uplifting_data.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/misc/task_lambda.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/utility.hpp>
#include <omp.h>
#include <execution>
#include <numbers>

namespace met {
  // Nr. of points generated on the color system boundary through spheriical smpling
  constexpr static uint n_system_boundary_samples = 128;
  
  bool GenUpliftingDataTask::is_active(SchedulerHandle &info) {
    met_trace();
    
    // Get external resources
    const auto &e_scene     = info.global("scene").getr<Scene>();
    const auto &e_uplifting = e_scene.components.upliftings[m_uplifting_i];

    // Force on first run, then make dependent on uplifting only, or
    // if some uplifting is still in progress
    return is_first_eval() 
        || e_uplifting
        || rng::any_of(m_mmv_builders, [](const auto &builder) { return !builder.is_converged(); });
  }

  void GenUpliftingDataTask::init(SchedulerHandle &info) {
    met_trace_full();
      
    constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
    constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

    // Initialize buffers to hold packed delaunay tesselation data; these buffers are used by
    // the gen_object_data task to generate barycentric weights for the objects' textures
    info("tesselation_data").init<gl::Buffer>({ .size = sizeof(MeshDataLayout),                               .flags = buffer_create_flags });
    info("tesselation_pack").init<gl::Buffer>({ .size = sizeof(MeshPackLayout) * detail::met_max_constraints, .flags = buffer_create_flags });
    info("tesselation_coef").init<gl::Buffer>({ .size = sizeof(SpecCoefLayout) * detail::met_max_constraints, .flags = buffer_create_flags });
    m_tesselation_data_map = info("tesselation_data").getw<gl::Buffer>().map_as<MeshDataLayout>(buffer_access_flags).data();
    m_tesselation_pack_map = info("tesselation_pack").getw<gl::Buffer>().map_as<MeshPackLayout>(buffer_access_flags);
    m_tesselation_coef_map = info("tesselation_coef").getw<gl::Buffer>().map_as<SpecCoefLayout>(buffer_access_flags);

    // Specify spectrum cache, for plotting of generated constraint spectra
    info("constraint_samples").set<std::vector<MismatchSample>>({});

    // Specify draw dispatch, as handle for a potential viewer to render the tesselation
    info("tesselation_draw").set<gl::DrawInfo>({});
    info("mismatch_hulls").set<std::vector<ConvexHull>>({});
  }

  void GenUpliftingDataTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get const external uplifting; note that mutable caches are accessible
    const auto &e_scene = info.global("scene").getr<Scene>();
    const auto &[e_uplifting, e_state] 
      = info.global("scene").getw<Scene>().components.upliftings[m_uplifting_i];

    // Get shared resources
    const auto &e_basis   = e_scene.resources.bases[e_uplifting.basis_i];
    const auto &e_cmfs    = e_scene.resources.observers[e_uplifting.observer_i];
    const auto &e_illm    = e_scene.resources.illuminants[e_uplifting.illuminant_i];
    
    // Flag tells if the color system spectra have, in any way, been modified
    bool csys_stale = is_first_eval() 
      || e_state.basis_i      || e_basis 
      || e_state.observer_i   || e_cmfs
      || e_state.illuminant_i || e_illm;

    // Flag tells if the resulting tesselation has, in any way, been modified;
    // this is set to true in step 2 if necessary
    bool tssl_stale = is_first_eval() || e_state.verts.is_resized() || csys_stale;

    // Color system spectra within which the 'uplifted' texture is defined
    auto csys = ColrSystem { .cmfs = *e_cmfs, .illuminant = *e_illm };

    // 1. Generate boundary spectra, coefficients, and colors.
    //    We use a spherical sampling method; see Uplifting::sample_color_solid
    if (csys_stale) {
      m_boundary_samples = e_uplifting.sample_color_solid(e_scene, 4, n_system_boundary_samples);
      fmt::print("Uplifting: sampled {} boundary points\n", m_boundary_samples.size());
    }

    // 2. Generate interior spectra, coefficients, and colors.
    //    We rely on MetamerConstraintBuilder as a shortcut; instead of solving
    //    for the spectra directly, we interpolate the mismatch volume interior
    {
      if (e_state.verts.is_resized())
        m_mmv_builders.resize(e_uplifting.verts.size());

      // Generate constraint spectra
      m_interior_samples.resize(e_uplifting.verts.size());
      for (int i = 0; i < e_uplifting.verts.size(); ++i) {
        auto &builder = m_mmv_builders[i];
        
        // If a state change occurred, restart spectrum builder
        if (csys_stale || !builder.matches_vertex(e_uplifting.verts[i]))
          builder.assign_vertex(e_uplifting.verts[i]);
        
        // If the vertex was not edited, or the metamer builder has converged, we can exit early
        guard_continue(e_state.verts[i] || !builder.is_converged());

        // Generate vertex color, attached metamer, and its originating coefficients
        auto sample = builder.realize(e_uplifting.verts[i], e_scene, e_uplifting);
        
        // Add to set of spectra and coefficients; we set tssl_stale to true only if
        // the color output is changed;
        {
          auto sample_old = m_interior_samples[i];
          m_interior_samples[i] = sample;
          if (!sample_old.colr.isApprox(sample.colr))
            tssl_stale = true; // skipping atomic, as everyone's just setting to true r.n.
        }
      }
    }

    // 3. Merge boundary and interior sample data
    {
      m_tessellation_samples.resize(m_boundary_samples.size() + m_interior_samples.size());
      rng::copy(m_boundary_samples, m_tessellation_samples.begin());
      rng::copy(m_interior_samples, m_tessellation_samples.begin() + m_boundary_samples.size());
    }

    // 4. Generate color system tesselation and pack for the gl-side
    if (tssl_stale) {
      // Generate new tesselation
      auto points = m_tessellation_samples | vws::transform(&MismatchSample::colr) | view_to<std::vector<Colr>>();
      m_tesselation = generate_delaunay<AlDelaunay, Colr>(points);

      // Update packed data for fast per-object delaunay traversal
      std::transform(std::execution::par_unseq, 
                     range_iter(m_tesselation.elems), 
                     m_tesselation_pack_map.begin(), 
      [&](const auto &el) {
        const auto vts = el | index_into_view(m_tesselation.verts);
        MeshPackLayout pack;
        pack.inv.block<3, 3>(0, 0) = (eig::Matrix3f() 
          << vts[0] - vts[3], vts[1] - vts[3], vts[2] - vts[3]
        ).finished().inverse();
        pack.sub.head<3>() = vts[3];
        return pack;
      });

      // Update packing layout data
      m_tesselation_data_map->elem_offs = detail::met_max_constraints * m_uplifting_i;
      m_tesselation_data_map->elem_size = m_tesselation.elems.size();

      // Get writeable buffers and flush changes
      auto &i_tesselation_pack = info("tesselation_pack").getw<gl::Buffer>();
      auto &i_tesselation_data = info("tesselation_data").getw<gl::Buffer>();
      i_tesselation_pack.flush(m_tesselation.elems.size() * sizeof(MeshPackLayout));
      i_tesselation_data.flush();
    } // if (tssl_stale)

    // 5. Pack spectral coefficients for the gl-side
    //    Spectra are always changed, so update them either way
    {
      // Assemble coefficients into mapped buffer. We pack all 4x12 coefficients of a tetrahedron
      // into a single interleaved object, for fast 4-component texture sampling during baking
      for (uint i = 0; i < m_tesselation.elems.size(); ++i) {
        const auto &el = m_tesselation.elems[i];
        SpecCoefLayout coeffs;
        for (uint i = 0; i < 4; ++i)
          coeffs.col(i) = m_tessellation_samples[el[i]].coef;
        m_tesselation_coef_map[i] = coeffs;
      }

      // Flush changes to GL-side 
      info("tesselation_coef").getw<gl::Buffer>().flush();
    }

    // 6. Expose some data for visualisation
    {
      // Generated spectral constraint data is useful to have
      info("constraint_samples").getw<std::vector<MismatchSample>>() = m_interior_samples;

      // Mismatch volume hull data is useful to have
      if (!m_mmv_builders.empty() && rng::any_of(m_mmv_builders, [](const auto &m) {
        return m.did_sample();
      })) {
        auto &i_mismatch_hull = info("mismatch_hulls").getw<std::vector<ConvexHull>>();
        i_mismatch_hull.resize(m_mmv_builders.size());
        rng::transform(m_mmv_builders, i_mismatch_hull.begin(), &MetamerConstraintBuilder::chull);
      }
    }
  }

  Spec GenUpliftingDataTask::query_constraint(uint i) const {
    met_trace();
    return m_interior_samples[i].spec;
  }

  TetrahedronRecord GenUpliftingDataTask::query_tetrahedron(uint i) const {
    met_trace();

    TetrahedronRecord tr = { .weights = 0.f }; // no weights known

    // Find element indices for this tetrahedron, and then fill per-vertex data
    for (auto [i, elem_i] : enumerate_view(m_tesselation.elems[i])) {
      int j = static_cast<int>(elem_i) - static_cast<int>(m_boundary_samples.size());
      tr.indices[i] = std::max<int>(j, -1);                // Assign constraint index, or -1 if a constraint is a boundary vertex
      tr.spectra[i] = m_tessellation_samples[elem_i].spec; // Assign corresponding spectrum
    }

    return tr;
  }

  TetrahedronRecord GenUpliftingDataTask::query_tetrahedron(const Colr &c) const {
    met_trace();

    float result_err = std::numeric_limits<float>::max();
    uint  result_i = 0;
    auto  result_bary = eig::Vector4f(0.f);

    // Find tetrahedron with positive barycentric weights
    for (uint i = 0; i < m_tesselation.elems.size(); ++i) {
      auto inv = m_tesselation_pack_map[i].inv.block<3, 3>(0, 0).eval();
      auto sub = m_tesselation_pack_map[i].sub.head<3>().eval();

      // Compute barycentric weights using packed element data
      eig::Vector3f xyz  = (inv * (c - sub.array()).matrix()).eval();
      eig::Vector4f bary = (eig::Array4f() << xyz, 1.f - xyz.sum()).finished();

      // Compute squared error of potentially unbounded barycentric weights
      float err = (bary - bary.cwiseMax(0.f).cwiseMin(1.f)).matrix().squaredNorm();

      // Continue if error does not improve
      // or store best result
      if (err > result_err)
        continue;
      result_err  = err;
      result_bary = bary;
      result_i    = i;
    }

    // Assign weights to return value
    debug::check_expr(result_i < m_tesselation.elems.size());
    TetrahedronRecord tr = { .weights = result_bary };

    // Find element indices for this tetrahedron, and then fill per-vertex data
    for (auto [i, elem_i] : enumerate_view(m_tesselation.elems[result_i])) {
      int j = static_cast<int>(elem_i) - static_cast<int>(m_boundary_samples.size());
      tr.indices[i] = std::max<int>(j, -1);                // Assign constraint index, or -1 if a constraint is a boundary vertex
      tr.spectra[i] = m_tessellation_samples[elem_i].spec; // Assign corresponding spectrum
    }

    return tr;
  }
} // namespace met