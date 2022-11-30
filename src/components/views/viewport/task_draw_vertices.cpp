#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/viewport/task_draw_vertices.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met {
  constexpr float unselected_psize = 0.005f;
  constexpr float selected_psize   = 0.02f;

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

  ViewportDrawVerticesTask::ViewportDrawVerticesTask(const std::string &name)
  : detail::AbstractTask(name, true) { }

  void ViewportDrawVerticesTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_gamut_buffer = info.get_resource<gl::Buffer>("gen_spectral_gamut", "buffer_colr");

    // Setup program for instanced billboard point draw
    m_program = {{ .type = gl::ShaderType::eVertex,   .path = "resources/shaders/viewport/draw_vertices.vert" },
                 { .type = gl::ShaderType::eFragment, .path = "resources/shaders/viewport/draw_vertices.frag" }};

    // Declare buffer flags for persistent, write-only, flushable mapping
    auto create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
    auto map_flags    = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;

    // Setup sizes buffer using mapping flags
    std::vector<float> input_sizes(16, unselected_psize);
    m_size_buffer = {{ .data = cnt_span<const std::byte>(input_sizes), .flags = create_flags }};
    info.insert_resource("size_map", cast_span<float>(m_size_buffer.map(map_flags)));

    // Setup objects for instanced quad draw
    m_vert_buffer = {{ .data = cnt_span<const std::byte>(verts) }};
    m_elem_buffer = {{ .data = cnt_span<const std::byte>(elems) }};
    m_array = {{
      .buffers = {{ .buffer = &m_vert_buffer,  .index = 0, .stride = 2 * sizeof(float), .divisor = 0 },
                  { .buffer = &e_gamut_buffer, .index = 1, .stride = 4 * sizeof(float), .divisor = 1 },
                  { .buffer = &m_size_buffer,  .index = 2, .stride = sizeof(float),     .divisor = 1 }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e2 },
                  { .attrib_index = 1, .buffer_index = 1, .size = gl::VertexAttribSize::e3 },
                  { .attrib_index = 2, .buffer_index = 2, .size = gl::VertexAttribSize::e1 }},
      .elements = &m_elem_buffer
    }};
    m_draw = {
      .type             = gl::PrimitiveType::eTriangles,
      .vertex_count     = elems.size(),
      .instance_count   = 5,
      .bindable_array   = &m_array,
      .bindable_program = &m_program
    };

    m_gamut_buffer_cache = e_gamut_buffer.object();
  }

  void ViewportDrawVerticesTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();

    m_size_buffer.unmap();
  }

  void ViewportDrawVerticesTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources 
    auto &e_arcball         = info.get_resource<detail::Arcball>("viewport_input", "arcball");
    auto &i_size_map        = info.get_resource<std::span<float>>("size_map");
    auto &e_gamut_buffer    = info.get_resource<gl::Buffer>("gen_spectral_gamut", "buffer_colr");
    auto &e_gamut_selection = info.get_resource<std::vector<uint>>("viewport_input", "gamut_selection");

    // Update array object in case gamut buffer was resized
    if (m_gamut_buffer_cache != e_gamut_buffer.object()) {
      m_gamut_buffer_cache = e_gamut_buffer.object();
      m_array.attach_buffer({{ .buffer = &e_gamut_buffer, .index = 1, .stride = sizeof(eig::AlArray3f), .divisor = 1 }});
    }

    // Update point size data based on selected vertices
    std::ranges::for_each(i_size_map, [](float &f) { f = unselected_psize; });
    std::ranges::for_each(e_gamut_selection, [&](uint i) { i_size_map[i] = selected_psize; });
    m_size_buffer.flush();

    // Declare scoped OpenGL state
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,       true),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest,  true) };
    
    // Update program uniforms
    eig::Matrix4f camera_matrix = e_arcball.full().matrix();
    eig::Vector2f camera_aspect = { 1.f, e_arcball.m_aspect };
    m_program.uniform("u_camera_matrix", camera_matrix);
    m_program.uniform("u_billboard_aspect", camera_aspect);

    // Submit draw information
    gl::dispatch_draw(m_draw);
  }
} // namespace met