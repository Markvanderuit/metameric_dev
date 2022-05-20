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
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <iostream>

namespace gl {
  using Texture2dms = Texture2d<float, 3, gl::TextureType::eMultisample>;
} // namespace gl

namespace met {
  namespace detail {
    template <class T, class C>
    std::span<T> as_typed_span(C &c) {
      auto data = c.data();
      guard(data, {});
      return { reinterpret_cast<T*>(data), (c.size() * sizeof(C::value_type)) / sizeof(T) };
    }

    template <class T, class U>
    std::span<T> convert_span(std::span<U> s) {
      auto data = s.data();
      guard(data, {});
      return { reinterpret_cast<T*>(data), s.size_bytes() / sizeof(T) };
    }
  } // namespace detail

  class ViewportTask : public detail::AbstractTask {
    // Test components for a simple triangle draw
    gl::Array        m_triangle_array;
    gl::Buffer       m_triangle_buffer;
    gl::DrawInfo     m_triangle_draw;
    
    // Array draw components
    gl::Array        m_texture_array;
    gl::Buffer       m_texture_buffer; 
    gl::DrawInfo     m_texture_draw;
    gl::Program      m_texture_program;

    // Camera components
    float            m_model_rotation = 0.f;
    glm::mat4        m_model_view_matrix;

    // Frame draw components
    gl::Framebuffer  m_fb_msaa, m_fb;
    gl::Texture2dms  m_fb_texture_msaa;
    gl::Texture2d3f  m_fb_texture;

    // Testing components
    gl::Texture2d3f  m_temp_texture;

  public:
    ViewportTask(const std::string &name)
    : detail::AbstractTask(name) { }

    void init(detail::TaskInitInfo &info) override {
      // Obtain texture data from external resource
      auto &texture_obj = info.get_resource<io::TextureData<float>>("global", "texture_data");
      auto texture_data = detail::as_typed_span<glm::vec3>(texture_obj.data);

      // Load texture data into vertex buffer and create vertex array around it
      m_texture_buffer = gl::Buffer({ .data = detail::convert_span<std::byte>(texture_data) });
      m_texture_array = gl::Array({ 
        .buffers = {{ .buffer = &m_texture_buffer, .index = 0, .stride  = sizeof(glm::vec3) }},
        .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }}
      });
        
      // Temporary triangle data
      std::vector<glm::vec3> triangle_data = { 
        glm::vec3(-.66f, .33f, 0), glm::vec3(.66f, .33f, 0), glm::vec3(0, -.66f, 0) };
      m_triangle_buffer = gl::Buffer({ .data = detail::as_typed_span<std::byte>(triangle_data) });
      m_triangle_array = gl::Array({ 
        .buffers = {{ .buffer = &m_triangle_buffer, .index = 0, .stride  = sizeof(glm::vec3) }},
        .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }}
      });
      
      // Build shader program
      m_texture_program = gl::Program({
        { .type = gl::ShaderType::eVertex, .path = "../resources/shaders/texture_draw.vert.spv" },
        { .type = gl::ShaderType::eFragment,  .path = "../resources/shaders/texture_draw.frag.spv" }
      });
      m_texture_program.uniform("projection_matrix", glm::perspective(45.f, 1.f, 0.0001f, 1000.f));

      // Assemble draw object for render of vertex array
      m_texture_draw = {
        .type = gl::PrimitiveType::ePoints, .array = &m_texture_array, 
        .vertex_count = (uint) texture_data.size(), .program = &m_texture_program };
      m_triangle_draw = { 
        .type = gl::PrimitiveType::eTriangles, .array = &m_triangle_array,
        .vertex_count = (uint) triangle_data.size(), .program = &m_texture_program };

      // Simple texture for testing purposes; holds texture object data
      m_temp_texture = gl::Texture2d3f({ .size = texture_obj.size, .data = texture_obj.data });
    }

    void eval(detail::TaskEvalInfo &info) override {
      // Begin window draw
      auto viewport_vars = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 0.f), 
                             ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                             ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};
      ImGui::Begin("Viewport", 0, ImGuiWindowFlags_NoDecoration);

      // Compute texture viewport size based on window content region
      const auto viewport_size = static_cast<glm::ivec2>(ImGui::GetWindowContentRegionMax())
                               - static_cast<glm::ivec2>(ImGui::GetWindowContentRegionMin());

      // (Re)initialize framebuffer and texture objects on viewport resize
      if (!m_fb.is_init() || m_fb_texture.size() != viewport_size) {
        // Intermediate framebuffer is backed by a multisample texture
        m_fb_texture_msaa = {{ .size = viewport_size }};
        m_fb_msaa = {{ .type = gl::FramebufferType::eColor, .texture = &m_fb_texture_msaa }};

        // Output framebuffer is single-sample
        m_fb_texture = {{ .size = viewport_size }};
        m_fb = {{ .type = gl::FramebufferType::eColor, .texture = &m_fb_texture }};
      }

      // Setup framebuffer as draw target
      m_fb_msaa.bind();
      m_fb_msaa.clear(gl::FramebufferType::eColor, glm::vec3(0.f));

      // Perform minr rotation until I get a trackball working
      m_model_rotation += 1.5f;
      m_model_view_matrix = glm::lookAt(glm::vec3(0.f, 0.f, 2.f),
                                        glm::vec3(0.f, 0.f, 0.f),
                                        glm::vec3(0.f, 1.f, 0.f))
                          * glm::rotate(glm::radians(m_model_rotation), 
                                        glm::vec3(0.f, 1.f, 0.f));
      m_texture_program.uniform("model_view_matrix", m_model_view_matrix);
      m_texture_program.uniform("viewport_size", viewport_size);

      { // Setup draw state and dispatch a draw call
        auto draw_state = { gl::state::ScopedSet(gl::DrawCapability::eMSAA, true),
                            gl::state::ScopedSet(gl::DrawCapability::eCullFace, false) };
        gl::state::set_viewport(viewport_size);
        gl::dispatch_draw(m_triangle_draw);
        gl::sync::texture_barrier();
      }

      // Blit from MSAA framebuffer to single-sample output framebuffer 
      m_fb_msaa.blit_to(m_fb, m_fb_texture_msaa.size(), { 0, 0}, 
                              m_fb_texture.size(), { 0, 0 }, 
                              gl::FramebufferMaskFlags::eColor);

      // Draw framebuffer to viewport
      ImGui::Image(ImGui::to_ptr(m_fb_texture.object()), m_fb_texture.size());
      // ImGui::Image(ImGui::to_ptr(m_temp_texture.object()), m_temp_texture.size());

      // End window draw
      ImGui::End();
    }
  };
} // namespace met