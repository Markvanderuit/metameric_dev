#include <metameric/core/data.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/components/pipeline/task_gen_color_system_solid.hpp>
#include <omp.h>
#include <algorithm>
#include <execution>
#include <numbers>
#include <random>
#include <ranges>

namespace met {
  constexpr uint n_samples = 1024; // Nr. of samples for color system OCS generation

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

      using SeedTy = std::random_device::result_type;

      // Generate separate seeds for each thread's rng
      std::random_device rd;
      std::vector<SeedTy> seeds(omp_get_max_threads());
      for (auto &s : seeds) s = rd();

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
      std::vector<SeedTy> seeds(omp_get_max_threads());
      for (auto &s : seeds) s = rd();

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

  bool GenColorSystemSolidTask::is_active(SchedulerHandle &info) {
    met_trace();
    return info("state", "proj_state").read_only<ProjectState>().csys[0];
  }

  void GenColorSystemSolidTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data = e_appl_data.project_data;

    // Compute data points on convex hull of object color solid
    auto data = generate_ocs_boundary({ .basis      = e_appl_data.loaded_basis,
                                        .basis_mean = e_appl_data.loaded_basis_mean, 
                                        .system     = e_proj_data.csys(0).finalize_direct(), 
                                        .samples    = detail::gen_unit_dirs<3>(n_samples) });

    // Generate cleaned mesh from data
    auto mesh = simplify_edge_length<AlignedMeshData>(
      generate_convex_hull<HalfedgeMeshData, eig::Array3f>(data), 0.001f);

    // Compute center of convex hull
    constexpr auto f_add = [](const auto &a, const auto &b) { return a + b; };
    auto cntr = std::reduce(std::execution::par_unseq, range_iter(mesh.verts), eig::AlArray3f(0), f_add) 
              / static_cast<float>(mesh.verts.size());

    // Submit mesh and center as resources; used by viewport draw tasks
    info("chull_mesh").set<AlignedMeshData>(std::move(mesh));
    info("chull_cntr").set<eig::Array3f>(cntr);
  }
} // namespace met