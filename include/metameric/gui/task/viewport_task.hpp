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

    /* 
      src: https://asliceofrendering.com/camera/2019/11/30/ArcballCamera/
     */
    struct ArcballCamera {
      ArcballCamera() = default;
      ArcballCamera(const glm::vec3 &eye, 
                    const glm::vec3 &center, 
                    const glm::vec3 &up)
      : eye(eye), center(center), up(up), view_matrix(glm::lookAt(eye, center, up)) { }

      glm::vec3 eye, center, up;
      glm::mat4 view_matrix;

      glm::vec3 view_vector() const { return glm::transpose(view_matrix)[2]; }
      glm::vec3 right_vector() const { return -glm::transpose(view_matrix)[0]; }

      void eval(const glm::vec2 &delta, const glm::vec2 &viewport) {
        // Homogeneous versions of camera eye, pivot center
        glm::vec4 eye_hom = glm::vec4(eye, 1.f);
        glm::vec4 cen_hom = glm::vec4(center, 1.f);

        // Calculate amount of rotation in radians
        auto delta_step = glm::vec2(-2.f, 1.f) * glm::pi<float>() / viewport;
        auto delta_angle = delta * delta_step;

        // Prevent view=up edgecase
        std::cout << view_vector() << '\n';
        if (glm::dot(view_vector(), up) * glm::sign(delta_angle.y) >= 0.99f) {
          delta_angle.y = 0.f;
        }

        // Rotate camera around pivot on _separate_ axes
        glm::mat4 rot = glm::rotate(delta_angle.y, right_vector())
                      * glm::rotate(delta_angle.x, up);

        // Apply rotation and recompute lookat matrix
        eye = glm::vec3(cen_hom + rot * (eye_hom - cen_hom));
        view_matrix = glm::lookAt(eye, center, up);
      }
    };
  } // namespace detail

  class ViewportTask : public detail::AbstractTask {
    // Test components for a simple triangle draw
    gl::Array        m_triangle_array;
    gl::Buffer       m_triangle_buffer;
    gl::DrawInfo     m_triangle_draw;
    gl::Texture2d3f  m_temp_texture;
    
    // Vertex draw components
    gl::Array        m_texture_array;
    gl::Buffer       m_texture_buffer; 
    gl::DrawInfo     m_texture_draw;
    gl::Program      m_texture_program;

    // Transform components
    detail::ArcballCamera    
                     m_camera;
    float            m_fov = glm::radians(45.f);
    glm::mat4        m_model_matrix = glm::identity<glm::mat4>();
    glm::mat4        m_projection_matrix;

    // Frame draw components
    gl::Framebuffer  m_fb_msaa, m_fb;
    gl::Renderbuffer<float, 3, gl::RenderbufferType::eMultisample>
                     m_fb_rbuffer_msaa;
    gl::Texture2d3f  m_fb_texture;

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

      // Initialize matrix transforms 
      m_camera = detail::ArcballCamera(glm::vec3(0.f, 2.f, 0.f),
                                       glm::vec3(0.f, 0.f, 0.f),
                                       glm::vec3(0.f, 0.f, 1.f));
      // m_model_matrix = glm::identity<glm::mat4>();
      /* m_view_matrix = glm::lookAt(glm::vec3(0.f, 2.f, 0.f),
                                  glm::vec3(0.f, 0.f, 0.f),
                                  glm::vec3(0.f, 0.f, 1.f)); */

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
      ImGui::Begin("Camera settings");
      ImGui::SliderAngle("Camera fov", &m_fov, 1.f, 360.f, "%.0f");
      ImGui::End();

      // Begin window draw
      auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 0.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};
      ImGui::Begin("Viewport");

      // Compute framebuffer/texture/viewport size based on window content region
      const auto viewport_size = static_cast<glm::ivec2>(ImGui::GetWindowContentRegionMax())
                               - static_cast<glm::ivec2>(ImGui::GetWindowContentRegionMin());

      // (Re)initialize framebuffer and texture objects on viewport resize
      if (!m_fb.is_init() || m_fb_texture.size() != viewport_size) {
        // Intermediate framebuffer is backed by a multisampled renderbuffer
        m_fb_rbuffer_msaa = {{ .size = viewport_size }};
        m_fb_msaa = {{ .type = gl::FramebufferType::eColor, .attachment = &m_fb_rbuffer_msaa }};

        // Output framebuffer is single-sample
        m_fb_texture = {{ .size = viewport_size }};
        m_fb = {{ .type = gl::FramebufferType::eColor, .attachment = &m_fb_texture }};
      }

      // Setup framebuffer as draw target
      m_fb_msaa.bind();
      m_fb_msaa.clear(gl::FramebufferType::eColor, glm::vec3(0.f));

      // Handle view rotation
      auto &io = ImGui::GetIO();
      if (io.MouseDown[0]) {
        glm::vec2 mouse_delta = static_cast<glm::vec2>(io.MouseDelta);
        m_camera.eval(mouse_delta, viewport_size);


        // glm::vec2 viewport_offset = ImGui::GetWindowContentRegionMin();
                              // / static_cast<glm::vec2>(viewport_size);
        // glm::vec2 mouse_pos = (static_cast<glm::vec2>(io.MousePos) - viewport_offset) 
        //                     / static_cast<glm::vec2>(viewport_size)
        //                     * 2.f - 1.f;
        /* glm::vec2 mouse_pos_prev = mouse_pos - mouse_delta;

        glm::vec2 p0_xy = mouse_pos_prev, p1_xy = mouse_pos;
        glm::vec3 p0 = glm::vec3(p0_xy, 
          glm::sqrt(1.0 - glm::pow(p0_xy.x, 2) - glm::pow(p0_xy.y, 2)));
        glm::vec3 p1 = glm::vec3(p1_xy, 
          glm::sqrt(1.0 - glm::pow(p1_xy.x, 2) - glm::pow(p1_xy.y, 2)));
        if (!(p0.z >= 0.f && p0.z <= 1.f)) {
          p0.z = 0.f;
          p0 = glm::normalize(p0);
        }
        if (!(p1.z >= 0.f && p1.z <= 1.f)) {
          p1.z = 0.f;
          p1 = glm::normalize(p1);
        }

        glm::vec3 ax = glm::normalize(glm::cross(p1, p0));
        float rot = glm::orientedAngle(p0, p1, ax);

        if (mouse_delta != glm::vec2(0.f)) {
          // m_view_eye = glm::rotate(rot, ax) * glm::vec4(m_view_eye, 1.f));
          // m_view_up = glm::rotate(rot, ax) * glm::vec4(m_view_up, 1.f));
          m_view_matrix = m_view_matrix * glm::rotate(rot, ax); // * m_view_matrix;
        }

        std::cout << "delta" << '\t' << mouse_delta << '\n'
                  << "p0" << '\t' << p0 << '\n'
                  << "p1" << '\t' << p1 << '\n'
                  << "rot" << '\t' << rot << '\n'
                  << "ax" << '\t' << ax << '\n'
                  << "m_view_eye" << '\t' << m_view_eye << '\n'; */
        // std::cout << mouse_delta << '\t' << p0 << '\t' << p1 << '\t' << rot << '\n';
        
        // glm::vec3 
        // glm::vec2 delta_p = -glm::vec2(io.MouseDelta);


        /* glm::mat4 rx = glm::rotate(glm::radians(-io.MouseDelta.x), m_view_right);
        glm::mat4 ry = glm::rotate(glm::radians(-io.MouseDelta.y), m_view_up);
        m_view_eye = glm::vec3(ry * rx * glm::vec4(m_view_eye, 1.f));
        m_view_up = glm::vec3(ry * rx * glm::vec4(m_view_up, 1.f));
        m_view_right = glm::vec3(ry * rx * glm::vec4(m_view_right, 1.f));
        m_view_matrix = glm::lookAt(m_view_eye,
                                    glm::vec3(0, 0, 0),
                                    glm::vec3(0, 1, 0)); */
        // m_view_matrix = ry * rx * m_view_matrix;
      }

      // Update draw matrices
      const float aspect = static_cast<float>(viewport_size.x) / static_cast<float>(viewport_size.y);
      m_projection_matrix = glm::perspective(m_fov, aspect, 0.001f, 1000.f);
      /* auto view_matrix = glm::lookAt(glm::vec3(2, 0, 0),
                                     glm::vec3(0, 0, 0),
                                     glm::vec3(0, 1, 0)) * m_view_matrix; */
      m_texture_program.uniform("model_matrix", m_model_matrix);
      m_texture_program.uniform("view_matrix", m_camera.view_matrix);
      m_texture_program.uniform("projection_matrix", m_projection_matrix);

      { // Setup scoped draw state and dispatch a draw call
        auto draw_state = { gl::state::ScopedSet(gl::DrawCapability::eMSAA, true),
                            gl::state::ScopedSet(gl::DrawCapability::eCullFace, false) };
        gl::state::set_viewport(viewport_size);
        gl::dispatch_draw(m_texture_draw);
      }

      // Blit MSAA framebuffer into single-sample framebuffer to obtain output texture
      m_fb_msaa.blit_to(m_fb, viewport_size, { 0, 0 }, 
                              viewport_size, { 0, 0 }, 
                              gl::FramebufferMaskFlags::eColor);

      // Pass output to viewport; flip y-axis UVs for correct orientation
      ImGui::Image(ImGui::to_ptr(m_fb_texture.object()),  m_fb_texture.size(), 
        glm::vec2(0, 1), glm::vec2(1, 0));

      // Insert ImGuizmo components
      ImGuizmo::SetRect(0, 0, viewport_size.x, viewport_size.y);
      ImGuizmo::Manipulate(glm::value_ptr(m_camera.view_matrix), 
                           glm::value_ptr(m_projection_matrix),
        ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::MODE::LOCAL, glm::value_ptr(m_model_matrix));
      ImGuizmo::ViewManipulate(glm::value_ptr(m_camera.view_matrix),
       2.0, glm::vec2(16, viewport_size.y - 48), glm::vec2(64),
       ImGui::GetColorU32(glm::vec4(0.5, 0.5, 0.5, 0.5)));

      // End window draw
      ImGui::End();
    }
  };
} // namespace met