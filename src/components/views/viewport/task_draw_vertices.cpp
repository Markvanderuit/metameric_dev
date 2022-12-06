#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
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
  constexpr uint  max_vertices     = 16u;
  constexpr float deselected_psize = 0.005f;
  constexpr float mouseover_psize  = 0.015f;
  constexpr float selected_psize   = 0.01f;

  constexpr std::array<float, 2 * 4> verts = { -1.f, -1.f, 1.f, -1.f, 1.f,  1.f, -1.f,  1.f };
  constexpr std::array<uint, 2 * 3>  elems = { 0, 1, 2, 2, 3, 0 };

  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;

  ViewportDrawVerticesTask::ViewportDrawVerticesTask(const std::string &name)
  : detail::AbstractTask(name, true) { }

  void ViewportDrawVerticesTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_app_data     = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data    = e_app_data.project_data;
    auto &e_gamut_buffer = info.get_resource<gl::Buffer>("gen_spectral_gamut", "colr_buffer");

    // Setup program for instanced billboard point draw
    m_program = {{ .type = gl::ShaderType::eVertex,   .path = "resources/shaders/viewport/draw_vertices.vert" },
                 { .type = gl::ShaderType::eFragment, .path = "resources/shaders/viewport/draw_vertices.frag" }};

    // Setup sizes buffer using mapping flags
    std::vector<float> input_sizes(max_vertices, deselected_psize);
    m_size_buffer = {{ .data = cnt_span<const std::byte>(input_sizes), .flags = buffer_create_flags }};
    m_size_map = cast_span<float>(m_size_buffer.map(buffer_access_flags));

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
      .instance_count   = static_cast<uint>(e_proj_data.gamut_colr_i.size()),
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
    auto &e_app_data        = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data       = e_app_data.project_data;
    auto &e_arcball         = info.get_resource<detail::Arcball>("viewport_input", "arcball");
    auto &e_gamut_buffer    = info.get_resource<gl::Buffer>("gen_spectral_gamut", "colr_buffer");
    auto &e_gamut_selection = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection");
    auto &e_gamut_mouseover = info.get_resource<std::vector<uint>>("viewport_input_vert", "mouseover");

    // On state change, update array object as gamut buffer was re-allocated
    if (m_gamut_buffer_cache != e_gamut_buffer.object()) {
      m_array.attach_buffer({{ .buffer = &e_gamut_buffer, .index = 1, .stride = sizeof(eig::AlArray3f), .divisor = 1 }});
      m_draw.instance_count = static_cast<uint>(e_proj_data.gamut_colr_i.size());
      m_gamut_buffer_cache = e_gamut_buffer.object();
    }

    // On state change, update point size data based on selected vertices
    if (!std::ranges::equal(m_gamut_select_cache, e_gamut_selection) || !std::ranges::equal(m_gamut_msover_cache, e_gamut_mouseover)) {
      std::ranges::fill(m_size_map, deselected_psize);
      std::ranges::for_each(e_gamut_mouseover, [&](uint i) { m_size_map[i] = mouseover_psize; });
      std::ranges::for_each(e_gamut_selection, [&](uint i) { m_size_map[i] = selected_psize; });
      m_size_buffer.flush();
      m_gamut_select_cache = e_gamut_selection;
      m_gamut_msover_cache = e_gamut_mouseover;
    }

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