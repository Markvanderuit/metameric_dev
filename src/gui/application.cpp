#include <small_gl/framebuffer.hpp>
#include <small_gl/utility.hpp>
#include <small_gl/window.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/glm.hpp>
#include <metameric/gui/detail/imgui.hpp>
#include <metameric/gui/detail/scheduler.hpp>
#include <metameric/gui/task/lambda_task.hpp>
#include <metameric/gui/task/viewport_base_task.hpp>
#include <metameric/gui/task/viewport_task.hpp>
#include <metameric/gui/task/gamut_picker.hpp>
#include <metameric/gui/application.hpp>

namespace met {
  gl::WindowCreateFlags window_flags = gl::WindowCreateFlags::eVisible
  #ifndef NDEBUG                    
                                      | gl::WindowCreateFlags::eDebug 
  #endif
                                      | gl::WindowCreateFlags::eFocused   
                                      | gl::WindowCreateFlags::eDecorated
                                      | gl::WindowCreateFlags::eResizable
                                      | gl::WindowCreateFlags::eSRGB
                                      | gl::WindowCreateFlags::eMSAA;
                                      
  void init_schedule(detail::LinearScheduler &scheduler, gl::Window &window) {
    // First task to run prepares for a new frame
    scheduler.emplace_task<LambdaTask>("frame_begin", [&] (auto &) {
      window.poll_events();
      ImGui::BeginFrame();
    });

    // Third task to run prepares imgui's viewport layout
    scheduler.emplace_task<ViewportBaseTask>("viewport_base");

    // Next tasks to run define main viewport components and tasks
    scheduler.emplace_task<GamutPickerTask>("gamut_picker");
    scheduler.emplace_task<ViewportTask>("viewport");

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

  void create_application(ApplicationCreateInfo info) {
    detail::LinearScheduler scheduler;

    // Load initial texture data, strip gamma correction, submit to scheduler
    fmt::print("Loading startup texture: {}\n", info.texture_path.string());
    auto texture_data = io::load_texture_float(info.texture_path);
    io::apply_srgb_to_lrgb(texture_data, true);
    scheduler.insert_resource("texture_data", std::move(texture_data));
    scheduler.insert_resource("application_create_info", ApplicationCreateInfo(info));

    // Load test spectral database
    fmt::print("Loading test database: {}\n", info.spectral_db_path.string());
    auto spectral_data = io::load_spectral_data_hd5(info.spectral_db_path);

    // Initialize OpenGL context and primary window, submit to scheduler
    auto &window = scheduler.emplace_resource<gl::Window, gl::WindowCreateInfo>("window", 
      { .size = { 1280, 800 }, .title = "Metameric", .flags = window_flags });

    // Enable OpenGL debug messages, ignoring notification-type messages
#ifndef NDEBUG
    gl::debug::enable_messages(gl::DebugMessageSeverity::eLow, gl::DebugMessageTypeFlags::eAll);
    gl::debug::insert_message("OpenGL debug messages are active!", gl::DebugMessageSeverity::eLow);
#endif

    ImGui::Init(window, info);

    // Create and run loop
    init_schedule(scheduler, window);
    while (!window.should_close()) { 
      scheduler.run(); 
    } 
    
    ImGui::Destr();
  }
} // namespace met