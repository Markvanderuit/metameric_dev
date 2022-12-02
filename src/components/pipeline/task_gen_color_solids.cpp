#include <metameric/components/pipeline/task_gen_color_solids.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/linprog.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/pca.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <omp.h>
#include <algorithm>
#include <execution>
#include <numbers>
#include <random>
#include <ranges>
#include <unordered_map>

namespace met {
  constexpr uint n_samples = 32; // Nr. of samples for OCS generation
  constexpr uint n_subdivs = 4; // Nr. of subdivisions for input sphere

  namespace detail {
    // Given a random vector in RN bounded to [-1, 1], return a vector
    // distributed over a gaussian distribution
    template <uint N>
    eig::Array<float, N, 1> inv_gaussian_cdf(const eig::Array<float, N, 1> &x) {
      met_trace();
      
      using ArrayNf = eig::Array<float, N, 1>;

      auto y = (ArrayNf(1.f) - x * x).max(.0001f).log().eval();
      auto z = (0.5f * y + (2.f / std::numbers::pi_v<float>)).eval();
      return (((z * z - y).sqrt() - z).sqrt() * x.sign()).eval();
    }
    
    // Given a random vector in RN bounded to [-1, 1], return a uniformly
    // distributed point on the unit sphere
    template <uint N>
    eig::Array<float, N, 1> inv_unit_sphere_cdf(const eig::Array<float, N, 1> &x) {
      met_trace();
      return inv_gaussian_cdf<N>(x).matrix().normalized().eval();
    }

    // Generate a set of random, uniformly distributed unit vectors in RN
    template <uint N>
    std::vector<eig::Array<float, N, 1>> generate_unit_dirs(uint n_samples) {
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
          unit_dirs[i] = detail::inv_unit_sphere_cdf<N>(v);
        }
      }

      return unit_dirs;
    }
  } // namespace detail

  GenColorSolidsTask::GenColorSolidsTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void GenColorSolidsTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_app_data  = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data = e_app_data.project_data;
    
    // Generate reused 6d samples and a uv sphere mesh for faster OCS generation
    m_sphere_samples = detail::generate_unit_dirs<6>(n_samples);
    m_sphere_mesh = generate_spheroid<HalfedgeMeshTraits>(n_subdivs);

    // Register resources to hold convex hull data for each vertex of the gamut shape
    info.insert_resource("ocs_points", std::vector<std::vector<eig::AlArray3f>>());
    info.insert_resource("ocs_centers", std::vector<Colr>());
    info.insert_resource("ocs_chulls", std::vector<HalfedgeMesh>());
  }
  
  void GenColorSolidsTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Continue only on relevant state change
    auto &e_state_gamut = info.get_resource<std::vector<CacheState>>("project_state", "gamut_summary");
    guard(std::ranges::any_of(e_state_gamut, [](auto s) { return s == CacheState::eStale; }));

    // Get shared resources
    auto &e_app_data     = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data    = e_app_data.project_data;
    auto &e_gamut_colr_i = e_proj_data.gamut_colr_i;
    auto &e_gamut_mapp_i = e_proj_data.gamut_mapp_i;
    auto &e_gamut_mapp_j = e_proj_data.gamut_mapp_j;
    auto &e_basis        = info.get_resource<BMatrixType>(global_key, "pca_basis");
    auto &e_gamut_spec   = info.get_resource<std::vector<Spec>>("gen_spectral_gamut", "gamut_spec");
    auto &i_ocs_data     = info.get_resource<std::vector<std::vector<eig::AlArray3f>>>("ocs_points");
    auto &i_ocs_cntrs    = info.get_resource<std::vector<Colr>>("ocs_centers");
    auto &i_ocs_hulls    = info.get_resource<std::vector<HalfedgeMesh>>("ocs_chulls");

    // Deal with resized gamut
    if (e_state_gamut.size() != i_ocs_data.size()) {
      i_ocs_data.resize(e_state_gamut.size());
      i_ocs_cntrs.resize(e_state_gamut.size());
      i_ocs_hulls.resize(e_state_gamut.size());
    }

    // Describe ranges over stale gamut vertices
    auto vert_range = std::views::iota(0u, static_cast<uint>(e_state_gamut.size()))
                    | std::views::filter([&](uint i) { return e_state_gamut[i] == CacheState::eStale; });

    // For each vertex of the gamut shape
    for (uint i : vert_range) {
      // Generate color system spectra
      CMFS cmfs_i = e_app_data.loaded_mappings[e_gamut_mapp_i[i]].finalize(e_gamut_spec[i]);
      CMFS cmfs_j = e_app_data.loaded_mappings[e_gamut_mapp_j[i]].finalize(e_gamut_spec[i]);

      // Generate points on metamer set boundary
      auto basis  = e_basis.rightCols(wavelength_bases);
      auto points = generate_boundary(basis, cmfs_i, cmfs_j, e_gamut_colr_i[i], m_sphere_samples);

      // Store in aligned format // TODO generate in aligned format
      i_ocs_data[i] = std::vector<eig::AlArray3f>(range_iter(points));
      
      // Compute center of metamer set boundary
      constexpr auto f_add = [](const auto &a, const auto &b) { return (a + b).eval(); };
      i_ocs_cntrs[i] = std::reduce(std::execution::par_unseq, range_iter(i_ocs_data[i]), 
        eig::AlArray3f(0.f), f_add) / static_cast<float>(i_ocs_data[i].size());
    }

    // Process convex hull generation worklist in parallel
    std::vector<uint> vert_indices(range_iter(vert_range));
    std::for_each(std::execution::par_unseq, range_iter(vert_indices), [&](uint i) {
      i_ocs_hulls[i] = generate_convex_hull<HalfedgeMeshTraits, eig::AlArray3f>(i_ocs_data[i], m_sphere_mesh);
    });
  }
} // namespace met