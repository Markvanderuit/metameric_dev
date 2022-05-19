#pragma once

#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/texture.hpp>
#include <metameric/core/detail/glm.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/gui/detail/imgui.hpp>
#include <metameric/gui/detail/linear_scheduler/task.hpp>
#include <glm/glm.hpp>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <iostream>

namespace met {
  namespace detail {
    template <class T, class C>
    std::span<T> as_typed_span(C &c) {
      auto data = c.data();
      guard(data, {});
      return { reinterpret_cast<T&>(data), (c.size() * sizeof(C::value_type)) / sizeof(T) };
    }

    template <class T, class U>
    std::span<T> convert_span(std::span<U> s) {
      auto data = s.data();
      guard(data, {});
      return { reinterpret_cast<T*>(data), s.size_bytes() / sizeof(T) };
    }
    
    template <typename T, typename Array>
    std::span<T> to_span(const Array &v) { return std::span<T>((T *) v.data(), v.rows() * v.cols()); }
  }

  class ViewportTask : public detail::AbstractTask {
    using Tex3f2d = gl::Texture<float, 2, 3>;
    using Tex3f2dInfo = gl::TextureCreateInfo<float, 2>;
    
    // Array draw components
    gl::Array       _texture_array;
    gl::Buffer      _texture_buffer; 
    gl::DrawInfo    _texture_draw;
    gl::Program     _texture_program;

    // Camera components
    gl::Matrix4f    _model_view_matrix;
    gl::Matrix4f    _projection_matrix;

    // Frame draw components
    gl::Framebuffer _fb;
    gl::Array3f     _fb_clear = { 255.f, 0.f, 255.f };
    Tex3f2d         _fb_texture;

    // Testing components
    Tex3f2d         _temp_texture;

  private:

  public:
    ViewportTask(const std::string &name)
    : detail::AbstractTask(name) { }

    template <typename uint D>
    void test_func(glm::vec<D, float> v) {

    }

    void init(detail::TaskInitInfo &info) override {
      // Obtain handle to loaded texture data
      auto &texture_obj = info.get_resource<io::TextureData<float>>("global", "texture_data");
      auto texture_data = std::span(texture_obj.data);

      // Load texture data into buffer object
      _texture_buffer = gl::Buffer({ .data = detail::convert_span<std::byte>(texture_data) });

      // Assemble vertex array around buffer object
      _texture_array = gl::Array({ .buffers = {{ .buffer = &_texture_buffer, 
                                                 .binding = 0,
                                                 .stride  = sizeof(Vector3f) }},
                                   .attributes = {{ .attribute_binding = 0,
                                                    .buffer_binding = 0,
                                                    .format_size = gl::VertexFormatSize::e3 }}});

      _model_view_matrix = math::lookat_matrix({ 1.f, 0.f, 2.f },
                                               { 1.f, 0.f, 0.f },
                                               { 0.f, 1.f, 0.f });
      _projection_matrix = math::perspective_matrix(45.f, 1.f, 0.0001f, 1000.f);

      // Build shader program
      _texture_program = gl::Program({{ .type = gl::ShaderType::eVertex,
                                        .path = "../resources/shaders/texture_draw.vert.spv" },
                                      { .type = gl::ShaderType::eFragment,
                                        .path = "../resources/shaders/texture_draw.frag.spv" }});
      _texture_program.uniform("model_view_matrix", _model_view_matrix);
      _texture_program.uniform("projection_matrix", _projection_matrix);

      // Assemble draw object for render of vertex array
      _texture_draw = { .type = gl::PrimitiveType::ePoints,
                        .array = &_texture_array,
                        .vertex_count = (uint) texture_data.size(),
                        .program = &_texture_program };

      _temp_texture = Tex3f2d({ .size = texture_obj.size, .data = texture_obj.data });
    }

    void eval(detail::TaskEvalInfo &info) override {
      auto scope_vars = { ImGui::ScopedStyleVar { ImGuiStyleVar_WindowRounding, 0.f }, 
                          ImGui::ScopedStyleVar { ImGuiStyleVar_WindowBorderSize, 0.f }, 
                          ImGui::ScopedStyleVar { ImGuiStyleVar_WindowPadding, { 0.f, 0.f } }};
                          
      // Begin window draw
      ImGui::Begin("Viewport", 0, ImGuiWindowFlags_NoDecoration);

      // Compute texture viewport size based on window content region
      const auto viewport_size = static_cast<glm::ivec2>(ImGui::GetWindowContentRegionMax())
                               - static_cast<glm::ivec2>(ImGui::GetWindowContentRegionMin());

      // (Re)initialize framebuffer and texture objects on viewport resize
      if (!_fb_texture.is_init() || glm::any(_fb_texture.size() != viewport_size)) {
        _fb_texture = Tex3f2d({ .size = viewport_size });
        _fb = gl::Framebuffer({ .type = gl::FramebufferType::eColor, .texture = &_fb_texture });
      }

      gl::state::set_viewport(viewport_size);
      glPointSize(1.f);
      _fb.bind();
      _fb.clear<Vector3f>(gl::FramebufferType::eColor, Vector3f(0.f, 1.f, 0.f));
      _texture_program.bind();
      gl::state::set(gl::DrawCapability::eCullFace, false);
      gl::dispatch_draw(_texture_draw);

      // Draw texture to viewport
      ImGui::Image(ImGui::to_ptr(_fb_texture.object()), ImVec2(_fb_texture.size()));
      // ImGui::Image(ImGui::to_ptr(_temp_texture.object()), ImVec2(_temp_texture.size()));

      // End window draw
      ImGui::End();
    }
  };
} // namespace met