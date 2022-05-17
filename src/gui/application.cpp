#include <small_gl/framebuffer.hpp>
#include <small_gl/utility.hpp>
#include <small_gl/window.hpp>
#include <metameric/gui/detail/imgui.hpp>
#include <metameric/gui/detail/linear_scheduler/scheduler.hpp>
#include <metameric/gui/task/lambda_task.hpp>
#include <metameric/gui/task/viewport_base_task.hpp>
#include <metameric/gui/task/viewport_task.hpp>
#include <metameric/gui/application.hpp>

namespace met {
  detail::LinearScheduler init_scheduler(gl::Window &window) {
    detail::LinearScheduler scheduler;

    // First task to run prepares for a new frame
    scheduler.emplace_task<LambdaTask>("frame_begin", [&] (auto &) {
      window.poll_events();
      ImGui::BeginFrame();
    });

    // Next task to run prepares imgui's viewport
    scheduler.emplace_task<ViewportBaseTask>("viewport_base");

    // Next tasks to run define main program components 
    scheduler.emplace_task<ViewportTask>("viewport");
    scheduler.emplace_task<LambdaTask>("imgui_demo", [](auto &) { ImGui::ShowDemoWindow(); });

    // Last task to run ends a frame
    scheduler.emplace_task<LambdaTask>("frame_end", [&] (auto &) {
      auto fb = gl::Framebuffer::make_default();
      fb.bind();
      fb.clear<gl::Vector4f>(gl::FramebufferType::eColor);
      fb.clear<float>(gl::FramebufferType::eDepth);
      
      ImGui::DrawFrame();
      window.swap_buffers();
    });

    return scheduler;
  }

  void create_application(ApplicationCreateInfo info) {
    gl::WindowFlags window_flags = gl::WindowFlags::eVisible
    #ifndef NDEBUG                    
                                 | gl::WindowFlags::eDebug 
    #endif
                                 | gl::WindowFlags::eFocused   
                                 | gl::WindowFlags::eDecorated
                                 | gl::WindowFlags::eResizable
                                 | gl::WindowFlags::eSRGB;
                                 
    // Initialize OpenGL context, primary window, and ImGui
    gl::Window window({.size = { 1280, 800 }, 
                       .title = "Metameric", 
                       .flags = window_flags });
    ImGui::Init(window);

    // Enable OpenGL debug messages, ignoring notification-type messages
#ifndef NDEBUG
    gl::debug::enable_messages(gl::DebugMessageSeverity::eLow, gl::DebugMessageTypeFlags::eAll);
    gl::debug::insert_message("OpenGL debug messages are active!", gl::DebugMessageSeverity::eLow);
#endif

    // Program loop
    auto scheduler = init_scheduler(window);
    while (!window.should_close()) { scheduler.run(); } 
    
    ImGui::Destr();
  }
} // namespace met