#include <metameric/core/io.hpp>
#include <metameric/core/knn.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/pca.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/schedule.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/app/application.hpp>

#include <small_gl/buffer.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <small_gl/window.hpp>
#include <small_gl_parser/parser.hpp>

#include <algorithm>
#include <execution>
#include <ranges>

namespace met {
  namespace detail {
    void init_state(LinearScheduler &scheduler, ApplicationCreateInfo info) {
      met_trace();
      
      ApplicationData state;      
      if (!info.project_path.empty()) {
        state.load(info.project_path);
      } else {
        state.unload();
      }
      scheduler.insert_resource("app_data", std::move(state));
    }

    void init_parser(LinearScheduler &scheduler) {
      glp::Parser parser;
      parser.add_string("MET_SUBGROUP_SIZE", 
        std::to_string(gl::state::get_variable_int(gl::VariableName::eSubgroupSize)));
      scheduler.insert_resource("glsl_parser", std::move(parser));
    }

    void init_schedule(LinearScheduler &scheduler) {
      auto &app_data = scheduler.get_resource<ApplicationData>(global_key, "app_data");
      if (app_data.project_state == ProjectState::eSaved) {
        submit_schedule_main(scheduler);
      } else {
        submit_schedule_empty(scheduler);
      }
    }

    void init_spectral_grid(LinearScheduler &scheduler, ApplicationCreateInfo info) {
      met_trace();

      // Load input data 
      auto hd5_data = io::load_hd5(info.database_path, "TotalRefs");

      // Input data layout
      const float data_minv  = 400.f,
                  data_maxv  = 710.f,
                  data_ssize = (data_maxv - data_minv) / static_cast<float>(hd5_data.dims);
      auto idx_to_data = [&](uint i) { return (.5f + static_cast<float>(i)) * data_ssize + data_minv; };

      // Fill list of wavelengths matching data layout for spectrum_from_data(...)
      std::vector<float> wavelengths(hd5_data.dims);
      std::ranges::copy(std::views::iota(0u, static_cast<uint>(hd5_data.dims)) 
        | std::views::transform(idx_to_data), wavelengths.begin());
      
      // Convert data into metameric's spectral format
      std::vector<Spec> internal_sd(hd5_data.size);
      std::transform(std::execution::par_unseq, range_iter(hd5_data.data), 
        internal_sd.begin(), [&](const auto &v) {  return io::spectrum_from_data(wavelengths, v); });

      // Test PCA generation using a bunch of randomly chosen vectors
      Mapp mapp = { .cmfs = models::cmfs_srgb, .illuminant = models::emitter_cie_d65 };
      std::vector<Spec> pca_input(32768);
      #pragma omp parallel for
      for (int i = 0; i < pca_input.size(); ++i)
        pca_input[i] = internal_sd[128 * i];
      auto pca_basis = eigen_vectors(covariance_matrix(pca_input));
      auto pca_orth  = orthogonal_complement(mapp.finalize(), pca_basis);
      scheduler.insert_resource<decltype(pca_orth)>("pca_orth", std::move(pca_orth));
      scheduler.insert_resource<BMatrixType>("pca_basis", std::move(pca_basis));
      scheduler.insert_resource<std::vector<Spec>>("pca_input", std::move(pca_input));

      // Create a KNN grid over the spectral distributions,based on their color as a position
      // TODO remove
      /* fmt::print("Constructing KNN grid\n");
      std::vector<eig::Array3f> knn_positions(internal_sd.size());
      std::transform(std::execution::par_unseq, range_iter(internal_sd), knn_positions.begin(),
        [&](const Spec &sd) { return reflectance_to_color(sd, { .cmfs = models::cmfs_srgb }); });
      KNNGrid<Spec> knn_grid = {{ .grid_size = 32, .max_cell_size = 128 }};
      knn_grid.insert_n(internal_sd, knn_positions);
      knn_grid.retrace_size();
      fmt::print("Constructed KNN grid\n"); */
      
      // Make resources available through application data object
      // TODO remove
      // auto &app_data = scheduler.get_resource<ApplicationData>(global_key, "app_data");
      // app_data.loaded_knn_grid = std::move(knn_grid); 
    }
  } // namespace detail                          

  void create_application(ApplicationCreateInfo info) {
    fmt::print("Metameric format\n  min : {} nm\n  max : {} nm\n  samples: {}\n",
      wavelength_min, wavelength_max, wavelength_samples);

    // Scheduler is responsible for handling application tasks and resources
    LinearScheduler scheduler;

    // Initialize OpenGL context (and primary window) and submit to scheduler
    gl::Window &window = scheduler.emplace_resource<gl::Window>("window", { 
      .size  = { 1680, 1024 }, 
      .title = "Metameric",
      .flags = gl::WindowCreateFlags::eVisible
             | gl::WindowCreateFlags::eFocused   
             | gl::WindowCreateFlags::eDecorated
             | gl::WindowCreateFlags::eResizable
             | gl::WindowCreateFlags::eSRGB
             | gl::WindowCreateFlags::eMSAA
#if defined(NDEBUG) || defined(MET_ENABLE_DBG_EXCEPTIONS)
             | gl::WindowCreateFlags::eDebug 
#endif
    });

    // Enable OpenGL debug messages, ignoring notification-type messages
#if defined(NDEBUG) || defined(MET_ENABLE_DBG_EXCEPTIONS)
    gl::debug::enable_messages(gl::DebugMessageSeverity::eLow, gl::DebugMessageTypeFlags::eAll);
    gl::debug::insert_message("OpenGL debug messages are active!", gl::DebugMessageSeverity::eLow);
#endif

    ImGui::Init(window, info.color_mode == AppliationColorMode::eDark);
    
    // Initialize major application components on startup
    detail::init_parser(scheduler);
    detail::init_state(scheduler, info);
    detail::init_spectral_grid(scheduler, info);
    detail::init_schedule(scheduler);

    // Main runtime loop
    while (!window.should_close()) { 
      scheduler.run();
    } 
    
    ImGui::Destr();
  }
} // namespace met