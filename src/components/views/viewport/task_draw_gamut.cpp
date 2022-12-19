#include <metameric/core/data.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/viewport/task_draw_gamut.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met {
  // Size/opacity settings for vertex/element selection
  constexpr float vert_deslct_size = 0.005f;
  constexpr float vert_select_size = 0.01f;
  constexpr float vert_msover_size = 0.015f;
  constexpr float elem_deslct_opac = 0.05f;
  constexpr float elem_select_opac = 0.1f;
  constexpr float elem_msover_opac = 0.2f;

  // Mesh data for instanced billboard quad draw
  constexpr std::array<float, 2 * 4> inst_vert_data = { -1.f, -1.f, 1.f, -1.f, 1.f,  1.f, -1.f,  1.f };
  constexpr std::array<uint, 2 * 3>  inst_elem_data = { 0, 1, 2, 2, 3, 0 };

  // Buffer flags for flushable, persistent, write-only mapping
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;

  ViewportDrawGamutTask::ViewportDrawGamutTask(const std::string &name)
  : detail::AbstractTask(name, true) { }

  void ViewportDrawGamutTask::init(detail::TaskInitInfo &info) {
    met_trace_full();
    
    // Get shared resources
    auto &e_verts = info.get_resource<gl::Buffer>("gen_spectral_gamut", "vert_buffer");
    auto &e_elems = info.get_resource<gl::Buffer>("gen_spectral_gamut", "elem_buffer_unal");

    // Setup sizes/opacities buffers and instantiate relevant mappings
    std::vector<float> vert_input_sizes(barycentric_weights, vert_deslct_size);
    std::vector<float> elem_input_opacs(barycentric_weights, elem_deslct_opac);
    m_vert_size_buffer = {{ .data = cnt_span<const std::byte>(vert_input_sizes), .flags = buffer_create_flags }};
    m_elem_opac_buffer = {{ .data = cnt_span<const std::byte>(elem_input_opacs), .flags = buffer_create_flags }};
    m_vert_size_map = cast_span<float>(m_vert_size_buffer.map(buffer_access_flags));
    m_elem_opac_map = cast_span<float>(m_elem_opac_buffer.map(buffer_access_flags));

    // Setup buffer objects for instanced billboard quad draw with relevant mesh data
    m_inst_vert_buffer = {{ .data = cnt_span<const std::byte>(inst_vert_data) }};
    m_inst_elem_buffer = {{ .data = cnt_span<const std::byte>(inst_elem_data) }};

    // Setup array objects for (A) instanced quad draw (B) mesh line draw (C) mesh face draw
    m_vert_array = {{
      .buffers = {{ .buffer = &m_inst_vert_buffer,  .index = 0, .stride = 2 * sizeof(float), .divisor = 0 },
                  { .buffer = &e_verts,             .index = 1, .stride = 4 * sizeof(float), .divisor = 1 },
                  { .buffer = &m_vert_size_buffer,  .index = 2, .stride = sizeof(float),     .divisor = 1 }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e2 },
                  { .attrib_index = 1, .buffer_index = 1, .size = gl::VertexAttribSize::e3 },
                  { .attrib_index = 2, .buffer_index = 2, .size = gl::VertexAttribSize::e1 }},
      .elements = &m_inst_elem_buffer
    }};
    m_elem_array =  {{ 
      .buffers = {{ .buffer = &e_verts,  .index = 0, .stride = sizeof(eig::AlArray3f) }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }}
    }};

    // Setup dispatch objects summarizing both draw operations
    m_vert_draw = {
      .type             = gl::PrimitiveType::eTriangles,
      .vertex_count     = inst_elem_data.size(),
      .instance_count   = static_cast<uint>(e_verts.size() / sizeof(eig::AlArray3f)),
      .bindable_array   = &m_vert_array,
      .bindable_program = &m_vert_program
    };
    m_elem_draw = { 
      .type             = gl::PrimitiveType::eTriangles,
      .vertex_count     = static_cast<uint>(e_elems.size() / sizeof(uint)),
      .bindable_array   = &m_elem_array,
      .bindable_program = &m_elem_program 
    };
      
    // Load shader program objects
    m_vert_program = {{ .type = gl::ShaderType::eVertex,   .path = "resources/shaders/viewport/draw_vertices.vert" },
                      { .type = gl::ShaderType::eFragment, .path = "resources/shaders/viewport/draw_vertices.frag" }};
    m_elem_program = {{ .type = gl::ShaderType::eVertex,   .path = "resources/shaders/viewport/draw_gamut.vert" },
                      { .type = gl::ShaderType::eFragment, .path = "resources/shaders/viewport/draw_gamut.frag" }};

    // Set non-changing uniform values
    m_elem_program.uniform("u_model_matrix", eig::Matrix4f::Identity().eval());
    m_elem_program.uniform("u_offset",       .25f);

    m_buffer_object_cache = e_verts.object();
  }

  void ViewportDrawGamutTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();

    if (m_vert_size_buffer.is_init() && m_vert_size_buffer.is_mapped()) m_vert_size_buffer.unmap();
    if (m_elem_opac_buffer.is_init() && m_elem_opac_buffer.is_mapped()) m_elem_opac_buffer.unmap();
  }

  void ViewportDrawGamutTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();
                                
    // Get shared resources 
    auto &e_arcball    = info.get_resource<detail::Arcball>("viewport_input", "arcball");
    auto &e_verts      = info.get_resource<gl::Buffer>("gen_spectral_gamut", "vert_buffer");
    auto &e_elems      = info.get_resource<gl::Buffer>("gen_spectral_gamut", "elem_buffer_unal");
    auto &e_view_state = info.get_resource<ViewportState>("state", "viewport_state");

    // Update array object handles in case gamut buffer was resized (and likely reallocated)
    if (m_buffer_object_cache != e_verts.object()) {
      m_buffer_object_cache = e_verts.object();

      m_vert_array.attach_buffer({{ .buffer = &e_verts, .index = 1, .stride = sizeof(eig::AlArray3f), .divisor = 1 }});
      m_elem_array.attach_buffer({{ .buffer = &e_verts, .index = 0, .stride = sizeof(eig::AlArray3f) }});
      m_elem_array.attach_elements(e_elems);
      
      m_vert_draw.instance_count = static_cast<uint>(e_verts.size() / sizeof(eig::AlArray3f));
      m_elem_draw.vertex_count = static_cast<uint>(e_elems.size() / sizeof(uint));
    }

    // Update size data based on selected vertices, if a state change occurred
    if (e_view_state.vert_selection || e_view_state.vert_mouseover) {
      auto &e_vert_select = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection");
      auto &e_vert_msover = info.get_resource<std::vector<uint>>("viewport_input_vert", "mouseover");

      std::ranges::fill(m_vert_size_map, vert_deslct_size);
      std::ranges::for_each(e_vert_msover, [&](uint i) { m_vert_size_map[i] = vert_msover_size; });
      std::ranges::for_each(e_vert_select, [&](uint i) { m_vert_size_map[i] = vert_select_size; });
      m_vert_size_buffer.flush();
    }

    // Update opacity data based on selected elements, if a state change occurred
    if (e_view_state.elem_selection || e_view_state.elem_mouseover) {
      auto &e_elem_select = info.get_resource<std::vector<uint>>("viewport_input_elem", "selection");
      auto &e_elem_msover = info.get_resource<std::vector<uint>>("viewport_input_elem", "mouseover");

      std::ranges::fill(m_elem_opac_map, elem_deslct_opac);
      std::ranges::for_each(e_elem_msover, [&](uint i) { m_elem_opac_map[i] = elem_msover_opac; });
      std::ranges::for_each(e_elem_select, [&](uint i) { m_elem_opac_map[i] = elem_select_opac; });
      m_elem_opac_buffer.flush();
    }
    
    // Update relevant program uniforms for coming draw operations
    eig::Matrix4f camera_matrix = e_arcball.full().matrix();
    eig::Vector2f camera_aspect = { 1.f, e_arcball.m_aspect };
    m_vert_program.uniform("u_billboard_aspect", camera_aspect);
    m_vert_program.uniform("u_camera_matrix", camera_matrix);
    m_elem_program.uniform("u_camera_matrix", camera_matrix);

    // Set OpenGL state for coming draw operations
    gl::state::set_op(gl::CullOp::eFront);
    gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,      true),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest, false),
                               gl::state::ScopedSet(gl::DrawCapability::eCullOp,    false),
                               gl::state::ScopedSet(gl::DrawCapability::eBlendOp,   true) };

    // Handle element draw; cull front faces
    {
      auto scoped_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eCullOp, true) };
      m_elem_opac_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0u);
      m_elem_program.uniform("u_use_opacity", true);
      gl::dispatch_draw(m_elem_draw);
    }

    // Handle edge draw
    m_elem_program.uniform("u_use_opacity", false);
    gl::state::set_op(gl::DrawOp::eLine);
    gl::dispatch_draw(m_elem_draw);

    // Handle vertex draw
    gl::state::set_op(gl::DrawOp::eFill);
    gl::dispatch_draw(m_vert_draw);
  }
} // namespace met