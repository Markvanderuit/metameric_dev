#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/gui/application.hpp>
#include <metameric/gui/detail/imgui.hpp>
#include <metameric/gui/detail/arcball.hpp>
#include <metameric/gui//task/viewport_draw_task.hpp>

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
    auto &e_gamut_buffer = info.get_resource<gl::Buffer>("global", "color_gamut_buffer");
    auto &e_texture_obj  = info.get_resource<io::TextureData<float>>("global", "color_texture_buffer_cpu");
    auto &e_color_data   = info.get_resource<std::vector<PaddedColor>>("global", "color_data");

    // Setup objects for cube line draw
    m_cube_vertex_buffer = {{ .data = as_typed_span<const std::byte>(cube_vertices) }};
    m_cube_elem_buffer   = {{ .data = as_typed_span<const std::byte>(cube_elements) }};
    m_cube_array         = {{
      .buffers = {{ .buffer = &m_cube_vertex_buffer, .index = 0, .stride = sizeof(glm::vec3) }},
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
    m_gamut_elem_buffer = gl::Buffer({ .data = as_typed_span<const std::byte>(gamut_elements) });
    m_gamut_array = gl::Array({
      .buffers = {{ .buffer = &e_gamut_buffer, .index = 0, .stride = sizeof(glm::vec3) }},
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

    // Setup objects for dataset point draw
    auto dataset_span = as_typed_span<std::byte>(e_color_data);
    m_data_points_buffer = gl::Buffer({ .data = convert_span<std::byte>(dataset_span) });
    m_data_points_array = gl::Array({ 
      .buffers = {{ .buffer = &m_data_points_buffer, .index = 0, .stride = sizeof(PaddedColor) }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }}
    });
    m_data_points_program = gl::Program({{ .type = gl::ShaderType::eVertex, 
                                           .path = "resources/shaders/viewport_task/value_draw.vert" },
                                         { .type = gl::ShaderType::eFragment,  
                                           .path = "resources/shaders/viewport_task/vec3_passthrough.frag" }});
    m_data_points_draw = { .type             = gl::PrimitiveType::ePoints,
                           .vertex_count     = (uint) e_color_data.size(),
                           .bindable_array   = &m_data_points_array,
                           .bindable_program = &m_data_points_program };

    // Setup objects for texture point draw
    auto texture_span = as_typed_span<PaddedColor>(e_texture_obj.data);
    m_texture_points_buffer = gl::Buffer({ .data = convert_span<std::byte>(texture_span) });
    m_texture_points_array = gl::Array({ 
      .buffers = {{ .buffer = &m_texture_points_buffer, .index = 0, .stride = sizeof(PaddedColor) }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }}
    });
    m_texture_points_program = gl::Program({{ .type = gl::ShaderType::eVertex, 
                                              .path = "resources/shaders/viewport_task/value_draw.vert" },
                                            { .type = gl::ShaderType::eFragment,  
                                              .path = "resources/shaders/viewport_task/vec3_passthrough.frag" }});
    m_texture_points_draw = { .type             = gl::PrimitiveType::ePoints,
                              .vertex_count     = (uint) e_texture_obj.data.size() / 3,
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
      ImGui::SliderFloat("Dataset point size", &m_data_points_psize, 1.f, 32.f, "%.0f");
      ImGui::SliderFloat("Texture point size", &m_texture_points_psize, 1.f, 32.f, "%.0f");
    }
    ImGui::End();
                                
    // Get externally shared resources 
    auto &e_viewport_texture      = info.get_resource<gl::Texture2d3f>("viewport", "viewport_texture");
    auto &e_viewport_arcball      = info.get_resource<detail::Arcball>("viewport", "viewport_arcball");
    auto &e_viewport_model_matrix = info.get_resource<glm::mat4>("viewport", "viewport_model_matrix");
    auto &e_viewport_fbuffer      = info.get_resource<gl::Framebuffer>("viewport_draw_begin", "viewport_fbuffer_msaa");
    
    // Declare scoped OpenGL state
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA, true),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true),
                               gl::state::ScopedSet(gl::DrawCapability::eLineSmooth, false) };

    // Prepare multisampled framebuffer as draw target
    e_viewport_fbuffer.bind();
    gl::state::set_viewport(e_viewport_texture.size());
    
    // Update program uniforms
    auto camera_matrix = e_viewport_arcball.full();
    m_data_points_program.uniform("u_camera_matrix",    camera_matrix);
    m_gamut_program.uniform("u_camera_matrix",          camera_matrix);
    m_cube_program.uniform("u_camera_matrix",           camera_matrix);
    m_texture_points_program.uniform("u_camera_matrix", camera_matrix);
    m_data_points_program.uniform("u_model_matrix",     e_viewport_model_matrix);
    m_texture_points_program.uniform("u_model_matrix",  e_viewport_model_matrix);

    // Dispatch draw for spectral dataset points
    // gl::state::set_point_size(m_data_points_psize);
    // gl::dispatch_draw(m_data_points_draw);

    // Dispatch draw for loaded texture points
    gl::state::set_point_size(m_texture_points_psize);
    gl::dispatch_draw(m_texture_points_draw);

    // Dispatch draws for gamut and bounding box
    gl::state::set_line_width(m_cube_lwidth);
    gl::dispatch_draw(m_cube_draw);

    // Dispatch draws for gamut and bounding box
    gl::state::set_line_width(m_gamut_lwidth);
    gl::dispatch_draw(m_gamut_draw);
  }
} // namespace met