#include <metameric/core/data.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/viewport/task_draw_delaunay.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/utility.hpp>

namespace met {
  // Size settings for vertex selection
  constexpr float vert_deslct_size = 0.015f;
  constexpr float vert_select_size = 0.030f;
  constexpr float vert_msover_size = 0.045f;

  // Buffer flags for flushable, persistent, write-only mapping
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;

  // Initial allocation sizes for vertex/element dependent buffers
  constexpr uint init_vert_support = 64;
  constexpr uint init_elem_support = 64;

  ViewportDrawDelaunayTask::ViewportDrawDelaunayTask(const std::string &name)
  : detail::AbstractTask(name, true) { }

  void ViewportDrawDelaunayTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_appl_data   = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data   = e_appl_data.project_data;
    auto &e_vert_buffer = info.get_resource<gl::Buffer>("gen_spectral_data", "vert_buffer");

    // Setup mapped buffer objects
    std::vector<float> size_init(init_vert_support, vert_deslct_size);
    m_size_buffer = {{ .data = cnt_span<const std::byte>(size_init), .flags = buffer_create_flags }};
    m_size_map    = m_size_buffer.map_as<float>(buffer_access_flags);
    m_unif_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_unif_map    = m_unif_buffer.map_as<UniformBuffer>(buffer_access_flags).data();
    m_elem_buffer = {{ .size = init_elem_support * sizeof(eig::Array3u), .flags = buffer_create_flags }};
    m_elem_map    = m_elem_buffer.map_as<eig::Array3u>(buffer_access_flags);

    // Setup array objects for (A) instanced quad draw and (B) mesh line draw
    m_vert_array = {{ }};
    m_elem_array = {{
      .buffers  = {{ .buffer = &e_vert_buffer,  .index = 0, .stride = sizeof(eig::AlArray3f) }},
      .attribs  = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
      .elements = &m_elem_buffer
    }};

    // Setup dispatch objects summarizing both draw operations
    m_vert_draw = {
      .type             = gl::PrimitiveType::eTriangles,
      .vertex_count     = 3 * static_cast<uint>(e_proj_data.vertices.size()),
      .capabilities     = {{ gl::DrawCapability::eMSAA, false }},
      .draw_op          = gl::DrawOp::eFill,
      .bindable_array   = &m_vert_array,
      .bindable_program = &m_vert_program
    };
    m_elem_draw = { 
      .type             = gl::PrimitiveType::eTriangles,
      .capabilities     = {{ gl::DrawCapability::eCullOp, false }},
      .draw_op          = gl::DrawOp::eLine,
      .bindable_array   = &m_elem_array,
      .bindable_program = &m_elem_program 
    };

    // Load shader program objects
    m_vert_program = {{ .type = gl::ShaderType::eVertex,   .path = "resources/shaders/viewport/draw_delaunay_vert.vert" },
                      { .type = gl::ShaderType::eFragment, .path = "resources/shaders/viewport/draw_delaunay_vert.frag" }};
    m_elem_program = {{ .type = gl::ShaderType::eVertex,   .path = "resources/shaders/viewport/draw_delaunay_elem.vert" },
                      { .type = gl::ShaderType::eFragment, .path = "resources/shaders/viewport/draw_delaunay_elem.frag" }};

    eig::Array4f clear_colr = e_appl_data.color_mode == AppColorMode::eDark
                            ? eig::Array4f { 1, 1, 1, 1 }
                            : eig::Array4f { 0, 0, 0, 1 };

    // Set non-changing uniform values
    m_vert_program.uniform("u_value", clear_colr);
  }

  void ViewportDrawDelaunayTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();

    if (m_size_buffer.is_init() && m_size_buffer.is_mapped()) 
      m_size_buffer.unmap();
    if (m_unif_buffer.is_init() && m_unif_buffer.is_mapped()) 
      m_unif_buffer.unmap();
    if (m_elem_buffer.is_init() && m_elem_buffer.is_mapped()) 
      m_elem_buffer.unmap();
  }

  void ViewportDrawDelaunayTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_pipe_state   = info.get_resource<ProjectState>("state", "pipeline_state");
    auto &e_view_state   = info.get_resource<ViewportState>("state", "viewport_state");
    auto &e_frame_buffer = info.get_resource<gl::Framebuffer>("viewport_draw_begin", "frame_buffer");
    auto &e_arcball      = info.get_resource<detail::Arcball>("viewport_input", "arcball");
    auto &e_appl_data    = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data    = e_appl_data.project_data;
    auto &e_vert_buffer  = info.get_resource<gl::Buffer>("gen_spectral_data", "vert_buffer");

    // On relevant state change, update mesh buffer data
    if (e_pipe_state.any_verts) {
      // Resize fixed-size buffer if current available size is exceeded
      if (e_pipe_state.verts.size() > m_size_map.size()) {
        m_size_buffer.unmap();
        std::vector<float> size_init(2 * e_pipe_state.verts.size(), vert_deslct_size);
        m_size_buffer = {{ .data = cnt_span<const std::byte>(size_init), .flags = buffer_create_flags }};
        m_size_map    = m_size_buffer.map_as<float>(buffer_access_flags);
      }

      // Generate triangulated mesh over tetrahedral delaunay structure
      auto &e_delaunay = info.get_resource<AlignedDelaunayData>("gen_spectral_data", "delaunay");
      auto trimesh = convert_mesh<AlignedMeshData>(convert_mesh<IndexedDelaunayData>(e_delaunay));

      // Resize fixed-size element buffer if current available size is exceeded
      if (trimesh.elems.size() > m_elem_map.size()) {
        m_elem_array.detach_elements();
        m_elem_buffer.unmap();
        m_elem_buffer = {{ .size = 2 * trimesh.elems.size() * sizeof(eig::Array3u), .flags = buffer_create_flags }};
        m_elem_map    = m_elem_buffer.map_as<eig::Array3u>(buffer_access_flags);
        m_elem_array.attach_elements(m_elem_buffer);  
      }

      std::ranges::copy(trimesh.elems, m_elem_map.begin());
      m_elem_buffer.flush(sizeof(eig::Array3u) * trimesh.elems.size());

      m_vert_draw.vertex_count = 3 * e_proj_data.vertices.size();
      m_elem_draw.vertex_count = 3 * trimesh.elems.size();
    }

    // On relevant state change, update selection buffer data
    if (e_view_state.vert_selection || e_view_state.vert_mouseover) {
      auto &e_vert_select = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection");
      auto &e_vert_msover = info.get_resource<std::vector<uint>>("viewport_input_vert", "mouseover");
      
      std::ranges::fill(m_size_map, vert_deslct_size);
      std::ranges::for_each(e_vert_msover, [&](uint i) { m_size_map[i] = vert_msover_size; });
      std::ranges::for_each(e_vert_select, [&](uint i) { m_size_map[i] = vert_select_size; });
      m_size_buffer.flush();
    }

    // On relevant state change, update uniform buffer data
    if (e_view_state.camera_matrix || e_view_state.camera_aspect) {
      m_unif_map->camera_matrix = e_arcball.full().matrix();
      m_unif_map->camera_aspect = { 1.f, e_arcball.m_aspect };
      m_unif_buffer.flush();
    }

    // Set OpenGL state shared between coming draw operations
    auto shared_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eDepthTest,  true),
                                 gl::state::ScopedSet(gl::DrawCapability::eBlendOp,    true),
                                 gl::state::ScopedSet(gl::DrawCapability::eCullOp,     true),
                                 gl::state::ScopedSet(gl::DrawCapability::eMSAA,       true) };
    
    // Bind buffers to relevant buffer targets
    m_unif_buffer.bind_to(gl::BufferTargetType::eUniform,        0);
    e_vert_buffer.bind_to(gl::BufferTargetType::eShaderStorage,  0);
    m_size_buffer.bind_to(gl::BufferTargetType::eShaderStorage,  1);

    // Dispatch draw calls
    gl::dispatch_draw(m_elem_draw);
    gl::dispatch_draw(m_vert_draw);
  }
} // namespace met