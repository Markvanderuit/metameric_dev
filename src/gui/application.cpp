#include <imgui.h>
#include <small_gl/buffer.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <small_gl/window.hpp>
#include <metameric/gui/application.hpp>
#include <metameric/gui/detail/imgui.hpp>
#include <metameric/gui/detail/view.hpp>
#include <metameric/gui/detail/linear_scheduler/scheduler.hpp>
#include <metameric/gui/task/lambda_task.hpp>
#include <metameric/gui/task/viewport_task.hpp>
#include <iostream>

namespace met {
  template <typename T>
  gl::Array2i to_array(T t) { return { static_cast<int>(t[0]), static_cast<int>(t[1]) }; }

  template <typename T>
  ImVec2 to_imvec2(T t) { return { static_cast<float>(t[0]), static_cast<float>(t[1]) }; }

  template <typename T, typename Array>
  std::span<T> to_span(const Array &v) { return std::span<T>((T *) v.data(), v.rows() * v.cols()); }

  void graph_example() {
    detail::DirectedGraph graph;

    /* map-based approach */
    using Node = detail::DirectedGraphNode<std::string>;
    using Edge = detail::DirectedGraphEdge<std::string>;

    graph.create_node<Node>("node_0");
    graph.create_node<Node>("node_1");
    graph.create_node<Node>("node_2");
    graph.create_node<Node>("node_3");

    graph.create_edge<Edge>("node_0", "node_1");
    graph.create_edge<Edge>("node_1", "node_3");
    graph.create_edge<Edge>("node_0", "node_3");
    graph.create_edge<Edge>("node_3", "node_2");
    
    graph.compile();
    graph.traverse();
  }

  detail::LinearScheduler create_initial_schedule() {
    detail::LinearScheduler scheduler;

    // Create task to instantiate main menu bar
    scheduler.emplace_task<LambdaFunctionTask>("main_menu_bar", [](auto &info) {
      guard(ImGui::BeginMainMenuBar());

      if (ImGui::BeginMenu("File")) {
        // ...
        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Edit")) {
        // ...
        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("View")) {
        // ...
        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Help")) {
        // ...
        ImGui::EndMenu();
      }

      ImGui::EndMainMenuBar();
    });

    // Create task to insert the primary dock space 
    scheduler.emplace_task<LambdaFunctionTask>("primary_dock_space", [](auto &info) {
      auto flags = ImGuiDockNodeFlags_AutoHideTabBar
                 | ImGuiDockNodeFlags_PassthruCentralNode;
      ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), flags);
    });

    // Create application tasks
    scheduler.emplace_task<ViewportTask>("viewport");
    scheduler.emplace_task<LambdaFunctionTask>("imgui_demo", [](auto &) { ImGui::ShowDemoWindow(); });

    // Create task to wipe default framebuffer all the way at the end
    scheduler.emplace_task<LambdaFunctionTask>("clear_default_framebuffer", [](auto &) {
      gl::Framebuffer framebuffer = gl::Framebuffer::make_default();
      framebuffer.bind();
      framebuffer.clear<gl::Vector4f>(gl::FramebufferType::eColor);
      framebuffer.clear<float>(gl::FramebufferType::eDepth);
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
    auto scheduler = create_initial_schedule();
    while (!window.should_close()) {
      // Begin frame
      window.poll_events();
      ImGui::BeginFrame();
      
      // Scheduler handles all tasks inside window
      scheduler.run();

      // End frame
      ImGui::DrawFrame();
      window.swap_buffers();
    } 
    
    ImGui::Destroy();
  }
} // namespace met