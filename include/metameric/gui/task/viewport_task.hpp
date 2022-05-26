#pragma once

#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/texture.hpp>
#include <metameric/core/detail/glm.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/gui/detail/imgui.hpp>
#include <metameric/gui/detail/arcball.hpp>
#include <metameric/gui/detail/linear_scheduler/task.hpp>
#include <ImGuizmo.h>
#include <iostream>

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
    gl::Texture2d3f  m_temp_texture;

    // Camera components
    detail::Arcball  m_arcball;
    glm::mat4        m_model_matrix = glm::identity<glm::mat4>();
    glm::vec2        m_viewport_size;
    
    // Vertex draw components
    gl::Array        m_texture_array;
    gl::Buffer       m_texture_buffer; 
    gl::DrawInfo     m_texture_draw;
    gl::Program      m_texture_program;

    // Frame draw components
    gl::Framebuffer  m_fb_msaa, m_fb;
    gl::Renderbuffer<float, 3, gl::RenderbufferType::eMultisample>
                     m_fb_rbuffer_msaa;
    gl::Texture2d3f  m_fb_texture;

    void draw_texture() {
      // (re)initialize framebuffer and attachments
      if (!m_fb.is_init() || m_fb_texture.size() != glm::ivec2(m_viewport_size)) {
        // Intermediate framebuffer is backed by a multisampled renderbuffer
        m_fb_rbuffer_msaa = {{ .size = m_viewport_size }};
        m_fb_msaa = {{ .type = gl::FramebufferType::eColor, .attachment = &m_fb_rbuffer_msaa }};

        // Output framebuffer is single-sample
        m_fb_texture = {{ .size = m_viewport_size }};
        m_fb = {{ .type = gl::FramebufferType::eColor, .attachment = &m_fb_texture }};
      }

      // Set scoped OpenGL draw capabilities 
      auto draw_state = { gl::state::ScopedSet(gl::DrawCapability::eMSAA, true) };
      gl::state::set_viewport(m_viewport_size);

      // Setup program; push updated matrices
      m_texture_program.uniform("model_matrix", m_model_matrix);
      m_texture_program.uniform("view_matrix", m_arcball.view());
      m_texture_program.uniform("projection_matrix", m_arcball.proj());

      // Setup msaa framebuffer as draw target
      m_fb_msaa.bind();
      m_fb_msaa.clear(gl::FramebufferType::eColor, glm::vec3(0.f));

      // Dispatch draw call with provided components
      gl::dispatch_draw(m_texture_draw);

      // Blit MSAA framebuffer into single-sample framebuffer with attached output texture
      m_fb_msaa.blit_to(m_fb, m_viewport_size, { 0, 0 }, 
                              m_viewport_size, { 0, 0 }, 
                              gl::FramebufferMaskFlags::eColor);
    }

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
        glm::vec3(-.66f, 0.f, 0), glm::vec3(.66f, 0.f, 0), glm::vec3(0, 1.f, 0) };
      m_triangle_buffer = gl::Buffer({ .data = detail::as_typed_span<std::byte>(triangle_data) });
      m_triangle_array = gl::Array({ 
        .buffers = {{ .buffer = &m_triangle_buffer, .index = 0, .stride  = sizeof(glm::vec3) }},
        .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }}
      });

      // Build shader program
      m_texture_program = gl::Program({
        { .type = gl::ShaderType::eVertex, 
          .path = "resources/shaders/viewport_task/texture_draw.vert",
          .is_spirv_binary = false },
        { .type = gl::ShaderType::eFragment,  
          .path = "resources/shaders/viewport_task/texture_draw.frag",
          .is_spirv_binary = false }
      });

      // Assemble draw object data for provided vertex array
      m_texture_draw = {
        .type = gl::PrimitiveType::ePoints, .array = &m_texture_array, 
        .vertex_count = (uint) texture_data.size(), .program = &m_texture_program 
      };
      m_triangle_draw = { 
        .type = gl::PrimitiveType::eTriangles, .array = &m_triangle_array,
        .vertex_count = (uint) triangle_data.size(), .program = &m_texture_program 
      };

      // Simple texture for testing purposes; holds texture object data
      m_temp_texture = gl::Texture2d3f({ .size = texture_obj.size, .data = texture_obj.data });
    }

    void eval(detail::TaskEvalInfo &info) override {
      // Begin window draw
      auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 0.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};
      ImGui::Begin("Viewport");

      // Update viewport size
      auto viewport_size = static_cast<glm::vec2>(ImGui::GetWindowContentRegionMax())
                         - static_cast<glm::vec2>(ImGui::GetWindowContentRegionMin());
      if (viewport_size != m_viewport_size) {
        m_viewport_size = viewport_size;
        m_arcball.m_aspect = m_viewport_size.x / m_viewport_size.y;
        m_arcball.update_matrices();
      }

      // Pass drawn texture texture to viewport; flip y-axis UVs for correct orientation
      draw_texture();
      ImGui::Image(ImGui::to_ptr(m_fb_texture.object()), m_fb_texture.size(), 
        glm::vec2(0, 1), glm::vec2(1, 0));

      // Handle arcball rotation
      auto &io = ImGui::GetIO();
      if (ImGui::IsItemHovered()) {
        m_arcball.update_dist_delta(io.MouseWheel);
        if (io.MouseDown[0] && !ImGuizmo::IsUsing()) {
          m_arcball.update_pos_delta(static_cast<glm::vec2>(io.MouseDelta) / m_viewport_size);
        }
        m_arcball.update_matrices();
      }
      
      // Insert ImGuizmo components
      auto rect_min = ImGui::GetWindowContentRegionMin();
      auto rect_max = ImGui::GetWindowContentRegionMax();
      ImGuizmo::SetRect(rect_min.x, rect_min.y, rect_max.x, rect_max.y);
      // ImGui::PushClipRect(ImGui::GetWindowContentRegionMin(),
      //                     ImGui::GetWindowContentRegionMax(), false);
      ImGuizmo::Manipulate(glm::value_ptr(m_arcball.view()), 
                           glm::value_ptr(m_arcball.proj()),
                           ImGuizmo::OPERATION::TRANSLATE, 
                           ImGuizmo::MODE::LOCAL, 
                           glm::value_ptr(m_model_matrix));
      ImGuizmo::ViewManipulate(glm::value_ptr(m_arcball.view()),
                               2.0, 
                               glm::vec2(16, m_viewport_size.y - 48), 
                               glm::vec2(64),
                               ImGui::GetColorU32(glm::vec4(0.5, 0.5, 0.5, 0.5)));
      // ImGui::PopClipRect();

      ImGui::End();
    }
  };
} // namespace met