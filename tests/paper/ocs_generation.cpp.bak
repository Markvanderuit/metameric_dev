#include <catch2/catch_test_macros.hpp>
#include <metameric/core/detail/packing.hpp>
#include <metameric/core/constraints.hpp>
#include <metameric/core/matching.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/moments.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/distribution.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/schedule.hpp>
#include <metameric/components/misc/task_frame_begin.hpp>
#include <metameric/components/misc/task_frame_end.hpp>
#include <metameric/components/views/task_window.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/task_viewport.hpp>
#include <metameric/components/views/detail/task_arcball_input.hpp>
#include <small_gl/window.hpp>
#include <small_gl/program.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/array.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/utility.hpp>
#include <oneapi/tbb/concurrent_vector.h>
#include <algorithm>
#include <execution>
#include <sstream>

using namespace met;

namespace test {
  // Given a random vector in RN bounded to [-1, 1], return a vector
  // distributed over a gaussian distribution
  template <uint N>
  inline auto inv_gaussian_cdf(const eig::Array<float, N, 1> &x) {
    auto y = (-(x * x) + 1.f).max(.0001f).log().eval();
    auto z = (0.5f * y + (2.f / std::numbers::pi_v<float>)).eval();
    return (((z * z - y).sqrt() - z).sqrt() * x.sign()).eval();
  }
  
  // Given a random vector in RN bounded to [-1, 1], return a uniformly
  // distributed point on the unit sphere
  template <uint N>
  inline auto inv_unit_sphere_cdf(const eig::Array<float, N, 1> &x) {
    return inv_gaussian_cdf<N>(x).matrix().normalized().array().eval();
  }
  
  // Generate a set of random, uniformly distributed unit vectors in RN
  template <uint N>
  inline auto gen_unit_dirs(uint n_samples, uint seed = 4) {
    met_trace();

    std::vector<eig::Array<float, N, 1>> unit_dirs(n_samples);

    if (n_samples <= 16) {
      UniformSampler sampler(-1.f, 1.f, seed);
      for (int i = 0; i < unit_dirs.size(); ++i)
        unit_dirs[i] = inv_unit_sphere_cdf<N>(sampler.next_nd<N>());
    } else {
      UniformSampler sampler(-1.f, 1.f, seed);
      #pragma omp parallel
      { // Draw samples for this thread's range with separate sampler per thread
        UniformSampler sampler(-1.f, 1.f, seed + static_cast<uint>(omp_get_thread_num()));
        #pragma omp for
        for (int i = 0; i < unit_dirs.size(); ++i)
          unit_dirs[i] = inv_unit_sphere_cdf<N>(sampler.next_nd<N>());
      }
    }

    return unit_dirs;
  }

  // Mapping from static to dynamic, so we can vary the nr. of objectives for which samples are generated
  auto gen_unit_dirs(uint n_objectives, uint n_samples, uint seed = 4) {
    constexpr auto eig_static_to_dynamic = [](const auto &v) { return (eig::ArrayXf(v.size()) << v).finished(); };
    std::vector<eig::ArrayXf> X(n_samples);
    switch (n_objectives) {
      case 1: rng::transform(gen_unit_dirs<3>(n_samples, seed),  X.begin(), eig_static_to_dynamic); break;
      case 2: rng::transform(gen_unit_dirs<6>(n_samples, seed),  X.begin(), eig_static_to_dynamic); break;
      case 3: rng::transform(gen_unit_dirs<9>(n_samples, seed),  X.begin(), eig_static_to_dynamic); break;
      case 4: rng::transform(gen_unit_dirs<12>(n_samples, seed), X.begin(), eig_static_to_dynamic); break;
      case 5: rng::transform(gen_unit_dirs<15>(n_samples, seed), X.begin(), eig_static_to_dynamic); break;
      default: debug::check_expr(false, "Not implemented!");
    }
    return X;
  }
} // namespace test

std::vector<Spec> generate_ocs(const DirectColorSystemOCSInfo &info) {
  // Sample unit vectors in 3d
  auto samples = test::gen_unit_dirs<3>(info.n_samples, info.seed);

  // Output for parallel solve
  std::vector<Spec> out(samples.size());

  // Parallel solve for boundary spectra
  auto A = info.direct_objective.finalize();
  #pragma omp parallel for
  for (int i = 0; i < samples.size(); ++i) {
    // Obtain actual spectrum by projecting sample onto optimal
    Spec s = (A * samples[i].matrix()).eval();
    for (auto &f : s)
      f = f >= 0.f ? 1.f : 0.f;
    out[i] = s;
  }

  return out;
}

TEST_CASE("ocs_generation") {
  // Load spectral basis
  // Normalize if they not already normalized
  auto basis = io::load_basis("resources/misc/basis_262144.txt");
  for (auto col : basis.func.colwise()) {
    auto min_coeff = col.minCoeff(), max_coeff = col.maxCoeff();
    col /= std::max(std::abs(max_coeff), std::abs(min_coeff));
  }

  // Define color system
  ColrSystem csys = {
    .cmfs       = models::cmfs_cie_xyz,
    .illuminant = models::emitter_cie_e
  };

  // Find boundary spectra
  auto ocs_srgb = generate_ocs({
    .direct_objective = csys,
    .basis            = basis,
    .n_samples        = 16384
  }) | vws::transform([&](const Spec &s) { return csys(s); })
    //  | vws::filter([](const auto &v) { return !v.isZero(); })
     | vws::transform(lrgb_to_srgb)
     | rng::to<std::vector>();
  // auto ocs_xyY = ocs_rgb
  //    | vws::transform(lrgb_to_xyz)
  //    | vws::transform(xyz_to_xyY)
  //    | rng::to<std::vector>();
  // auto ocs = generate_color_system_ocs({
  //   .direct_objective = csys,
  //   .basis            = basis,
  //   .n_samples        = 256
  // });


  // Print to string
  std::stringstream ss;
  for (const auto &v : ocs_srgb)
    ss << fmt::format("{}, ", v);
  ss << '\n';

  // Save to file
  // fs::path out_path = std::format("C:/Data/Dump/ocs_srgb_{}.txt", wavelength_bases);
  fs::path out_path = std::format("C:/Data/Dump/ocs_srgb_full.txt", wavelength_bases);
  io::save_string(out_path, ss.str());


  /* // Setup OpenGL components
  auto al = std::vector<eig::AlArray3f>(range_iter(ocs));
  gl::Buffer verts = {{ .data = cnt_span<const std::byte>(al)}};


  // Setup program loop
  {
    // Scheduler is responsible for handling application tasks, resources, and runtime loop
    LinearScheduler scheduler;

    // Initialize window (OpenGL context), as a resource owned by the scheduler
    auto &window = scheduler.global("window").init<gl::Window>({ 
      .size  = { 1024, 1024 }, 
      .title = "Test window, pls ignore", 
      .flags = gl::WindowFlags::eVisible   | gl::WindowFlags::eFocused 
             | gl::WindowFlags::eDecorated | gl::WindowFlags::eResizable 
             | gl::WindowFlags::eMSAA met_debug_insert(| gl::WindowFlags::eDebug)
    }).getw<gl::Window>();

    // Initialize OpenGL debug messages, if requested
    if constexpr (met_enable_debug) {
      gl::debug::enable_messages(gl::DebugMessageSeverity::eLow, gl::DebugMessageTypeFlags::eAll);
      gl::debug::insert_message("OpenGL debug messages are active!", gl::DebugMessageSeverity::eLow);
    }

    // Initialize program cache, as a resource owned by the scheduler
    scheduler.global("cache").set<gl::ProgramCache>({ });

    // Initialize schedule
    scheduler.task("frame_begin").init<FrameBeginTask>();
    scheduler.task("window").init<WindowTask>();
    scheduler.task("frame_end").init<FrameEndTask>();
    
    // Create and start runtime loop
    while (!window.should_close())
      scheduler.run();
  } */
}