#include <metameric/core/data.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/linprog.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/pca.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/pipeline/task_gen_color_solids.hpp>
#include <omp.h>
#include <algorithm>
#include <execution>
#include <numbers>
#include <random>
#include <ranges>
#include <unordered_map>

namespace met {
  constexpr uint n_constraints = 4;  // Maximum nr. of secondary color constraints
  constexpr uint n_samples     = 32; // Nr. of samples for OCS generation
  constexpr uint n_subdivs     = 4;  // Nr. of subdivisions for input sphere

  namespace detail {
    // Given a random vector in RN bounded to [-1, 1], return a vector
    // distributed over a gaussian distribution
    auto inv_gaussian_cdf(const auto &x) {
      met_trace();
      auto y = (-(x * x) + 1.f).max(.0001f).log().eval();
      auto z = (0.5f * y + (2.f / std::numbers::pi_v<float>)).eval();
      return (((z * z - y).sqrt() - z).sqrt() * x.sign()).eval();
    }
    
    // Given a random vector in RN bounded to [-1, 1], return a uniformly
    // distributed point on the unit sphere
    auto inv_unit_sphere_cdf(const auto &x) {
      met_trace();
      return inv_gaussian_cdf(x).matrix().normalized().eval();
    }

    // Generate a set of random, uniformly distributed unit vectors in RN
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

  GenColorSolidsTask::GenColorSolidsTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void GenColorSolidsTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Generate reused 6/9/12/X dimensional samples for color solid sampling
    for (uint i = 1; i <= n_constraints; ++i) {
      const uint dims = 3 + 3 * i;
      info.insert_resource(fmt::format("samples_{}", i), detail::gen_unit_dirs_x(n_samples, dims));
    }

    // Register resources to hold convex hull data for each vertex of the gamut shape
    info.insert_resource("csol_data", std::vector<eig::AlArray3f>());
    info.insert_resource("csol_cntr", Colr(0.f));
  }
  
  void GenColorSolidsTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Continue only if vertex/constraint selection is sensible
    auto &e_vert_slct = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection");
    auto &e_cstr_slct = info.get_resource<int>("viewport_overlay", "constr_selection");
    guard(e_vert_slct.size() == 1 && e_cstr_slct != -1);

    // Continue only on relevant state change
    auto &e_view_state = info.get_resource<ViewportState>("state", "viewport_state");
    auto &e_pipe_state = info.get_resource<ProjectState>("state", "pipeline_state");
    guard(e_pipe_state.verts[e_vert_slct[0]].any || e_view_state.vert_selection || e_view_state.cstr_selection);

    // Get shared resources
    auto &e_basis     = info.get_resource<BMatrixType>(global_key, "pca_basis");
    auto &e_appl_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_vert      = e_appl_data.project_data.gamut_verts[e_vert_slct[0]];
    auto &e_mapp      = e_appl_data.loaded_mappings;
    auto &e_spec      = info.get_resource<std::vector<Spec>>("gen_spectral_gamut", "gamut_spec")[e_vert_slct[0]];
    auto &i_csol_data = info.get_resource<std::vector<eig::AlArray3f>>("csol_data");
    auto &i_csol_cntr = info.get_resource<Colr>("csol_cntr");

    // Gather color system spectra and corresponding signals
    // The primary color system and color signal are added first
    // All secondary color systems and signals are added after, until the one given by e_cstr_index
    std::vector<CMFS> cmfs_i = { e_mapp[e_vert.mapp_i].finalize(e_spec) };
    std::vector<Colr> sign_i = { e_vert.colr_i };
    std::copy(e_vert.colr_j.begin(), e_vert.colr_j.begin() + e_cstr_slct, std::back_inserter(sign_i));
    std::transform(e_vert.mapp_j.begin(), e_vert.mapp_j.begin() + e_cstr_slct, std::back_inserter(cmfs_i),
      [&](uint j) { return e_mapp[j].finalize(e_spec); });

    // The selected constraint is the varying component, for which we generate a metamer boundary
    CMFS cmfs_j = e_mapp[e_vert.mapp_j[e_cstr_slct]].finalize(e_spec);

    // Obtain 6/9/12/X dimensional random unit vectors for the given configration
    const auto &i_samples = info.get_resource<std::vector<eig::ArrayXf>>(fmt::format("samples_{}", cmfs_i.size()));

    // Generate points on metamer set boundary
    auto basis  = e_basis.rightCols(wavelength_bases);
    auto points = generate_boundary_i(basis, cmfs_i, sign_i, cmfs_j, i_samples);

    // Store in aligned format // TODO generate in aligned format, you numbskull
    i_csol_data = std::vector<eig::AlArray3f>(range_iter(points));
    
    // Compute center of metamer set boundary
    constexpr auto f_add = [](const auto &a, const auto &b) { return (a + b).eval(); };
    i_csol_cntr = std::reduce(std::execution::par_unseq, range_iter(i_csol_data), 
      eig::AlArray3f(0.f), f_add) / static_cast<float>(i_csol_data.size());
  }
} // namespace met