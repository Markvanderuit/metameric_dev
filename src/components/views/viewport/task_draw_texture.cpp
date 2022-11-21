#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/viewport/task_draw_texture.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met {
  constexpr float point_psize_min = 0.001f;
  constexpr float point_psize_max = 0.003f;

  constexpr std::array<float, 2 * 4> verts = {
    -1.f, -1.f,
     1.f, -1.f,
     1.f,  1.f,
    -1.f,  1.f
  };

  constexpr std::array<uint, 2 * 3> elems = {
    0, 1, 2,
    2, 3, 0
  };

  ViewportDrawTextureTask::ViewportDrawTextureTask(const std::string &name)
  : detail::AbstractTask(name, true) { }

  void ViewportDrawTextureTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_texture_buffer = info.get_resource<gl::Buffer>("gen_spectral_texture", "color_buffer");

    // Setup program for instanced billboard point draw
    m_program = {{ .type = gl::ShaderType::eVertex,   .path = "resources/shaders/viewport/draw_texture.vert" },
                 { .type = gl::ShaderType::eFragment, .path = "resources/shaders/viewport/draw_texture.frag" }};

    // Setup objects for instanced quad draw
    m_vert_buffer = {{ .data = cnt_span<const std::byte>(verts) }};
    m_elem_buffer = {{ .data = cnt_span<const std::byte>(elems) }};
    m_array = {{
      .buffers = {{ .buffer = &m_vert_buffer,    .index = 0, .stride = 2 * sizeof(float),      .divisor = 0 },
                  { .buffer = &e_texture_buffer, .index = 1, .stride = sizeof(eig::AlArray3f), .divisor = 1 }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e2 },
                  { .attrib_index = 1, .buffer_index = 1, .size = gl::VertexAttribSize::e3 }},
      .elements = &m_elem_buffer
    }};
    m_draw = {
      .type             = gl::PrimitiveType::eTriangles,
      .vertex_count     = elems.size(),
      .instance_count   = (uint) (e_texture_buffer.size() / sizeof(eig::AlArray3f)),
      .bindable_array   = &m_array,
      .bindable_program = &m_program
    };

    // Set constant uniforms
    m_program.uniform("u_point_radius_min", point_psize_min);
    m_program.uniform("u_point_radius_max", point_psize_max);
  }

  void ViewportDrawTextureTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources 
    auto &e_arcball           = info.get_resource<detail::Arcball>("viewport_input", "arcball");
    auto &e_validation_buffer = info.get_resource<gl::Buffer>("validate_spectral_texture", "validation_buffer");
    auto &e_error_buffer      = info.get_resource<gl::Buffer>("error_viewer", "color_buffer");

    // Declare scoped OpenGL state
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,       true),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest,  true) };
    
    // Update varying program uniforms
    eig::Matrix4f camera_matrix = e_arcball.full().matrix();
    eig::Vector2f camera_aspect = { 1.f, e_arcball.m_aspect };
    m_program.uniform("u_camera_matrix", camera_matrix);
    m_program.uniform("u_billboard_aspect", camera_aspect);

    // Bind resources to buffer targets
    e_error_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    // e_validation_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    
    // Submit draw information
    gl::dispatch_draw(m_draw);
  }
} // namespace met