#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/viewport/task_draw.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met {
  constexpr std::array<float, 24> cube_vertices = { 
    0.f, 0.f, 0.f,
    0.f, 0.f, 1.f,
    0.f, 1.f, 0.f,
    0.f, 1.f, 1.f,
    1.f, 0.f, 0.f,
    1.f, 0.f, 1.f,
    1.f, 1.f, 0.f,
    1.f, 1.f, 1.f
  };

  constexpr std::array<uint, 30> cube_elements = {
    0, 1, 0, 2,
    1, 3, 2, 3,

    0, 4, 1, 5,
    2, 6, 3, 7,

    4, 5, 4, 6,
    5, 7, 6, 7
  };

  constexpr std::array<uint, 8> gamut_elements = {
    0, 1, 2, 0,
    3, 1, 3, 2
  };

  ViewportDrawTask::ViewportDrawTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void ViewportDrawTask::init(detail::TaskInitInfo &info) {
    // Get externally shared resources 
    auto &e_gamut_buffer   = info.get_resource<gl::Buffer>("gen_spectral_gamut", "color_buffer");
    auto &e_texture_buffer = info.get_resource<gl::Buffer>("gen_spectral_texture", "color_buffer");

    // Setup objects for cube line draw
    m_cube_vertex_buffer = {{ .data = as_span<const std::byte>(cube_vertices) }};
    m_cube_elem_buffer   = {{ .data = as_span<const std::byte>(cube_elements) }};
    m_cube_array         = {{
      .buffers = {{ .buffer = &m_cube_vertex_buffer, .index = 0, .stride = sizeof(eig::Vector3f) }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
      .elements = &m_cube_elem_buffer
    }};
    m_cube_program = gl::Program({{ .type = gl::ShaderType::eVertex, 
                                    .path = "resources/shaders/viewport_task/uniform_draw.vert" },
                                  { .type = gl::ShaderType::eFragment,  
                                    .path = "resources/shaders/viewport_task/vec3_passthrough.frag" }});
    m_cube_draw = { .type             = gl::PrimitiveType::eLines,
                    .vertex_count     = (uint) cube_elements.size(),
                    .bindable_array   = &m_cube_array,
                    .bindable_program = &m_cube_program };

    // Setup objects for gamut line draw
    m_gamut_elem_buffer = gl::Buffer({ .data = as_span<const std::byte>(gamut_elements) });
    m_gamut_array = gl::Array({
      .buffers = {{ .buffer = &e_gamut_buffer, .index = 0, .stride = sizeof(eig::AlArray3f) }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
      .elements = &m_gamut_elem_buffer
    });
    m_gamut_program = gl::Program({{ .type = gl::ShaderType::eVertex, 
                                     .path = "resources/shaders/viewport_task/value_draw.vert" },
                                   { .type = gl::ShaderType::eFragment,  
                                     .path = "resources/shaders/viewport_task/vec3_passthrough.frag" }});
    m_gamut_draw = { .type             = gl::PrimitiveType::eLineLoop,
                     .vertex_count     = (uint) gamut_elements.size(),
                     .bindable_array   = &m_gamut_array,
                     .bindable_program = &m_gamut_program };

    // Setup objects for texture point draw
    m_texture_points_array = gl::Array({ 
      .buffers = {{ .buffer = &e_texture_buffer, .index = 0, .stride = sizeof(eig::AlArray3f) }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }}
    });
    m_texture_points_program = gl::Program({{ .type = gl::ShaderType::eVertex, 
                                              .path = "resources/shaders/viewport_task/value_draw.vert" },
                                            { .type = gl::ShaderType::eFragment,  
                                              .path = "resources/shaders/viewport_task/vec3_passthrough.frag" }});
    m_texture_points_draw = { .type             = gl::PrimitiveType::ePoints,
                              .vertex_count     = (uint) e_texture_buffer.size() / sizeof(std::byte) / 3,
                              .bindable_array   = &m_texture_points_array,
                              .bindable_program = &m_texture_points_program };

    // Set non-changing uniform values
    m_gamut_program.uniform("u_model_matrix", glm::identity<glm::mat4>());
    m_cube_program.uniform("u_model_matrix",  glm::identity<glm::mat4>());
    m_cube_program.uniform("u_value",         glm::vec3(1));
  }

  void ViewportDrawTask::eval(detail::TaskEvalInfo &info) {
    // Insert temporary window to modify draw settings
    if (ImGui::Begin("Viewport draw settings")) {
      ImGui::SliderFloat("Line width", &m_gamut_lwidth, 1.f, 16.f, "%.0f");
      ImGui::SliderFloat("Texture point size", &m_texture_points_psize, 1.f, 32.f, "%.0f");
    }
    ImGui::End();
                                
    // Get externally shared resources 
    auto &e_viewport_texture   = info.get_resource<gl::Texture2d3f>("viewport", "draw_texture");
    auto &e_viewport_arcball   = info.get_resource<detail::Arcball>("viewport", "arcball");
    auto &e_viewport_fbuffer   = info.get_resource<gl::Framebuffer>("viewport_draw_begin", "frame_buffer_msaa");
    
    // Declare scoped OpenGL state
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA, true),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true),
                               gl::state::ScopedSet(gl::DrawCapability::eLineSmooth, false) };

    // Prepare multisampled framebuffer as draw target
    e_viewport_fbuffer.bind();
    gl::state::set_viewport(e_viewport_texture.size());
    
    // Update program uniforms
    auto camera_matrix = e_viewport_arcball.full();
    m_gamut_program.uniform("u_camera_matrix",          camera_matrix);
    m_cube_program.uniform("u_camera_matrix",           camera_matrix);
    m_texture_points_program.uniform("u_camera_matrix", camera_matrix);
    m_texture_points_program.uniform("u_model_matrix",  glm::mat4(1));

    // Dispatch draw for loaded texture points
    gl::state::set_point_size(m_texture_points_psize);
    gl::dispatch_draw(m_texture_points_draw);

    // Dispatch draws for bounding box
    gl::state::set_line_width(m_cube_lwidth);
    gl::dispatch_draw(m_cube_draw);

    // Dispatch draws for gamut shape
    gl::state::set_line_width(m_gamut_lwidth);
    gl::dispatch_draw(m_gamut_draw);
  }
} // namespace met