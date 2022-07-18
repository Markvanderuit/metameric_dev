#include <metameric/core/io.hpp>
#include <metameric/core/knn.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/glm.hpp>

#include <metameric/components/lambda_task.hpp>
#include <metameric/components/tasks/generate_spectral_task.hpp>
#include <metameric/components/tasks/mapping_task.hpp>
#include <metameric/components/tasks/mapping_cpu_task.hpp>
#include <metameric/components/views/gamut_viewer.hpp>
#include <metameric/components/views/image_viewer.hpp>
#include <metameric/components/views/viewport_task.hpp>
#include <metameric/components/views/window_task.hpp>
#include <metameric/components/views/detail/imgui.hpp>

#include <metameric/app/application.hpp>

#include <small_gl/buffer.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/utility.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/window.hpp>

#include <algorithm>
#include <execution>
#include <ranges>

namespace met {
  namespace detail {
    void init_color_texture(detail::LinearScheduler &scheduler, ApplicationCreateInfo info) {
      // Load texture from disk
      auto texture_data = io::load_texture_color(info.texture_path);
      io::apply_srgb_to_lrgb(texture_data, true);
      fmt::print("Loaded startup texture\n\tpath: {}\n\tdims: {}x{}\n", 
        info.texture_path.string(), texture_data.size.x, texture_data.size.y);

      // Load aligned data into buffer and pass to scheduler
      std::vector<eig::AlArray3f> aligned_texture(texture_data.data.begin(), texture_data.data.end());
      scheduler.emplace_resource<gl::Buffer, gl::BufferCreateInfo>("color_texture_buffer_gpu", {
        .data = as_typed_span<std::byte>(aligned_texture)
      });
      scheduler.insert_resource("color_texture_buffer_cpu", std::move(texture_data));
    }

    void init_spectral_gamut(detail::LinearScheduler &scheduler) {
      // Define initial vertex positions; vec3 with 4 byte padding for alignment
      std::array<eig::AlArray3f, 4> gamut_initial_vertices = {
        eig::AlArray3f { .75f, .40f, .25f },
        eig::AlArray3f { .68f, .49f, .58f },
        eig::AlArray3f { .50f, .58f, .39f },
        eig::AlArray3f { .35f, .30f, .34f }
      };

      // Load data into buffer and pass to scheduler; enable read/write mapping if necessary
      scheduler.emplace_resource<gl::Buffer, gl::BufferCreateInfo>("color_gamut_buffer",  { 
        .data  = as_typed_span<const std::byte>(gamut_initial_vertices), 
        .flags = gl::BufferCreateFlags::eMapReadWrite
      });
    }

    void init_spectral_grid(detail::LinearScheduler &scheduler, ApplicationCreateInfo info) {
      // Load input data 
      auto spectral_data = io::load_spectral_data_hd5(info.spectral_db_path);

      // Input data layout
      const uint  data_samples = spectral_data.channels;
      const float data_minv    = 400.f,
                  data_maxv    = 710.f,
                  data_ssize   = (data_maxv - data_minv) / static_cast<float>(data_samples);
      auto idx_to_data = [&](uint i) { return (.5f + static_cast<float>(i)) * data_ssize + data_minv; };

      // Fill list of wavelengths matching data layout for spectrum_from_data(...)
      std::vector<float> wavelengths(data_samples);
      std::ranges::copy(std::views::iota(0u, data_samples) 
        | std::views::transform(idx_to_data), wavelengths.begin());
      
      // Convert data into metameric's spectral format
      std::vector<Spec> internal_sd(spectral_data.size);
      std::transform(std::execution::par_unseq, spectral_data.data.begin(), spectral_data.data.end(), 
        internal_sd.begin(), [&](const auto &v) {  return io::spectrum_from_data(wavelengths, v); });

      // Create a KNN grid over the spectral distributions,based on their color as a position
      fmt::print("Constructing KNN grid\n");
      std::vector<eig::Array3f> knn_positions(internal_sd.size());
      std::transform(std::execution::par_unseq, internal_sd.begin(), internal_sd.end(), knn_positions.begin(),
        [&](const Spec &sd) { return reflectance_to_color(sd, { .cmfs = models::cmfs_srgb }); });
      KNNGrid<Spec> knn_grid = {{ .grid_size = 32 }};
      knn_grid.insert_n(internal_sd, knn_positions);
      fmt::print("Constructed KNN grid\n");

      fmt::print("Constructing voxel grid\n");
      VoxelGrid<Spec> voxel_grid = {{ .grid_size = 16 }};

      // Construct list of voxel grid indices
      auto grid_view = std::views::iota(0, (int) voxel_grid.size())
                     | std::views::transform([&](int i) { return voxel_grid.grid_pos_from_index(i); });
      std::vector<eig::Array3i> grid_pos(voxel_grid.size());
      std::ranges::copy(grid_view, grid_pos.begin());

      // Fill voxel grid by querying KNN grid at each voxel center and averaging, for now
      constexpr auto kernel = [](float x) {
        constexpr float stddev = 1.f;
        constexpr float alpha = -1.f / (2.f * stddev * stddev);
        return std::max(0.f, std::exp(alpha * (x * x)));
      };
      std::for_each(std::execution::par_unseq, grid_pos.begin(), grid_pos.end(), [&](const auto &p_i) {
        auto p = voxel_grid.pos_from_grid_pos(p_i);
        auto query_list = knn_grid.query_k_nearest(p, 4);
        
        Spec value = 0.f;
        float weight = 0.f;
        for (auto &query : query_list) {
          float w = kernel(query.distance);
          value += w * query.value;
          weight += w;
        }
        voxel_grid.at(p_i) = value / std::max(weight, 0.00001f);
      });
      fmt::print("Constructed voxel grid\n");

      // Make resources available for other components during runtime
      scheduler.insert_resource("spectral_knn_grid",   std::move(knn_grid));
      scheduler.insert_resource("spectral_voxel_grid", std::move(voxel_grid));
    }

    void init_schedule_temp(detail::LinearScheduler &scheduler, gl::Window &window) {
      scheduler.emplace_task<LambdaTask>("imgui_demo", [](auto &) {  ImGui::ShowDemoWindow(); });
      scheduler.emplace_task<LambdaTask>("imgui_metrics", [](auto &) { ImGui::ShowMetricsWindow(); });

      // Temporary window to plot some timings
      scheduler.emplace_task<LambdaTask>("imgui_delta", [](auto &info) {
        if (ImGui::Begin("Imgui timings")) {
          auto &io = ImGui::GetIO();

          // Report frame times
          ImGui::LabelText("Frame delta, last", "%.3f ms (%.1f fps)", 1000.f * io.DeltaTime, 1.f / io.DeltaTime);
          ImGui::LabelText("Frame delta, average", "%.3f ms (%.1f fps)", 1000.f / io.Framerate, io.Framerate);
          
          // Report mouse pos
          const auto &window = info.get_resource<gl::Window>("global", "window");
          const auto &input = window.input_info();
          
          glm::vec2 mouse_pos_2 = input.mouse_position;
          ImGui::LabelText("Mouse delta", "%.1f, %.1f", io.MouseDelta.x, io.MouseDelta.y);
          ImGui::LabelText("Mouse position", "%.1f, %.1f", io.MousePos.x, io.MousePos.y);
          ImGui::LabelText("Mouse position (glfw)", "%.1f, %.1f", mouse_pos_2.x, mouse_pos_2.y);
        }
        ImGui::End();
      });
      
      // Temporary window to plot some distributions
      scheduler.emplace_task<LambdaTask>("plot_models", [](auto &) {
        if (ImGui::Begin("Model plots")) {
          auto plot_size = (static_cast<glm::vec2>(ImGui::GetWindowContentRegionMax())
                          - static_cast<glm::vec2>(ImGui::GetWindowContentRegionMin())) * glm::vec2(.67f, 0.3f);
          ImGui::PlotLines("Emitter, d65", models::emitter_cie_d65.data(), 
            wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
          ImGui::PlotLines("Emitter, fl11", models::emitter_cie_fl11.data(), 
            wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
          ImGui::PlotLines("Emitter, ledb1", models::emitter_cie_ledb1.data(), 
            wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
          ImGui::PlotLines("Emitter, ledrgb1", models::emitter_cie_ledrgb1.data(), 
            wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
          ImGui::PlotLines("CIE XYZ, x()", models::cmfs_cie_xyz.col(0).data(), 
            wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
          ImGui::PlotLines("CIE XYZ, y()", models::cmfs_cie_xyz.col(1).data(), 
            wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
          ImGui::PlotLines("CIE XYZ, z()", models::cmfs_cie_xyz.col(2).data(), 
            wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
        }
        ImGui::End();
      });
    }

    void init_schedule(detail::LinearScheduler &scheduler, gl::Window &window) {
      // First task to run prepares for a new frame
      scheduler.emplace_task<LambdaTask>("frame_begin", [&] (auto &) {
        window.poll_events();
        ImGui::BeginFrame();
      });

      // Third task to run prepares imgui's viewport layout
      scheduler.emplace_task<WindowTask>("window");

      // The following tasks define the uplifting pipeline
      scheduler.emplace_task<GenerateSpectralTask>("generate_spectral");
      scheduler.emplace_task<MappingTask>("mapping");
      scheduler.emplace_task<MappingCPUTask>("mapping_cpu");

      // The following tasks define UI components and windows
      scheduler.emplace_task<ViewportTask>("viewport");
      scheduler.emplace_task<GamutViewerTask>("gamut_viewer");
      scheduler.emplace_task<ImageViewerTask>("image_viewer");

      // Insert temporary unimportant tasks
      init_schedule_temp(scheduler, window);

      // Final task to run ends a frame
      scheduler.emplace_task<LambdaTask>("frame_end", [&] (auto &) {
        auto fb = gl::Framebuffer::make_default();
        fb.bind();
        fb.clear<glm::vec3>(gl::FramebufferType::eColor);
        fb.clear<float>(gl::FramebufferType::eDepth);
        ImGui::DrawFrame();
        window.swap_buffers();
      });
    }
  }

  gl::WindowCreateFlags window_flags = gl::WindowCreateFlags::eVisible
  #ifndef NDEBUG                    
                                      | gl::WindowCreateFlags::eDebug 
  #endif
                                      | gl::WindowCreateFlags::eFocused   
                                      | gl::WindowCreateFlags::eDecorated
                                      | gl::WindowCreateFlags::eResizable
                                      | gl::WindowCreateFlags::eSRGB
                                      | gl::WindowCreateFlags::eMSAA;
                                      

  void create_application(ApplicationCreateInfo info) {
     fmt::print("Metameric format\n\tmin : {}nm\n\tmax : {}nm\n\tbins: {}nm\n",
      wavelength_min, wavelength_max, wavelength_samples);

    // Add copy of application create info to scheduler's resources for later access
    detail::LinearScheduler scheduler;
    scheduler.insert_resource("application_create_info", ApplicationCreateInfo(info));

    // Initialize OpenGL context and primary window, submit to scheduler resourcess
    auto &window = scheduler.emplace_resource<gl::Window, gl::WindowCreateInfo>("window", 
      { .size = { 1280, 800 }, .title = "Metameric", .flags = window_flags });

    // Enable OpenGL debug messages, ignoring notification-type messages
#ifndef NDEBUG
    gl::debug::enable_messages(gl::DebugMessageSeverity::eLow, gl::DebugMessageTypeFlags::eAll);
    gl::debug::insert_message("OpenGL debug messages are active!", gl::DebugMessageSeverity::eLow);
#endif

    // Initialize major application components and set up runtime schedule
    ImGui::Init(window, info.color_mode == AppliationColorMode::eDark);
    detail::init_color_texture(scheduler, info);
    detail::init_spectral_grid(scheduler, info);
    detail::init_spectral_gamut(scheduler);
    detail::init_schedule(scheduler, window);
    
    // Output task order
    for (const auto &task : scheduler.tasks()) {
      fmt::print("{}\n", task->name());
    }

    // Main runtime loop
    while (!window.should_close()) { 
      scheduler.run(); 
    } 
    
    ImGui::Destr();
  }
} // namespace met