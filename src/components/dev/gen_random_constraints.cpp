#include <metameric/core/data.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/components/dev/gen_random_constraints.hpp>
#include <omp.h>
#include <algorithm>
#include <execution>
#include <numbers>
#include <random>
#include <ranges>

namespace met {
  constexpr uint n_attempts   = 16; // Nr. of images to generate
  constexpr uint n_samples    = 64; // Nr. of samples for color system OCS generation

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
    inline
    std::vector<eig::ArrayXf> gen_unit_dirs_x(uint n_samples, uint n_dims) {
      met_trace();

      // Generate separate seeds for each thread's rng
      std::random_device rd;
      std::vector<uint> seeds(omp_get_max_threads());
      for (auto &s : seeds) 
        s = rd();

      std::vector<eig::ArrayXf> unit_dirs(n_samples);

      #pragma omp parallel
      {
        // Initialize separate random number generator per thread
        std::mt19937 rng(seeds[omp_get_thread_num()]);
        std::uniform_real_distribution<float> distr(-1.f, 1.f);

        // Draw samples for this thread's range
        #pragma omp for
        for (int i = 0; i < unit_dirs.size(); ++i) {
          eig::ArrayXf v(n_dims);
          for (auto &f : v) f = distr(rng);
          unit_dirs[i] = detail::inv_unit_sphere_cdf(v);
        }
      }

      return unit_dirs;
    }

    // Generate a set of random, uniformly distributed unit vectors in RN
    template <uint N>
    inline
    std::vector<eig::Array<float, N, 1>> gen_unit_dirs(uint n_samples) {
      met_trace();
      
      using ArrayNf = eig::Array<float, N, 1>;
      using SeedTy = std::random_device::result_type;

      // Generate separate seeds for each thread's rng
      std::random_device rd;
      std::vector<uint> seeds(omp_get_max_threads());
      for (auto &s : seeds) 
        s = rd();

      std::vector<ArrayNf> unit_dirs(n_samples);
      #pragma omp parallel
      {
        // Initialize separate random number generator per thread
        std::mt19937 rng(seeds[omp_get_thread_num()]);
        std::uniform_real_distribution<float> distr(-1.f, 1.f);

        // Draw samples for this thread's range
        #pragma omp for
        for (int i = 0; i < unit_dirs.size(); ++i) {
          ArrayNf v;
          for (auto &f : v) f = distr(rng);
          unit_dirs[i] = detail::inv_unit_sphere_cdf(v);
        }
      }

      return unit_dirs;
    }
  } // namespace detail

  void GenRandomConstraints::init(SchedulerHandle &info) {
    met_trace();

    info("constraints").set<std::vector<std::vector<ProjectData::Vert>>>({ });

    m_has_run_once = false;
  }

  bool GenRandomConstraints::is_active(SchedulerHandle &info) {
    met_trace();

    // Get external resources
    const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data = e_appl_data.project_data;

    return e_proj_data.color_systems.size() > 1 && !m_has_run_once;
  }

  void GenRandomConstraints::eval(SchedulerHandle &info) {
    met_trace();
    
    // Get external resources
    const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data = e_appl_data.project_data;
    const auto &e_verts     = e_proj_data.verts;

    // Get modified resources
    auto &i_constraints = info("constraints").writeable<std::vector<std::vector<ProjectData::Vert>>>();

    // Resize constraints data to correct format
    i_constraints.resize(n_attempts);
    for (auto &constraints : i_constraints)
      constraints.resize(e_verts.size());

    // Provide items necessary for fast OCS generation
    auto samples_6d = detail::gen_unit_dirs_x(n_samples, 6);
    std::vector<CMFS> cmfs_i = { e_proj_data.csys(0).finalize_direct() }; // TODO ehhr
    std::vector<CMFS> cmfs_j = { e_proj_data.csys(1).finalize_direct() }; // TODO uhhr

    // Iterate through base vertex data step-by-step
    for (uint i = 0; i < e_verts.size(); ++i) {
      const auto &vert = e_verts[i];

      // Provide items necessary for OCS generation
      std::vector<Colr> sign_i = { vert.colr_i };

      // Generate boundary points over mismatch volume; these points lie on a convex hull
      auto ocs_gen_data = generate_mismatch_boundary({ .basis      = e_appl_data.loaded_basis, 
                                                       .basis_mean = e_appl_data.loaded_basis_mean, 
                                                       .systems_i  = cmfs_i, 
                                                       .signals_i  = sign_i, 
                                                       .system_j   = cmfs_j.front(), 
                                                       .samples    = samples_6d });

      // Generate a delaunay tesselation of the convex hull
      auto [del_verts, del_elems] = generate_delaunay<AlignedDelaunayData, eig::Array3f>(ocs_gen_data);

      // Compute volume of each tetrahedron in delaunay
      std::vector<float> del_volumes(del_elems.size());
      std::transform(std::execution::par_unseq, range_iter(del_elems), del_volumes.begin(),
      [&](const eig::Array4u &el) {
        // Get vertex positions for this tetrahedron
        std::array<eig::Vector3f, 4> p;
        std::ranges::transform(el, p.begin(), [&](uint i) { return del_verts[i]; });

        // Compute tetrahedral volume
        float nom = std::abs((p[0] - p[3]).dot((p[1] - p[3]).cross(p[2] - p[3])));
        return nom / 6.f;
      });

      // Normalize volume distribution by its sum
      float del_volume_sum = std::reduce(range_iter(del_volumes));
      std::for_each(range_iter(del_volumes), [&del_volume_sum](float &f) { f /= del_volume_sum; });

      // Generate cumulative density over normalized volumes
      std::vector<float> del_cumulative(del_elems.size());
      std::exclusive_scan(range_iter(del_volumes), del_cumulative.begin(), 0.f);

      // Start drawing samples
      for (uint i = 0; i < n_samples; ++i) {
        // First, sample a tetrahedron uniformly from the above CDF

        // Next, sample a position inside the tetrahedron uniformly

      }
    }

    m_has_run_once = true;
  }
} // namespace met