#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <small_gl/buffer.hpp>
#include <small_gl/exception.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <small_gl/window.hpp>
#include <metameric/gui/application.hpp>
#include <metameric/gui/detail/imgui.hpp>

gl::WindowFlags main_window_flags = gl::WindowFlags::eVisible   
                                  | gl::WindowFlags::eSRGB      
                                  | gl::WindowFlags::eDecorated
                                  | gl::WindowFlags::eFocused
                                  | gl::WindowFlags::eResizable;

namespace met {
  template <typename T>
  gl::Array2i to_array(T t) { return { static_cast<int>(t[0]), static_cast<int>(t[1]) }; }

  template <typename T>
  ImVec2 to_imvec2(T t) { return { static_cast<float>(t[0]), static_cast<float>(t[1]) }; }

  template <typename T, typename Array>
  std::span<T> to_span(const Array &v) { return std::span<T>((T *) v.data(), v.rows() * v.cols()); }

  void create_application(ApplicationCreateInfo info) {
    // Initialize OpenGL context and primary window
    gl::WindowFlags flags = main_window_flags;
#ifndef NDEBUG
    flags |= gl::WindowFlags::eDebug;
#endif
    gl::Window window({.size = { 1024, 768 }, .title = "Metameric", .flags = flags });

    window.attach_context();

#ifndef NDEBUG
    gl::enable_debug_callbacks();
#endif

    // Cause an error intentionally
    std::vector<gl::uint> false_data = { 1 };
    gl::Buffer false_buffer({
      .size = sizeof(gl::uint),
      .data = std::as_bytes(std::span(false_data))
    });

    gl::debug::begin_group("omg", false_buffer);

    gl::debug::end_group();

    ImGui::Init(window);

    { /* Initialize context-dependent objects _inside_ this scope */
      gl::Framebuffer default_framebuffer = gl::Framebuffer::make_default();

      gl::Texture<float, 2, 3> texture({ .size = { 128, 128 } });
      texture.clear(to_span<float, gl::Array3f>({ 255.f, 0.f, 255.f }));

      // Begin primary render loop
      while (!window.should_close()) {
        window.poll_events();
        ImGui::BeginFrame();
        
        { /* Begin render loop scope */

          // Draw a texture filling an ImGui window
          {
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
          }

          gl::gl_check();
        } /* End render loop scope */
        
        // Wipe framebuffer to black
        default_framebuffer.bind();
        default_framebuffer.clear<gl::Vector4f>(gl::FramebufferType::eColor);
        default_framebuffer.clear<float>(gl::FramebufferType::eDepth);

        ImGui::DrawFrame();
        window.swap_buffers();
      } 
    } /* Context-dependent objects are destroyed _beyond_ this scope */
    
    ImGui::Destroy();
  }
} // namespace met