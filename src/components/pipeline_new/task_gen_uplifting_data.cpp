#include <metameric/core/distribution.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/pipeline_new/task_gen_uplifting_data.hpp>
#include <omp.h>
#include <execution>
#include <numbers>
#include <small_gl/utility.hpp>

namespace met {
  constexpr uint n_system_boundary_samples = 128u;

  namespace detail {
    // Given a random vector in RN bounded to [-1, 1], return a vector
    // distributed over a gaussian distribution
    inline
    auto inv_gaussian_cdf(const auto &x) {
      met_trace();
      auto y = (-(x * x) + 1.f).max(.0001f).log().eval();
      auto z = (0.5f * y + (2.f / std::numbers::pi_v<float>)).eval();
      return (((z * z - y).sqrt() - z).sqrt() * x.sign()).eval();
    }
    
    // Given a random vector in RN bounded to [-1, 1], return a uniformly
    // distributed point on the unit sphere
    inline
    auto inv_unit_sphere_cdf(const auto &x) {
      met_trace();
      return inv_gaussian_cdf(x).matrix().normalized().eval();
    }

    // Generate a set of random, uniformly distributed unit vectors in RN
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
    const auto &e_scene = info.global("scene").getr<Scene>();
    return e_scene.components.upliftings[m_uplifting_i];
  }

  void GenUpliftingDataTask::init(SchedulerHandle &info) {
    met_trace_full();
    m_csys_boundary_samples = detail::gen_unit_dirs<3>(n_system_boundary_samples);
    info("spectra").set<std::vector<Spec>>({ });
    info("tesselation").set<AlDelaunay>({ });
  }

  void GenUpliftingDataTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    const auto &e_scene       = info.global("scene").getr<Scene>();
    const auto &[e_uplifting, 
                 e_state]     = e_scene.components.upliftings[m_uplifting_i];
    const auto &e_csys        = e_scene.components.colr_systems[e_uplifting.csys_i];
    const auto &e_basis       = e_scene.resources.bases[e_uplifting.basis_i];
    const auto &e_objects     = e_scene.components.objects;
    const auto &e_meshes      = e_scene.resources.meshes;
    const auto &e_images      = e_scene.resources.images;
          auto &i_spectra     = info("spectra").getw<std::vector<Spec>>();
          auto &i_tesselation = info("tesselation").getw<AlDelaunay>();
    
    // Internal state helper flags
    bool generally_stale   = e_state.basis_i || e_basis || e_state.csys_i  || e_csys;
    bool tesselation_stale = generally_stale;

    // Baseline color system spectra in which the 'uplifted' texture is defined
    CMFS csys = e_scene.get_csys(e_csys).finalize_direct();

    // 1. Generate color system boundary spectra on relevant state change
    if (generally_stale) {
      m_csys_boundary_spectra = generate_ocs_boundary_spec({ .basis      = e_basis.value(),
                                                             .system     = csys,
                                                             .samples    = m_csys_boundary_samples });

      // For each spectrum, add a point to the set of tesselation points for later
      m_tesselation_points.resize(m_csys_boundary_spectra.size() + e_uplifting.verts.size());
      std::transform(std::execution::par_unseq, 
        range_iter(m_csys_boundary_spectra), m_tesselation_points.begin(),
        [&](const Spec &s) { return (csys.transpose() * s.matrix()).eval(); });

      fmt::print("Sampled color system boundary, {} unique points\n", m_csys_boundary_spectra.size());
    }

    // Resize relevant objects
    i_spectra.resize(m_csys_boundary_spectra.size() + e_uplifting.verts.size());
    m_tesselation_points.resize(m_csys_boundary_spectra.size() + e_uplifting.verts.size());

    // 2. Generate constraint spectra
    // Iterate over stale constraint data, and generate corresponding spectra
    #pragma omp parallel for
    for (int i = 0; i < e_uplifting.verts.size(); ++i) {
      // We only generate a spectrum if the specific vertex is stale
      guard_continue(e_state.verts[i] || generally_stale);
      const auto &vert = e_uplifting.verts[i];
      
      Spec s;
      Colr c;

      // Dependent on type, generate spectral value in a different manner
      if (vert.type == Uplifting::Constraint::Type::eColor) {
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
          .basis      = e_basis.value(),
          .systems    = systems,
          .signals    = signals,
          .solve_dual = true
        });
      } else if (vert.type == Uplifting::Constraint::Type::eColorOnMesh) {
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
          .basis      = e_basis.value(),
          .systems    = systems,
          .signals    = signals,
          .solve_dual = true
        });
      } else if (vert.type == Uplifting::Constraint::Type::eMeasurement) {
        // Use measured spectral value directly
        s = vert.measurement;

        // Additionally acquire its color for the tesselation
        c = (csys.transpose() * s.matrix()).eval();
      }
      
      // Add to set of spectra, and to tesselation input points
      i_spectra[m_csys_boundary_spectra.size() + i] = s;

      // We only store colors for tesselation if the 'primary' color has updated
      // as otherwise it'd trigger on every constraint modification
      Colr prev_c = m_tesselation_points[m_csys_boundary_spectra.size() + i];
      if (!prev_c.isApprox(c)) {
        m_tesselation_points[m_csys_boundary_spectra.size() + i] = c;
        tesselation_stale = true; // skipping atomic, as everyone's just setting to true r.n.
      }
    } // for (int i)

    // 3. Generate color system tesselation
    if (tesselation_stale) {
      i_tesselation = generate_delaunay<AlDelaunay, Colr>(m_tesselation_points);
    }

    // 4. Consolidate and upload data to GL-side in one nice place
    {
      // TODO start here
      // ...
    }
  }
} // namespace met