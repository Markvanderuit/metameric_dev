#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <small_gl/exception.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <small_gl/window.hpp>
#include <metameric/gui/application.hpp>
#include <iostream>

namespace met {
  template <typename T>
  gl::Array2i to_array(T t) { return { static_cast<int>(t[0]), static_cast<int>(t[1]) }; }

  template <typename T>
  ImVec2 to_imvec2(T t) { return { static_cast<float>(t[0]), static_cast<float>(t[1]) }; }

  void create_application(ApplicationCreateInfo info) {
    // Specify window hint flags
    gl::WindowFlags flags = gl::WindowFlags::eVisible   | gl::WindowFlags::eDecorated
                          | gl::WindowFlags::eSRGB      | gl::WindowFlags::eFocused
                          | gl::WindowFlags::eResizable;
#ifdef NDEBUG
    flags |= gl::WindowFlags::eDebug;
#endif

    // Initialize OpenGL context and primary window
    gl::Window window({.size = { 1024, 768 }, .title = "Metameric", .flags = flags });
    window.attach_context();

    // Initialize ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsLight();

    // Initialize ImGui platform specific bindings
    ImGui_ImplGlfw_InitForOpenGL((GLFWwindow *) window.object(), true);
    ImGui_ImplOpenGL3_Init();

    { /* Initialize context-dependent objects _inside_ this scope */
      gl::Framebuffer default_framebuffer = gl::Framebuffer::make_default();

      gl::Texture<float, 2, 3> texture({ .size = { 128, 128 } });
      gl::Array3f texture_clear_value = { 255.f, 0.f, 255.f };
      texture.clear(std::span<float>{ texture_clear_value.data(), 3 });

      // Begin primary render loop
      while (!window.should_close()) {
        window.poll_events();

        // Start new frame for IMGUI
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        { /* Begin render loop scope */
          // Wipe framebuffer to black
          default_framebuffer.clear<gl::Vector4f>(gl::FramebufferType::eColor);

          { // Draw a texture filling an ImGui window
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4,4));
            ImGui::SetNextWindowSize(ImVec2 { 256, 256 });
            ImGui::Begin("Main window", nullptr, ImGuiWindowFlags_NoTitleBar);

            // Resize texture if necessary
            auto window_size = to_array(ImGui::GetWindowSize());
            if ((texture.size() != (window_size - 8)).any()) {
              gl::Array3f texture_clear_value = { 255.f, 0.f, 255.f };
              texture = gl::Texture<float, 2, 3>({ .size = (window_size) - 8 });
              texture.clear(std::span<float>{ texture_clear_value.data(), 3 });
            }
            
            // Pass texture to ImGUi
            ImGui::Image((void *) (size_t) texture.object(), to_imvec2(texture.size()) );

            ImGui::End();
            ImGui::PopStyleVar();
          }

          gl::gl_check();
        } /* End render loop scope */

        
        // Render ImGui components to default framebuffer
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        window.swap_buffers();
      } 
    } /* Context-dependent objects are destroyed _beyond_ this scope */
    
    // Destroy ImGui
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();
  }
} // namespace met