#include <metameric/core/data.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/linprog.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/components/pipeline/task_gen_mismatch_solid.hpp>
#include <omp.h>
#include <algorithm>
#include <execution>
#include <numbers>
#include <random>
#include <ranges>

namespace met {
  constexpr uint n_samples     = 64;   // Nr. of samples for metamer mismatch volume OCS generation
  constexpr uint n_constraints = 4;    // Maximum nr. of secondary color constraints

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

  void GenMismatchSolidTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_appl_data = info.global("app_data").read_only<ApplicationData>();
    const auto &e_proj_data = e_appl_data.project_data;

    // Generate reused 6/9/12/X dimensional samples for color solid sampling
    for (uint i = 1; i <= n_constraints; ++i) {
      const uint dims = 3 + 3 * i;
      info.resource(fmt::format("samples_{}", i)).set(detail::gen_unit_dirs_x(n_samples, dims));
    }

    // Register resources to hold convex hull data
    info.resource("chull_mesh").set<AlignedMeshData>({ });
    info.resource("chull_cntr").set(Colr(0.f));
  }

  bool GenMismatchSolidTask::is_active(SchedulerHandle &info) {
    met_trace_full();
    
    const auto &e_cstr_slct = info.resource("viewport.overlay", "constr_selection").read_only<int>();
    const auto &e_vert_slct = info.resource("viewport.input.vert", "selection").read_only<std::vector<uint>>();

    guard(e_cstr_slct != -1 && !e_vert_slct.empty(), false);

    const auto &e_view_state = info.resource("state", "viewport_state").read_only<ViewportState>();
    const auto &e_pipe_state = info.resource("state", "pipeline_state").read_only<ProjectState>();
    
    return e_pipe_state.verts[e_vert_slct[0]].any || e_view_state.vert_selection || e_view_state.cstr_selection;
  }
  
  void GenMismatchSolidTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_cstr_slct = info.resource("viewport.overlay", "constr_selection").read_only<int>();
    const auto &e_vert_slct = info.resource("viewport.input.vert", "selection").read_only<std::vector<uint>>();
    const auto &e_vert_sd   = info.resource("gen_spectral_data", "vert_spec").read_only<std::vector<Spec>>()[e_vert_slct[0]];
    const auto &e_appl_data = info.global("app_data").read_only<ApplicationData>();
    const auto &e_proj_data = e_appl_data.project_data;
    const auto &e_vert      = e_appl_data.project_data.vertices[e_vert_slct[0]];

    // Gather color system spectra and corresponding signals
    // The primary color system and color signal are added first
    // All secondary color systems and signals are added after, until the one given by e_cstr_index
    std::vector<CMFS> cmfs_i = { e_proj_data.csys(e_vert.csys_i).finalize_indirect(e_vert_sd) };
    std::vector<Colr> sign_i = { e_vert.colr_i };
    std::copy(e_vert.colr_j.begin(), e_vert.colr_j.begin() + e_cstr_slct, std::back_inserter(sign_i));
    std::transform(e_vert.csys_j.begin(), e_vert.csys_j.begin() + e_cstr_slct, std::back_inserter(cmfs_i),
      [&](uint j) { return e_proj_data.csys(j).finalize_indirect(e_vert_sd); });

    // The selected constraint is the varying component, for which we generate a metamer boundary
    CMFS cmfs_j = e_proj_data.csys(e_vert.csys_j[e_cstr_slct]).finalize_indirect(e_vert_sd);

    // Obtain 6/9/12/X dimensional random unit vectors for the given configration
    const auto &i_samples = info.resource(fmt::format("samples_{}", cmfs_i.size())).read_only<std::vector<eig::ArrayXf>>();

    // Generate points on metamer set boundary; store in aligned format
    auto data = generate_mismatch_boundary({ .basis     = e_appl_data.loaded_basis, 
                                             .basis_avg = e_appl_data.loaded_basis_mean, 
                                             .systems_i = cmfs_i, 
                                             .signals_i = sign_i, 
                                             .system_j  = cmfs_j, 
                                             .samples   = i_samples });
    
    // Generate cleaned mesh from data
    auto mesh = generate_convex_hull<AlignedMeshData, eig::Array3f>(data);

    // Compute center of metamer set boundary
    constexpr auto f_add = [](const auto &a, const auto &b) { return (a + b).eval(); };
    auto cntr = std::reduce(std::execution::par_unseq, range_iter(data), Colr(0.f), f_add)
              / static_cast<float>(data.size());

    // Submit mesh data
    info.resource("chull_mesh").writeable<AlignedMeshData>() = std::move(mesh);
    info.resource("chull_cntr").writeable<Colr>() = cntr;
  }
} // namespace met