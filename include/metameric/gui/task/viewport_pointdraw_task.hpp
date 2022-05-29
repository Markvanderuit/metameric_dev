#pragma once

#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/glm.hpp>
#include <metameric/gui/application.hpp>
#include <metameric/gui/detail/arcball.hpp>
#include <metameric/gui/detail/imgui.hpp>
#include <metameric/gui/detail/linear_scheduler/task.hpp>

namespace met {
  class ViewportPointdrawTask : public detail::AbstractTask {
    // Gamut draw components
    gl::Buffer       m_gamut_elem_buffer;
    gl::Array        m_gamut_array;
    gl::DrawInfo     m_gamut_draw;
    gl::Program      m_gamut_program;
    
    // Pointset draw components
    gl::Buffer       m_point_buffer; 
    gl::Array        m_point_array;
    gl::DrawInfo     m_point_draw;
    gl::Program      m_point_program;

    // Framebuffers and attachments
    gl::Renderbuffer<float, 3,
                     gl::RenderbufferType::eMultisample>
                     m_rbuffer_msaa;
    gl::Renderbuffer<gl::DepthComponent, 1,
                     gl::RenderbufferType::eMultisample>
                     m_dbuffer_msaa;
    gl::Framebuffer  m_fbuffer_msaa;
    gl::Framebuffer  m_fbuffer;
    glm::vec3        m_fbuffer_clear_value;

    // Draw settings passed to ImGui
    float            m_draw_point_size = 1.f;
    float            m_draw_line_width = 1.f;

  public:
    ViewportPointdrawTask(const std::string &name)
    : detail::AbstractTask(name) { }

    void init(detail::TaskInitInfo &info) override {
      // Get externally shared resources 
      auto &e_gamut_buffer = info.get_resource<gl::Buffer>("gamut_picker", "gamut_buffer");
      auto &e_texture_obj = info.get_resource<io::TextureData<float>>("global", "texture_data");

      // Element data to draw a tetrahedron from four vertices using a line strip
      std::vector<uint> gamut_elements = {
        0, 1, 2, 0,
        3, 1, 3, 2
      };

      // Load gamut data into buffers and create array object for upcoming draw
      m_gamut_elem_buffer = gl::Buffer({ .data = as_typed_span<std::byte>(gamut_elements) });
      m_gamut_array = gl::Array({
        .buffers = {{ .buffer = &e_gamut_buffer, .index = 0, .stride = sizeof(glm::vec3) }},
        .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
        .elements = &m_gamut_elem_buffer
      });

      // Build shader program
      m_gamut_program = gl::Program({
        { .type = gl::ShaderType::eVertex, 
          .path = "resources/shaders/viewport_task/gamut_draw.vert",
          .is_spirv_binary = false },
        { .type = gl::ShaderType::eFragment,  
          .path = "resources/shaders/viewport_task/gamut_draw.frag",
          .is_spirv_binary = false }
      });
      
      // Build draw object data for provided array object
      m_gamut_draw = { .type = gl::PrimitiveType::eLineLoop,
                       .vertex_count = (uint) gamut_elements.size(),
                       .bindable_array = &m_gamut_array,
                       .bindable_program = &m_gamut_program,
                       .bindable_framebuffer = &m_fbuffer_msaa };

      // Specify framebuffer color clear value depending on application style
      switch (info.get_resource<ApplicationCreateInfo>("global", "application_create_info").color_mode) {
        case AppliationColorMode::eLight :
          m_fbuffer_clear_value = glm::vec3(1);
          break;
        case AppliationColorMode::eDark :
          m_fbuffer_clear_value = glm::vec3(0);
          break;
      }

      // Load texture data into vertex buffer and create array object for upcoming draw
      auto texture_data = as_typed_span<glm::vec3>(e_texture_obj.data);
      m_point_buffer = gl::Buffer({ .data = convert_span<std::byte>(texture_data) });
      m_point_array = gl::Array({ 
        .buffers = {{ .buffer = &m_point_buffer, .index = 0, .stride  = sizeof(glm::vec3) }},
        .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }}
      });

      // Build shader program
      m_point_program = gl::Program({
        { .type = gl::ShaderType::eVertex, 
          .path = "resources/shaders/viewport_task/texture_draw.vert",
          .is_spirv_binary = false },
        { .type = gl::ShaderType::eFragment,  
          .path = "resources/shaders/viewport_task/texture_draw.frag",
          .is_spirv_binary = false }
      });

      // Build draw object data for provided array object
      m_point_draw = { .type = gl::PrimitiveType::ePoints,
                       .vertex_count = (uint) texture_data.size(),
                       .bindable_array = &m_point_array,
                       .bindable_program = &m_point_program,
                       .bindable_framebuffer = &m_fbuffer_msaa };
    }

    void eval(detail::TaskEvalInfo &info) override {
      // Insert temporary window to modify draw settings
      if (ImGui::Begin("Viewport draw settings")) {
        ImGui::SliderFloat("Line width", &m_draw_line_width, 1.f, 16.f, "%.0f");
        ImGui::SliderFloat("Point size", &m_draw_point_size, 1.f, 32.f, "%.0f");
      }
      ImGui::End();
                                 
      // Get externally shared resources 
      auto &e_viewport_texture = info.get_resource<gl::Texture2d3f>("viewport", "viewport_texture");
      auto &e_viewport_arcball = info.get_resource<detail::Arcball>("viewport", "viewport_arcball");
      auto &e_viewport_model_matrix = info.get_resource<glm::mat4>("viewport", "viewport_model_matrix");

      // (re-)create framebuffers and renderbuffers if the viewport has resized
      if (!m_fbuffer.is_init() || e_viewport_texture.size() != m_rbuffer_msaa.size()) {
        m_rbuffer_msaa  = {{ .size = e_viewport_texture.size() }};
        m_dbuffer_msaa  = {{ .size = e_viewport_texture.size() }};
        m_fbuffer_msaa  = {{ .type = gl::FramebufferType::eColor, .attachment = &m_rbuffer_msaa },
                           { .type = gl::FramebufferType::eDepth, .attachment = &m_dbuffer_msaa }};
        m_fbuffer       = {{ .type = gl::FramebufferType::eColor, .attachment = &e_viewport_texture }};
      }
      
      // Declare scoped OpenGL state
      auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA, true),
                                 gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true),
                                 gl::state::ScopedSet(gl::DrawCapability::eLineSmooth, false) };

      // Clear multisampled framebuffer components
      m_fbuffer_msaa.clear(gl::FramebufferType::eColor, m_fbuffer_clear_value);
      m_fbuffer_msaa.clear(gl::FramebufferType::eDepth, 1.f);

      // Viewport size equals output texture size
      gl::state::set_viewport(e_viewport_texture.size());
      gl::state::set_line_width(m_draw_line_width);
      gl::state::set_point_size(m_draw_point_size);

      // Draw point set
      m_point_program.uniform("model_matrix",  e_viewport_model_matrix);
      m_point_program.uniform("camera_matrix", e_viewport_arcball.full());
      gl::dispatch_draw(m_point_draw);

      // Draw gamut
      m_gamut_program.uniform("camera_matrix", e_viewport_arcball.full());
      gl::dispatch_draw(m_gamut_draw);

      // Blit color results into the single-sampled framebuffer with attached viewport texture
      m_fbuffer_msaa.blit_to(m_fbuffer, 
                             e_viewport_texture.size(), { 0, 0 }, 
                             e_viewport_texture.size(), { 0, 0 }, 
                             gl::FramebufferMaskFlags::eColor);
    }
  };
} // namespace met