#include <metameric/core/data.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/viewport/task_draw_cube.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/utility.hpp>

namespace met {
  // Buffer flags for flushable, persistent, write-only mapping
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

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

  void ViewportDrawCubeTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();

    // Setup program for instanced billboard point draw
    m_program = {{ .type = gl::ShaderType::eVertex,   
                   .spirv_path = "resources/shaders/viewport/draw_color_uniform.vert.spv",
                   .cross_path = "resources/shaders/viewport/draw_color_uniform.vert.json" },
                 { .type = gl::ShaderType::eFragment, 
                   .spirv_path = "resources/shaders/viewport/draw_color.frag.spv",
                   .cross_path = "resources/shaders/viewport/draw_color.frag.json" }};

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

    // Setup uniform buffer structure
    m_unif_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_unif_map    = m_unif_buffer.map_as<UniformBuffer>(buffer_access_flags).data();
    m_unif_map->model_matrix = eig::Matrix4f::Identity();
    m_unif_map->color_value = e_appl_data.color_mode == ApplicationData::ColorMode::eDark
                            ? 1
                            : eig::Vector4f { 0, 0, 0, 1 };
  }

  void ViewportDrawCubeTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources 
    const auto &e_arcball    = info("viewport.input", "arcball").read_only<detail::Arcball>();
    const auto &e_view_state = info("state", "viewport_state").read_only<ViewportState>();

    // Declare scoped OpenGL state
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,       true),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest,  true),
                               gl::state::ScopedSet(gl::DrawCapability::eLineSmooth, true) };
    
    // Update varying program uniforms
    if (e_view_state.camera_matrix) {
      m_unif_map->camera_matrix = e_arcball.full().matrix();
      m_unif_buffer.flush();
    }

    // Bind resources and submit draw information
    m_program.bind("m_uniform", m_unif_buffer);
    gl::dispatch_draw(m_draw);
  }
} // namespace met