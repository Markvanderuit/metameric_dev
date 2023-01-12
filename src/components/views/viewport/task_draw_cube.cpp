#include <metameric/core/data.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/viewport/task_draw_cube.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/utility.hpp>

namespace met {
  constexpr std::array<float, 3 * 8> verts = {
    0.f, 0.f, 0.f,
    0.f, 0.f, 1.f,
    0.f, 1.f, 0.f,
    0.f, 1.f, 1.f,
    1.f, 0.f, 0.f,
    1.f, 0.f, 1.f,
    1.f, 1.f, 0.f,
    1.f, 1.f, 1.f
  };

  constexpr std::array<uint, 30> elems = {
    0, 1, 0, 2,
    1, 3, 2, 3,

    0, 4, 1, 5,
    2, 6, 3, 7,

    4, 5, 4, 6,
    5, 7, 6, 7
  };

  ViewportDrawCubeTask::ViewportDrawCubeTask(const std::string &name)
  : detail::AbstractTask(name, true) { }

  void ViewportDrawCubeTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_appl_data       = info.get_resource<ApplicationData>(global_key, "app_data");

    // Setup program for instanced billboard point draw
    m_program = {{ .type = gl::ShaderType::eVertex,   .path = "resources/shaders/viewport/draw_color_uniform.vert" },
                 { .type = gl::ShaderType::eFragment, .path = "resources/shaders/viewport/draw_color.frag" }};

    // Setup objects for instanced quad draw
    m_vert_buffer = {{ .data = cnt_span<const std::byte>(verts) }};
    m_elem_buffer = {{ .data = cnt_span<const std::byte>(elems) }};
    m_array = {{
      .buffers = {{ .buffer = &m_vert_buffer,  .index = 0, .stride = 3 * sizeof(float) }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
      .elements = &m_elem_buffer
    }};
    m_draw = {
      .type             = gl::PrimitiveType::eLines,
      .vertex_count     = elems.size(),
      .bindable_array   = &m_array,
      .bindable_program = &m_program
    };

    eig::Array4f clear_colr = e_appl_data.color_mode == AppColorMode::eDark
                            ? 1
                            : eig::Array4f { 0, 0, 0, 1 };
                            
    // Set constant uniforms
    m_program.uniform("u_model_matrix", eig::Matrix4f::Identity().eval());
    m_program.uniform("u_value",      clear_colr);
  }

  void ViewportDrawCubeTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources 
    auto &e_arcball = info.get_resource<detail::Arcball>("viewport_input", "arcball");

    // Declare scoped OpenGL state
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,       true),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest,  true),
                               gl::state::ScopedSet(gl::DrawCapability::eLineSmooth, true) };
    
    // Update varying program uniforms
    m_program.uniform("u_camera_matrix", e_arcball.full().matrix());

    // Submit draw information
    gl::dispatch_draw(m_draw);
  }
} // namespace met