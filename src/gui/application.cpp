#include <metameric/core/io.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/glm.hpp>
#include <metameric/gui/detail/imgui.hpp>
#include <metameric/gui/detail/scheduler.hpp>
#include <metameric/gui/task/lambda_task.hpp>
#include <metameric/gui/task/mapping_task.hpp>
#include <metameric/gui/task/gamut_picker.hpp>
#include <metameric/gui/task/image_viewer.hpp>
#include <metameric/gui/task/viewport_task.hpp>
#include <metameric/gui/task/window_task.hpp>
#include <metameric/gui/application.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/utility.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/window.hpp>
#include <algorithm>
#include <execution>
#include <mutex>
#include <ranges>

namespace met {
  namespace detail {
    void init_color_texture(detail::LinearScheduler &scheduler, ApplicationCreateInfo info) {
      // Load texture from disk
      auto texture_data = io::load_texture_float(info.texture_path);
      io::apply_channel_conversion(texture_data, 4, 1.f);
      io::apply_srgb_to_lrgb(texture_data, true);
      fmt::print("Loaded startup texture\n\tpath: {}\n\tdims: {}x{}\n", 
        info.texture_path.string(), texture_data.size.x, texture_data.size.y);

      // Load data into buffer and pass to scheduler
      scheduler.emplace_resource<gl::Buffer, gl::BufferCreateInfo>("color_texture_buffer_gpu", {
        .data = as_typed_span<std::byte>(texture_data.data)
      });
      scheduler.insert_resource("color_texture_buffer_cpu", std::move(texture_data));
    }

    void init_spectral_gamut(detail::LinearScheduler &scheduler) {
      constexpr std::array<float, 12> gamut_initial_vertices = {
        0.2f, 0.2f, 0.2f, 0.5f, 0.2f, 0.2f,
        0.5f, 0.5f, 0.2f, 0.3f, 0.3f, 0.7f
      };
      constexpr auto create_flags
        = gl::BufferCreateFlags::eMapRead | gl::BufferCreateFlags::eMapWrite 
        | gl::BufferCreateFlags::eMapPersistent | gl::BufferCreateFlags::eMapCoherent;
      constexpr auto map_flags 
        = gl::BufferAccessFlags::eMapRead | gl::BufferAccessFlags::eMapWrite
        | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapCoherent;
    
      // Load data into buffer, generate a persistent mapping, and pass both to scheduler
      auto &buff = scheduler.emplace_resource<gl::Buffer, gl::BufferCreateInfo>(
        "color_gamut_buffer", 
        { .data = as_typed_span<const std::byte>(gamut_initial_vertices), .flags = create_flags }
      );
      scheduler.insert_resource("color_gamut_map", convert_span<Color>(buff.map(map_flags)));
    }

    void init_spectral_grid(detail::LinearScheduler &scheduler, ApplicationCreateInfo info) {
      // Load input data 
      auto spectral_data = io::load_spectral_data_hd5(info.spectral_db_path);

      // Input data layout
      const uint  data_samples = spectral_data.channels;
      const float data_minv    = 400.f,
                  data_maxv    = 700.f,
                  data_ssize   = (data_maxv - data_minv) / static_cast<float>(data_samples);
      auto idx_to_data = [&](uint i) { return (.5f + static_cast<float>(i)) * data_ssize + data_minv; };

      // Fill list of wavelengths matching data layout for spectrum_from_data(...)
      std::vector<float> wavelengths(data_samples);
      std::ranges::copy(std::views::iota(0u, data_samples) 
        | std::views::transform(idx_to_data), wavelengths.begin());

      // Convert data into metameric's spectral format
      std::vector<Spec> internal_sd(spectral_data.size);
      std::transform(std::execution::par_unseq, spectral_data.data.begin(), spectral_data.data.end(), 
        internal_sd.begin(), [&](const auto &v) {  return spectrum_from_data(wavelengths, v); });

      // Convert data to metameric's color format under D65
      std::vector<ColorAlpha> internal_color(internal_sd.size());
      std::transform(std::execution::par_unseq, internal_sd.begin(), internal_sd.end(), internal_color.begin(), 
      [&](const auto &sd) { return (ColorAlpha() << xyz_to_srgb(reflectance_to_xyz(sd)), 1.0).finished(); });

      // Initialize a empty 3D spectral grid
      constexpr uint grid_size = 64;
      std::vector<Spec> grid(std::pow(grid_size, 3), 0.f);
      constexpr auto color_to_grid = [&](const ColorAlpha &c) {
        auto v = c.head(3).min(1.f).max(0.f) * static_cast<float>(grid_size - 1);
        return v.cast<uint>().eval();
      };

      // Initialize helper grids for parallel computation
      std::vector<std::mutex> grid_mutexes(std::pow(grid_size, 3));
      std::vector<uint>       grid_count(std::pow(grid_size, 3));

      // Reduce spectra into grid based on their color as a position
      std::vector<uint> grid_indices(internal_color.size());
      std::iota(grid_indices.begin(), grid_indices.end(), 0);
      std::for_each(std::execution::par, grid_indices.begin(), grid_indices.end(), [&](uint i) {
        auto gridv = color_to_grid(internal_color[i]).eval();
        uint j     = gridv.z() * std::pow(grid_size, 2) 
                   + gridv.y() * grid_size 
                   + gridv.x();
        
        // Acquire a lock on the current position to ensure reduction remains atomic
        std::lock_guard<std::mutex> lock(grid_mutexes[j]);

        grid[j] += internal_sd[i];
        grid_count[j]++;
      });
      
      // Normalize grid by number of spectra added per bin
      grid_indices = std::vector<uint>(grid.size());
      std::iota(grid_indices.begin(), grid_indices.end(), 0);
      std::for_each(std::execution::par_unseq, grid_indices.begin(), grid_indices.end(), [&](uint i) {
        grid[i] /= static_cast<float>(std::max(grid_count[i], 1u));
      });

      fmt::print("Loaded spectral grid\n\tdims: {}x{}x{}\n",
        grid_size, grid_size, grid_size);

      // Make resources available for other components during runtime
      scheduler.insert_resource<std::vector<Spec>>("spectral_data", std::move(internal_sd));
      scheduler.insert_resource<std::vector<Spec>>("spectral_grid", std::move(grid));
      scheduler.insert_resource<std::vector<ColorAlpha>>("color_data", std::move(internal_color));
    }

    void init_schedule(detail::LinearScheduler &scheduler, gl::Window &window) {
      // First task to run prepares for a new frame
      scheduler.emplace_task<LambdaTask>("frame_begin", [&] (auto &) {
        window.poll_events();
        ImGui::BeginFrame();
      });

      // Third task to run prepares imgui's viewport layout
      scheduler.emplace_task<WindowTask>("viewport_base");

      // Next tasks to run define ui components and runtime tasks
      scheduler.emplace_task<GamutPickerTask>("gamut_picker");
      scheduler.emplace_task<MappingTask>("mapping");
      scheduler.emplace_task<ViewportTask>("viewport");
      scheduler.emplace_task<ImageViewerTask>("image_viewer");

      // Next tasks to run are temporary testing tasks
      scheduler.emplace_task<LambdaTask>("imgui_demo", [](auto &) {  ImGui::ShowDemoWindow(); });
      scheduler.emplace_task<LambdaTask>("imgui_metrics", [](auto &) { ImGui::ShowMetricsWindow(); });
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
          auto viewport_size = static_cast<glm::vec2>(ImGui::GetWindowContentRegionMax())
                            - static_cast<glm::vec2>(ImGui::GetWindowContentRegionMin());

          ImGui::PlotLines("Emitter, d65", models::emitter_cie_d65.data(), wavelength_samples, 0,
            nullptr, FLT_MAX, FLT_MAX, viewport_size * glm::vec2(.67f, 0.3f));
          ImGui::PlotLines("Emitter, e", models::emitter_cie_e.data(), wavelength_samples, 0,
            nullptr, FLT_MAX, FLT_MAX, viewport_size * glm::vec2(.67f, 0.3f));
          ImGui::PlotLines("CIE XYZ, x()", models::cmfs_cie_xyz.row(0).eval().data(), wavelength_samples, 0,
            nullptr, FLT_MAX, FLT_MAX, viewport_size * glm::vec2(.67f, 0.3f));
          ImGui::PlotLines("CIE XYZ, y()", models::cmfs_cie_xyz.row(1).eval().data(), wavelength_samples, 0,
            nullptr, FLT_MAX, FLT_MAX, viewport_size * glm::vec2(.67f, 0.3f));
          ImGui::PlotLines("CIE XYZ, z()", models::cmfs_cie_xyz.row(2).eval().data(), wavelength_samples, 0,
            nullptr, FLT_MAX, FLT_MAX, viewport_size * glm::vec2(.67f, 0.3f));

          Color rgb = xyz_to_srgb(reflectance_to_xyz(1.f));
          ImGui::PlotLines("Color, rgb", rgb.data(), 3);
          ImGui::ColorEdit3("Test color", rgb.data());
        }
        ImGui::End();
      });
      
      scheduler.emplace_task<LambdaTask>("plot_spectra", [](auto &info) {
        if (ImGui::Begin("Reflectance plots")) {
          const auto &spectra = info.get_resource<std::vector<Spec>>("global", "spectral_data");
          const auto viewport_size = static_cast<glm::vec2>(ImGui::GetWindowContentRegionMax())
                                  - static_cast<glm::vec2>(ImGui::GetWindowContentRegionMin());
          
          ImGui::PlotLines("reflectance 0", spectra[0].data(), wavelength_samples, 0,
            nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.2f));
          Color rgb_0 = xyz_to_srgb(reflectance_to_xyz(spectra[0]));
          ImGui::ColorEdit3("Test color ", rgb_0.data());

          ImGui::PlotLines("reflectance 1", spectra[1].data(), wavelength_samples, 0,
            nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.2f));
          Color rgb_1 = xyz_to_srgb(reflectance_to_xyz(spectra[1]));
          ImGui::ColorEdit3("Test color 1", rgb_1.data());

          ImGui::PlotLines("reflectance 2", spectra[2].data(), wavelength_samples, 0,
            nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.2f));
          Color rgb_2 = xyz_to_srgb(reflectance_to_xyz(spectra[2]));
          ImGui::ColorEdit3("Test color 2", rgb_2.data());

          ImGui::PlotLines("reflectance 3", spectra[3].data(), wavelength_samples, 0,
            nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.2f));
          Color rgb_3 = xyz_to_srgb(reflectance_to_xyz(spectra[3]));
          ImGui::ColorEdit3("Test color 3", rgb_3.data());
        }
        ImGui::End();
      });

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

    detail::LinearScheduler scheduler;

    // Add copy of application create info to scheduler resources for later access
    scheduler.insert_resource("application_create_info", ApplicationCreateInfo(info));

    // Initialize OpenGL context and primary window, submit to scheduler resourcess
    auto &window = scheduler.emplace_resource<gl::Window, gl::WindowCreateInfo>("window", 
      { .size = { 1280, 800 }, .title = "Metameric", .flags = window_flags });

    // Enable OpenGL debug messages, ignoring notification-type messages
#ifndef NDEBUG
    gl::debug::enable_messages(gl::DebugMessageSeverity::eLow, gl::DebugMessageTypeFlags::eAll);
    gl::debug::insert_message("OpenGL debug messages are active!", gl::DebugMessageSeverity::eLow);
#endif
    
    // Initialize ImGui; non-scoped so destroy it later
    ImGui::Init(window, info);

    // Initialize major application components and set up runtime schedule
    detail::init_color_texture(scheduler, info);
    detail::init_spectral_grid(scheduler, info);
    detail::init_spectral_gamut(scheduler);
    detail::init_schedule(scheduler, window);
    
    // Main runtime loop
    while (!window.should_close()) { 
      scheduler.run(); 
    } 
    
    ImGui::Destr();
  }
} // namespace met