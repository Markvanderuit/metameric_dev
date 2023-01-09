#include <metameric/core/data.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/viewport/task_draw_sample.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met {
  // Size/opacity settings for vertex/element selection
  constexpr float samp_deslct_size = 0.005f;
  constexpr float samp_select_size = 0.01f;
  constexpr float samp_msover_size = 0.015f;

  // Mesh data for instanced billboard quad draw
  constexpr std::array<float, 2 * 4> inst_vert_data = { -1.f, -1.f, 1.f, -1.f, 1.f,  1.f, -1.f,  1.f };
  constexpr std::array<uint, 2 * 3>  inst_elem_data = { 0, 1, 2, 2, 3, 0 };

  // Buffer flags for flushable, persistent, write-only mapping
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;

  // Max number of drawable samples
  constexpr uint max_samp_support = 128; // I really doubt I'll  exceed this number. I mean, c'mon. Really.

  ViewportDrawSampleTask::ViewportDrawSampleTask(const std::string &name)
  : detail::AbstractTask(name, true) { }

  
  void ViewportDrawSampleTask::init(detail::TaskInitInfo &info) {
    met_trace_full();
    
    // Get shared resources
    auto &e_appl_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_samples   = e_appl_data.project_data.sample_verts;
    
    // Setup size/position buffers and instantiate relevant mappings
    std::vector<float> samp_input_sizes(max_samp_support, samp_deslct_size);
    m_samp_posi_buffer = {{ .size = max_samp_support * sizeof(AlColr), .flags = buffer_create_flags }};
    m_samp_size_buffer = {{ .data = cnt_span<const std::byte>(samp_input_sizes), .flags = buffer_create_flags }};
    m_samp_posi_map = cast_span<AlColr>(m_samp_posi_buffer.map(buffer_access_flags));
    m_samp_size_map = cast_span<float>(m_samp_size_buffer.map(buffer_access_flags));

    // Setup buffer objects for instanced billboard quad draw with relevant mesh data
    m_inst_vert_buffer = {{ .data = cnt_span<const std::byte>(inst_vert_data) }};
    m_inst_elem_buffer = {{ .data = cnt_span<const std::byte>(inst_elem_data) }};

    // Setup array object for instanced quad draw
    m_samp_array = {{
      .buffers = {{ .buffer = &m_inst_vert_buffer,  .index = 0, .stride = 2 * sizeof(float), .divisor = 0 },
                  { .buffer = &m_samp_posi_buffer,  .index = 1, .stride = sizeof(AlColr),    .divisor = 1 },
                  { .buffer = &m_samp_size_buffer,  .index = 2, .stride = sizeof(float),     .divisor = 1 }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e2 },
                  { .attrib_index = 1, .buffer_index = 1, .size = gl::VertexAttribSize::e3 },
                  { .attrib_index = 2, .buffer_index = 2, .size = gl::VertexAttribSize::e1 }},
      .elements = &m_inst_elem_buffer
    }};

    // Setup dispatch object summarizing draw operation
    m_samp_draw = {
      .type             = gl::PrimitiveType::eTriangles,
      .vertex_count     = inst_elem_data.size(),
      .instance_count   = static_cast<uint>(e_samples.size()),
      .bindable_array   = &m_samp_array,
      .bindable_program = &m_samp_program
    };

    // Load shader program object
    m_samp_program = {{ .type = gl::ShaderType::eVertex,   .path = "resources/shaders/viewport/draw_vertices.vert" },
                      { .type = gl::ShaderType::eFragment, .path = "resources/shaders/viewport/draw_vertices.frag" }};
   
    eig::Array4f clear_colr = e_appl_data.color_mode == ApplicationColorMode::eDark
                            ? 1
                            : eig::Array4f { 0, 0, 0, 1 };

    // Set non-changing uniform values
    m_samp_program.uniform("u_value", clear_colr);
  }

  void ViewportDrawSampleTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();

    if (m_samp_posi_buffer.is_init() && m_samp_posi_buffer.is_mapped()) m_samp_posi_buffer.unmap();
    if (m_samp_size_buffer.is_init() && m_samp_size_buffer.is_mapped()) m_samp_size_buffer.unmap();
  }

  void ViewportDrawSampleTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources 
    auto &e_arcball    = info.get_resource<detail::Arcball>("viewport_input", "arcball");
    auto &e_view_state = info.get_resource<ViewportState>("state", "viewport_state");
    auto &e_pipe_state = info.get_resource<ProjectState>("state", "pipeline_state");
    auto &e_appl_data  = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_samples    = e_appl_data.project_data.sample_verts;

    // Write updated sample positional data to buffer map and flush
    for (uint i = 0; i < e_pipe_state.samps.size(); ++i) {
      guard_continue(e_pipe_state.samps[i].colr_i);
      m_samp_posi_map[i] = e_samples[i].colr_i;
      m_samp_posi_buffer.flush(sizeof(AlColr), i * sizeof(AlColr));
    }

    // Update opacity data based on selected elements, if a state change occurred
    if (e_view_state.samp_selection || e_view_state.samp_mouseover) {
      auto &e_samp_msover = info.get_resource<std::vector<uint>>("viewport_input_samp", "mouseover");
      auto &e_samp_select = info.get_resource<std::vector<uint>>("viewport_input_samp", "selection");

      std::ranges::fill(m_samp_size_map, samp_deslct_size);
      std::ranges::for_each(e_samp_msover, [&](uint i) { m_samp_size_map[i] = samp_msover_size; });
      std::ranges::for_each(e_samp_select, [&](uint i) { m_samp_size_map[i] = samp_select_size; });
      m_samp_size_buffer.flush();
    }

    guard(!e_samples.empty());
    m_samp_draw.instance_count = static_cast<uint>(e_samples.size());

    // Update relevant program uniforms for coming draw operations
    eig::Matrix4f camera_matrix = e_arcball.full().matrix();
    eig::Vector2f camera_aspect = { 1.f, e_arcball.m_aspect };
    m_samp_program.uniform("u_billboard_aspect", camera_aspect);
    m_samp_program.uniform("u_camera_matrix", camera_matrix);

    // Set OpenGL state for coming draw operations
    gl::state::set_op(gl::CullOp::eBack);
    gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,      true),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest, false),
                               gl::state::ScopedSet(gl::DrawCapability::eCullOp,    false),
                               gl::state::ScopedSet(gl::DrawCapability::eBlendOp,   true) };
    
    // Submit draw information with the correct nr. of instances
    gl::dispatch_draw(m_samp_draw);
  }
}