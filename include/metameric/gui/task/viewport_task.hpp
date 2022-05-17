#pragma once

#include <array>
#include <imgui.h>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/texture.hpp>
#include <metameric/core/detail/eigen.hpp>
#include <metameric/gui/detail/imgui.hpp>
#include <metameric/gui/detail/linear_scheduler/task.hpp>

namespace met {
  class ViewportTask : public detail::AbstractTask {
    using Tex3f2d = gl::Texture<float, 2, 3>;
    using Tex3f2dInfo = gl::TextureCreateInfo<float, 2>;
    
    // Array draw components
    gl::Array    _texture_array;
    gl::Buffer   _texture_buffer;
    gl::Program  _texture_program;
    gl::DrawInfo _texture_draw;

    // Frame draw components
    gl::Framebuffer _fb;
    gl::Array3f     _fb_clear = { 255.f, 0.f, 255.f };
    Tex3f2d         _fb_texture;

  private:
    template <typename T, typename Array>
    std::span<T> to_span(const Array &v) { return std::span<T>((T *) v.data(), v.rows() * v.cols()); }

  public:
    ViewportTask(const std::string &name)
    : detail::AbstractTask(name) { }

    void init(detail::TaskInitInfo &info) override {

    }

    void eval(detail::TaskEvalInfo &info) override {
      auto scope_vars = { ImGui::ScopedStyleVar { ImGuiStyleVar_WindowRounding, 0.f }, 
                          ImGui::ScopedStyleVar { ImGuiStyleVar_WindowBorderSize, 0.f }, 
                          ImGui::ScopedStyleVar { ImGuiStyleVar_WindowPadding, { 0.f, 0.f } }};
                          
      // Begin window draw
      ImGui::Begin("Viewport", 0, ImGuiWindowFlags_NoDecoration);

      const auto viewport_size = ImGui::to_array(ImGui::GetWindowContentRegionMax())
                               - ImGui::to_array(ImGui::GetWindowContentRegionMin());

      // Resize or initialize framebuffer and texutre objects
      if (!_fb_texture.is_init() || (_fb_texture.size() != viewport_size).any()) {
        _fb_texture = Tex3f2d({ .size = viewport_size });
        _fb_texture.clear(to_span<float>(_fb_clear));

        _fb = gl::Framebuffer({
          .type = gl::FramebufferType::eColor,
          .texture = &_fb_texture
        });
      }

      // Draw texture to viewport
      ImGui::Image(ImGui::to_ptr(_fb_texture.object()), ImGui::to_imvec2(_fb_texture.size()));

      // End window draw
      ImGui::End();
    }
  };
} // namespace met