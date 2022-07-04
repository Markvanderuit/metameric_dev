#include <small_gl/dispatch.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/gui/application.hpp>
#include <metameric/gui/detail/imgui.hpp>
#include <metameric/gui//task/viewport_draw_task.hpp>

namespace met {
  ViewportDrawTask::ViewportDrawTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void ViewportDrawTask::init(detail::TaskInitInfo &info) {
    // Get externally shared resources 
    auto &e_gamut_buffer = info.get_resource<gl::Buffer>("global", "color_gamut_buffer");
    auto &e_texture_obj  = info.get_resource<io::TextureData<float>>("global", "color_texture_buffer_cpu");
    auto &e_color_data   = info.get_resource<std::vector<ColorAlpha>>("global", "color_data");

    // Element data to draw a tetrahedron from four vertices using a line strip
    std::vector<uint> gamut_elements = {
      0, 1, 2, 0,
      3, 1, 3, 2
    };

    // Setup objects for gamut line draw
    m_gamut_elem_buffer = gl::Buffer({ .data = as_typed_span<std::byte>(gamut_elements) });
    m_gamut_array = gl::Array({
      .buffers = {{ .buffer = &e_gamut_buffer, .index = 0, .stride = sizeof(glm::vec4) }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
      .elements = &m_gamut_elem_buffer
    });
    m_gamut_program = gl::Program({{ .type = gl::ShaderType::eVertex, 
                                     .path = "resources/shaders/viewport_task/gamut_draw.vert" },
                                   { .type = gl::ShaderType::eFragment,  
                                     .path = "resources/shaders/viewport_task/gamut_draw.frag" }});
    m_gamut_draw = { .type             = gl::PrimitiveType::eLineLoop,
                     .vertex_count     = (uint) gamut_elements.size(),
                     .bindable_array   = &m_gamut_array,
                     .bindable_program = &m_gamut_program };

    // Specify framebuffer color clear value depending on application style
    switch (info.get_resource<ApplicationCreateInfo>("global", "application_create_info").color_mode) {
      case AppliationColorMode::eLight :
        m_fbuffer_clear_value = glm::vec3(1);
        break;
      case AppliationColorMode::eDark :
        m_fbuffer_clear_value = glm::vec3(0);
        break;
    }

    // Setup objects for dataset point draw
    auto dataset_span = as_typed_span<std::byte>(e_color_data);
    m_data_points_buffer = gl::Buffer({ .data = convert_span<std::byte>(dataset_span) });
    m_data_points_array = gl::Array({ 
      .buffers = {{ .buffer = &m_data_points_buffer, .index = 0, .stride = sizeof(ColorAlpha) }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e4 }}
    });
    m_data_points_program = gl::Program({{ .type = gl::ShaderType::eVertex, 
                                           .path = "resources/shaders/viewport_task/texture_draw.vert" },
                                         { .type = gl::ShaderType::eFragment,  
                                           .path = "resources/shaders/viewport_task/texture_draw.frag" }});
    m_data_points_draw = { .type             = gl::PrimitiveType::ePoints,
                           .vertex_count     = (uint) dataset_span.size(),
                           .bindable_array   = &m_data_points_array,
                           .bindable_program = &m_data_points_program };

    // Setup objects for texture point draw
    auto texture_span = as_typed_span<ColorAlpha>(e_texture_obj.data);
    m_texture_points_buffer = gl::Buffer({ .data = convert_span<std::byte>(texture_span) });
    m_texture_points_array = gl::Array({ 
      .buffers = {{ .buffer = &m_texture_points_buffer, .index = 0, .stride  = sizeof(ColorAlpha) }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e4 }}
    });
    m_texture_points_program = gl::Program({{ .type = gl::ShaderType::eVertex, 
                                              .path = "resources/shaders/viewport_task/texture_draw.vert" },
                                            { .type = gl::ShaderType::eFragment,  
                                              .path = "resources/shaders/viewport_task/texture_draw.frag" }});
    m_texture_points_draw = { .type             = gl::PrimitiveType::ePoints,
                              .vertex_count     = (uint) texture_span.size(),
                              .bindable_array   = &m_texture_points_array,
                              .bindable_program = &m_texture_points_program };
  }

  void ViewportDrawTask::eval(detail::TaskEvalInfo &info) {
    // Insert temporary window to modify draw settings
    if (ImGui::Begin("Viewport draw settings")) {
      ImGui::SliderFloat("Line width", &m_gamut_lwidth, 1.f, 16.f, "%.0f");
      ImGui::SliderFloat("Dataset point size", &m_data_points_psize, 1.f, 32.f, "%.0f");
      ImGui::SliderFloat("Texture point size", &m_texture_points_psize, 1.f, 32.f, "%.0f");
    }
    ImGui::End();
                                
    // Get externally shared resources 
    auto &e_viewport_texture      = info.get_resource<gl::Texture2d3f>("viewport", "viewport_texture");
    auto &e_viewport_arcball      = info.get_resource<detail::Arcball>("viewport", "viewport_arcball");
    auto &e_viewport_model_matrix = info.get_resource<glm::mat4>("viewport", "viewport_model_matrix");

    // (re-)create framebuffers and renderbuffers if the viewport has resized
    if (!m_fbuffer.is_init() || e_viewport_texture.size() != m_rbuffer_msaa.size()) {
      m_rbuffer_msaa = {{ .size = e_viewport_texture.size() }};
      m_dbuffer_msaa = {{ .size = e_viewport_texture.size() }};
      m_fbuffer_msaa = {{ .type = gl::FramebufferType::eColor, .attachment = &m_rbuffer_msaa },
                        { .type = gl::FramebufferType::eDepth, .attachment = &m_dbuffer_msaa }};
      m_fbuffer      = {{ .type = gl::FramebufferType::eColor, .attachment = &e_viewport_texture }};
    }
    
    // Declare scoped OpenGL state
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA, true),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true),
                               gl::state::ScopedSet(gl::DrawCapability::eLineSmooth, false) };

    // Prepare multisampled framebuffer as draw target
    m_fbuffer_msaa.bind();
    m_fbuffer_msaa.clear(gl::FramebufferType::eColor, m_fbuffer_clear_value);
    m_fbuffer_msaa.clear(gl::FramebufferType::eDepth, 1.f);
    
    // Update program uniforms
    m_data_points_program.uniform("model_matrix",  e_viewport_model_matrix);
    m_data_points_program.uniform("camera_matrix", e_viewport_arcball.full());
    m_texture_points_program.uniform("model_matrix",  e_viewport_model_matrix);
    m_texture_points_program.uniform("camera_matrix", e_viewport_arcball.full());
    m_gamut_program.uniform("camera_matrix", e_viewport_arcball.full());

    // Dispatch draw calls for pointsets and gamut lines
    gl::state::set_viewport(e_viewport_texture.size());
    gl::state::set_point_size(m_data_points_psize);
    gl::dispatch_draw(m_data_points_draw);
    // gl::state::set_point_size(m_texture_points_psize);
    // gl::dispatch_draw(m_texture_points_draw);
    gl::state::set_line_width(m_gamut_lwidth);
    gl::dispatch_draw(m_gamut_draw);

    // Blit color results into the single-sampled framebuffer with attached viewport texture
    m_fbuffer_msaa.blit_to(m_fbuffer,
                           e_viewport_texture.size(), { 0, 0 },
                           e_viewport_texture.size(), { 0, 0 },
                           gl::FramebufferMaskFlags::eColor);
  }
} // namespace met