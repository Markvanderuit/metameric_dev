#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <small_gl/buffer.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <small_gl/window.hpp>
#include <metameric/gui/application.hpp>
#include <metameric/gui/detail/imgui.hpp>
#include <metameric/gui/detail/view.hpp>

namespace met {
  template <typename T>
  gl::Array2i to_array(T t) { return { static_cast<int>(t[0]), static_cast<int>(t[1]) }; }

  template <typename T>
  ImVec2 to_imvec2(T t) { return { static_cast<float>(t[0]), static_cast<float>(t[1]) }; }

  template <typename T, typename Array>
  std::span<T> to_span(const Array &v) { return std::span<T>((T *) v.data(), v.rows() * v.cols()); }

  struct TestButtonTask : public AbstractTask {
    TestButtonTask(const std::string &name) : AbstractTask(name) { }

    void create(CreateTaskInfo &info) override {
      // ...
    }

    void run(RuntimeTaskInfo &info) {
      ImGui::Begin("Floating window");
      if (ImGui::Button("Close me, dammit!")) {
        info.remove_task(name());
      }
      ImGui::End();
    }
  };

  ApplicationScheduler create_scheduler() {
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

    std::exit(0);

    ApplicationScheduler scheduler;
      
    scheduler.insert_task(LambdaTask( "imgui_demo_task",
    [](auto &) { }, [](auto &) {
      ImGui::ShowDemoWindow();
    }));
    
    scheduler.insert_task(LambdaTask("imgui_dockable_task_0", 
    [](auto &) { }, [](auto &) {
      ImGui::Begin("Dockable 1");
      ImGui::LabelText("Label", "Text %i", 16);
      ImGui::LabelText("Label", "Text %i", 32);
      ImGui::LabelText("Label", "Text %i", 64);
      ImGui::End();
    }));
    
    scheduler.insert_task(LambdaTask("other_task", 
    [](auto &info) { 
      // Declare created objects
      info.emplace_resource<int>("integer", 5);
      info.emplace_resource<bool>("boolean", true);
    }, [](auto &info) {
      ImGui::Begin("Dockable 2");
      ImGui::LabelText("Label", "Text %i", 16);
      ImGui::LabelText("Label", "Text %i", 32);
      ImGui::LabelText("Label", "Text %i", 64);

      if (ImGui::Button("Test button, please ignore")) {
        info.insert_task_after<TestButtonTask>("other_task", "Spawned button");
      }

      ImGui::End();
    }));
    
    scheduler.insert_task(LambdaTask("imgui_resizable_texture", 
    [](auto &info) { 
      // Initialize a simple texture to pink
      gl::Texture<float, 2, 3> texture({ .size = { 128, 128 } });
      texture.clear(to_span<float, gl::Array3f>({ 255.f, 0.f, 255.f }));

      // Declare created objects
      auto texture_create = info.emplace_resource<gl::Texture<float, 2, 3>>(
        "texture", std::move(texture)
      );

      // Declare inputs
      auto integer_read = info.read_resource("other_task", "integer");
      auto boolean_read = info.read_resource("other_task", "boolean");

      // Declare outputs
      info.write_resource(integer_read, "written integer");
      info.write_resource(texture_create, "written texture");
    }, [](auto &info) {
      // Obtain shared objects
      auto &texture = info.get_resource<gl::Texture<float, 2, 3>>("texture");

      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4,4));
      ImGui::Begin("Main window", nullptr, ImGuiWindowFlags_NoTitleBar);
      
      // Resize texture if necessary
      if (auto window_size = to_array(ImGui::GetWindowSize()); 
          (texture.size() != (window_size - 8)).any()) {
        gl::Array3f texture_clear_value = { 255.f, 0.f, 255.f };
        texture = gl::Texture<float, 2, 3>({ .size = (window_size) - 8 });
        texture.clear(to_span<float, gl::Array3f>({ 255.f, 0.f, 255.f }));
      }

      // Pass texture to ImGUi
      ImGui::Image((void *) (size_t) texture.object(), to_imvec2(texture.size()) );

      ImGui::End();
      ImGui::PopStyleVar();
    }));

    scheduler.insert_task(LambdaTask("clear_default_framebuffer",
    [](auto &) {}, [](auto &) {
      gl::Framebuffer framebuffer = gl::Framebuffer::make_default();
      framebuffer.bind();
      framebuffer.clear<gl::Vector4f>(gl::FramebufferType::eColor);
      framebuffer.clear<float>(gl::FramebufferType::eDepth);
    }));

    scheduler.compile();
    return scheduler;
  }

  void create_application(ApplicationCreateInfo info) {
    // Initialize OpenGL context and primary window
    gl::WindowFlags window_flags = gl::WindowFlags::eVisible
    #ifndef NDEBUG                    
                                 | gl::WindowFlags::eDebug 
    #endif
                                 | gl::WindowFlags::eFocused   
                                 | gl::WindowFlags::eDecorated
                                 | gl::WindowFlags::eResizable
                                 | gl::WindowFlags::eSRGB;
    gl::Window window({.size = { 1280, 800 }, .title = "Metameric", .flags = window_flags });
    window.attach_context();

    // Enable OpenGL debug messages at default settings
#ifndef NDEBUG
    gl::debug::enable_messages(gl::DebugMessageSeverity::eLow, gl::DebugMessageTypeFlags::eAll);
    gl::debug::insert_message("OpenGL debug messages are active!", gl::DebugMessageSeverity::eLow);
#endif

    // Set up ImGui support in a subfunction
    ImGui::Init(window);

    // Set up application tasks in a subfunction
    ApplicationScheduler scheduler = create_scheduler();

    // Begin program loop
    while (!window.should_close()) {
      // Begin frame
      window.poll_events();
      ImGui::BeginFrame();
      
      // Scheduler handles all application tasks
      scheduler.run();
      scheduler.output_schedule();

      // End frame
      ImGui::DrawFrame();
      window.swap_buffers();
    } 
    
    ImGui::Destroy();
  }
} // namespace met