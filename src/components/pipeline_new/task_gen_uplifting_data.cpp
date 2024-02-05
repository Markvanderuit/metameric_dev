#include <metameric/core/distribution.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/pipeline_new/task_gen_uplifting_data.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/utility.hpp>
#include <omp.h>
#include <execution>
#include <numbers>

namespace met {
  constexpr uint n_system_boundary_samples    = 64u;
  constexpr uint max_supported_delaunay_elems = 512u;
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  // Generate a transforming view that performs unsigned integer index access over a range
  constexpr auto indexed_view(const auto &v) {
    return vws::transform([&v](uint i) { return v[i]; });
  };
  
  namespace detail {
    // Given a point in R^N bounded to [-1, 1], return a point
    // mapped to a gaussian distribution, uniformly distributed
    inline
    auto inv_gaussian_cdf(const auto &x) {
      met_trace();
      auto y = (-(x * x) + 1.f).max(.0001f).log().eval();
      auto z = (0.5f * y + (2.f / std::numbers::pi_v<float>)).eval();
      return (((z * z - y).sqrt() - z).sqrt() * x.sign()).eval();
    }
    
    // Given a point in R^N bounded to [-1, 1], return a point
    // mapped to the unit sphere, uniformly distributed
    inline
    auto inv_unit_sphere_cdf(const auto &x) {
      met_trace();
      return inv_gaussian_cdf(x).matrix().normalized().eval();
    }

    // Generate a set of uniformly distributed unit vectors in R^N
    template <uint N>
    inline
    std::vector<eig::Array<float, N, 1>> gen_unit_dirs(uint n_samples) {
      met_trace();
      
      std::vector<eig::Array<float, N, 1>> unit_dirs(n_samples);

      #pragma omp parallel
      {
        // Draw samples for this thread's range with separate sampler per thread
        // UniformSampler sampler(-1.f, 1.f, seeds[omp_get_thread_num()]);
        UniformSampler sampler(-1.f, 1.f, static_cast<uint>(omp_get_thread_num()));

        #pragma omp for
        for (int i = 0; i < unit_dirs.size(); ++i)
          unit_dirs[i] = detail::inv_unit_sphere_cdf(sampler.next_nd<N>());
      }

      return unit_dirs;
    }
  } // namespace detail

  GenUpliftingDataTask:: GenUpliftingDataTask(uint uplifting_i)
  : m_uplifting_i(uplifting_i) { }

  bool GenUpliftingDataTask::is_active(SchedulerHandle &info) {
    met_trace();
    
    // Get external resources
    const auto &e_scene     = info.global("scene").getr<Scene>();
    const auto &e_uplifting = e_scene.components.upliftings[m_uplifting_i];

     // Force on first run, then make dependent on uplifting only
    return is_first_eval() || e_uplifting;
  }

  void GenUpliftingDataTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Precalculate samples for finding the color system boundary
    m_csys_boundary_samples = detail::gen_unit_dirs<3>(n_system_boundary_samples);

    // Initialize buffers to hold packed delaunay tesselation data; these buffers are used by
    // the gen_object_data task to generate barycentric weights for the objects' textures
    info("tesselation_data").init<gl::Buffer>({ .size = sizeof(MeshDataLayout),                                .flags = buffer_create_flags });
    info("tesselation_pack").init<gl::Buffer>({ .size = sizeof(MeshPackLayout) * max_supported_delaunay_elems, .flags = buffer_create_flags });
    m_tesselation_data_map = info("tesselation_data").getw<gl::Buffer>().map_as<MeshDataLayout>(buffer_access_flags).data();
    m_tesselation_pack_map = info("tesselation_pack").getw<gl::Buffer>().map_as<MeshPackLayout>(buffer_access_flags);

    // Initialize buffer to hold packed spectral data; this buffer is copied over to a texture
    // in upliftings.gl for fast access during rendering
    m_buffer_spec_pack     = {{ .size = sizeof(SpecPackLayout) * max_supported_spectra, .flags = buffer_create_flags  }};
    m_buffer_spec_pack_map = m_buffer_spec_pack.map_as<SpecPackLayout>(buffer_access_flags);
  }

  void GenUpliftingDataTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get unmodifiable external uplifting; note that mutable caches are accessible
    const auto &e_scene = info.global("scene").getr<Scene>();
    const auto &[e_uplifting, e_state] 
      = info.global("scene").getw<Scene>().components.upliftings[m_uplifting_i];

    // Get shared resources
    const auto &e_csys    = e_scene.components.colr_systems[e_uplifting.csys_i];
    const auto &e_basis   = e_scene.resources.bases[e_uplifting.basis_i];
    const auto &e_objects = e_scene.components.objects;
    const auto &e_meshes  = e_scene.resources.meshes;
    const auto &e_images  = e_scene.resources.images;
    
    // Flag tells if the color system spectra have, in any way, been modified
    bool csys_stale = is_first_eval() || e_state.basis_i || e_basis || e_state.csys_i  || e_csys;

    // Flag tells if the resulting tesselation has, in any way, been modified;
    // this is set to true in step 2 if necessary
    bool tssl_stale = is_first_eval();

    // Color system spectra within which the 'uplifted' texture is defined
    auto csys = e_scene.get_csys(e_csys).finalize_direct();

    // 1. Generate color system boundary (spectra)
    if (csys_stale) {
      m_csys_boundary_spectra = generate_ocs_boundary_spec({ .basis   = e_basis.value(),
                                                             .system  = csys,
                                                             .samples = m_csys_boundary_samples });

      // For each spectrum, add a point to the set of tesselation points for later
      m_tesselation_points.resize(m_csys_boundary_spectra.size() + e_uplifting.verts.size());
      std::transform(std::execution::par_unseq, 
        range_iter(m_csys_boundary_spectra), m_tesselation_points.begin(),
        [&](const Spec &s) { return (csys.transpose() * s.matrix()).eval(); });

      fmt::print("Sampled color system boundary, {} points\n", m_csys_boundary_spectra.size());
    }

    // Resize internal objects storing vertex positions, and corresponding spectra;
    // total nr. of vertices = boundary vertices + inner constraint vertices
    m_tesselation_points.resize(m_csys_boundary_spectra.size() + e_uplifting.verts.size());
    m_tesselation_spectra.resize(m_csys_boundary_spectra.size() + e_uplifting.verts.size());

    // Copy boundary spectra over to vertex spectra
    std::copy(std::execution::par_unseq, range_iter(m_csys_boundary_spectra), m_tesselation_spectra.begin());

    // 2. Generate constraint spectra
    // Iterate over stale constraint data, and generate corresponding spectra
    #pragma omp parallel for
    for (int i = 0; i < e_uplifting.verts.size(); ++i) {
      // We only generate a spectrum if the specific vertex was changed,
      // or the entire underlying color system has changed
      guard_continue(e_state.verts[i] || csys_stale);
      const auto &vert = e_uplifting.verts[i];

      // TODO; how to deal with is_active flag?
      //       should probably reserve and push back instead of
      //       pre-resizing 

      // Generate vertex color and attached metamer;
      // this is handled in Scene object to keep it away from the pipeline
      auto [c, s] = e_scene.get_uplifting_constraint(e_uplifting, vert);

      /* // Dependent on type, generate spectral value in a different manner
      if (vert.type == UpliftingConstraint::Type::eColor) {
        // Generate spectral value based on color constraints
        c = vert.colr_i;

        // Obtain all color system spectra referred by this vertex
        std::vector<CMFS> systems = { csys };
        rng::transform(vert.csys_j, std::back_inserter(systems), 
          [&](uint j) { return e_scene.get_csys(j).finalize_direct(); });

        // Obtain corresponding color constraints for each color system
        std::vector<Colr> signals = { c };
        rng::copy(vert.csys_j, std::back_inserter(signals));

        // Generate a metamer satisfying the system+signal constraint set
        s = generate_spectrum({
          .basis              = e_basis.value(),
          .systems            = systems,
          .signals            = signals,
          .impose_boundedness = true,
          .solve_dual         = true
        });
      } else if (vert.type == UpliftingConstraint::Type::eColorOnMesh) {
        // Sample color constraint from mesh, and go from there
        // TODO experiment!
        
        // Obtain relevant mesh and uv coordinates to mesh
        const auto &e_object = e_objects[vert.object_i].value;
        const auto &e_mesh   = e_meshes[e_object.mesh_i].value();
        const auto &e_elem   = e_mesh.elems[vert.object_elem_i];
        eig::Array2f uv = (e_mesh.txuvs[e_elem[0]] * vert.object_elem_bary[0]
                         + e_mesh.txuvs[e_elem[1]] * vert.object_elem_bary[1]
                         + e_mesh.txuvs[e_elem[2]] * vert.object_elem_bary[2])
                        .unaryExpr([](float f) { return std::fmod(f, 1.f); });

        // Sample surface albedo at uv position
        if (e_object.diffuse.index() == 0) {
          c = std::get<0>(e_object.diffuse);
        } else {
          const auto &e_image = e_images[std::get<1>(e_object.diffuse)].value();
          c = e_image.sample(uv, Image::ColorFormat::eSRGB).head<3>();
        }

        // Obtain all color system spectra referred by this vertex
        std::vector<CMFS> systems = { csys };
        rng::transform(vert.csys_j, std::back_inserter(systems), 
          [&](uint j) { return e_scene.get_csys(j).finalize_direct(); });

        // Obtain corresponding color constraints for each color system
        std::vector<Colr> signals = { c };
        rng::copy(vert.csys_j, std::back_inserter(signals));

        // Generate a metamer satisfying the system+signal constraint set
        s = generate_spectrum({
          .basis              = e_basis.value(),
          .systems            = systems,
          .signals            = signals,
          .impose_boundedness = true,
          .solve_dual         = true
        });
      } else if (vert.type == UpliftingConstraint::Type::eMeasurement) {
        // Use measured spectral value directly
        s = vert.measurement;

        // Additionally acquire its color for the tesselation
        c = (csys.transpose() * s.matrix()).eval();
      } */
      
      // Add to set of spectra, and to tesselation input points
      m_tesselation_spectra[m_csys_boundary_spectra.size() + i] = s;

      // We only update vertices in the tesselation if the 'primary' has updated
      // as otherwise we'd trigger re-tesselation on every constraint modification
      Colr prev_c = m_tesselation_points[m_csys_boundary_spectra.size() + i];
      if (!prev_c.isApprox(c)) {
        m_tesselation_points[m_csys_boundary_spectra.size() + i] = c;
        tssl_stale = true; // skipping atomic, as everyone's just setting to true r.n.
      }
    } // for (int i)

    // 3. Generate color system tesselation
    if (tssl_stale) {
      // Generate new tesselation
      m_tesselation = generate_delaunay<AlDelaunay, Colr>(m_tesselation_points);

      // Update packed data for fast per-object delaunay traversal
      std::transform(std::execution::par_unseq, range_iter(m_tesselation.elems), m_tesselation_pack_map.begin(), [&](const auto &el) {
        const auto vts = el | indexed_view(m_tesselation.verts);
        MeshPackLayout pack;
        pack.inv.block<3, 3>(0, 0) = (eig::Matrix3f() 
          << vts[0] - vts[3], vts[1] - vts[3], vts[2] - vts[3]
        ).finished().inverse();
        pack.sub.head<3>() = vts[3];
        return pack;
      });

      fmt::print("Resulting tesselation: {} verts, {} elems\n", m_tesselation.verts.size(), m_tesselation.elems.size());

      // Update packing layout data
      m_tesselation_data_map->elem_offs = max_supported_spectra * m_uplifting_i;
      m_tesselation_data_map->elem_size = m_tesselation.elems.size();

      // Get writeable buffers and flush changes
      auto &i_tesselation_pack = info("tesselation_pack").getw<gl::Buffer>();
      auto &i_tesselation_data = info("tesselation_data").getw<gl::Buffer>();
      i_tesselation_pack.flush(m_tesselation.elems.size() * sizeof(MeshPackLayout));
      i_tesselation_data.flush();
    } // if (tssl_stale)

    // 4. Modify spectral data on the gl-side
    //    Spectra are always changed, so update them either way
    {
      // Assemble packed spectra into mapped buffer. We pack all four spectra of a tetrahedron
      // into a single interleaved object, for fast 4-component texture sampling during rendering
      for (uint i = 0; i < m_tesselation.elems.size(); ++i) {
        const auto &el = m_tesselation.elems[i];
        
        // Data is transposed and reshaped into a [wvls, 4]-shaped object for gpu-side layout
        SpecPackLayout pack;
        for (uint i = 0; i < 4; ++i) {
          pack.col(i) = m_tesselation_spectra[el[i]];
        }
        m_buffer_spec_pack_map[i] = pack.transpose().reshaped(wavelength_samples, 4);
      }

      // Flush changes to GL-side 
      m_buffer_spec_pack.flush();
      
      // Do pixel-buffer copy of packed spectra to sampleable texture
      e_scene.components.upliftings.gl.texture_spectra.set(m_buffer_spec_pack, 0, { wavelength_samples, m_tesselation_data_map->elem_size },
                                                                                  { 0,                  m_tesselation_data_map->elem_offs });
    }
  }
} // namespace met