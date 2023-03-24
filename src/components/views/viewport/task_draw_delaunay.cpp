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
  constexpr uint init_vert_support = 1024;
  constexpr uint init_elem_support = 1024;

  void ViewportDrawDelaunayTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    const auto &e_appl_data   = info.global("app_data").read_only<ApplicationData>();
    const auto &e_proj_data   = e_appl_data.project_data;
    const auto &e_vert_buffer = info.resource("gen_spectral_data", "vert_buffer").read_only<gl::Buffer>();

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

  void ViewportDrawDelaunayTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_pipe_state   = info.resource("state", "pipeline_state").read_only<ProjectState>();
    const auto &e_view_state   = info.resource("state", "viewport_state").read_only<ViewportState>();
    const auto &e_frame_buffer = info.resource("viewport.draw_begin", "frame_buffer").read_only<gl::Framebuffer>();
    const auto &e_arcball      = info.resource("viewport.input", "arcball").read_only<detail::Arcball>();
    const auto &e_appl_data    = info.global("app_data").read_only<ApplicationData>();
    const auto &e_proj_data    = e_appl_data.project_data;
    const auto &e_vert_buffer  = info.resource("gen_spectral_data", "vert_buffer").read_only<gl::Buffer>();

    // On relevant state change, update mesh buffer data
    if (e_pipe_state.any_verts) {
      // Resize fixed-size buffer if current available size is exceeded
      if (e_pipe_state.verts.size() > m_size_map.size()) {
        std::vector<float> size_init(2 * e_pipe_state.verts.size(), vert_deslct_size);
        m_size_buffer = {{ .data = cnt_span<const std::byte>(size_init), .flags = buffer_create_flags }};
        m_size_map    = m_size_buffer.map_as<float>(buffer_access_flags);
      }

      // Generate triangulated mesh over tetrahedral delaunay structure
      const auto &e_delaunay = info.resource("gen_spectral_data", "delaunay").read_only<AlignedDelaunayData>();
      auto trimesh = convert_mesh<AlignedMeshData>(convert_mesh<IndexedDelaunayData>(e_delaunay));

      // Resize fixed-size element buffer if current available size is exceeded
      if (trimesh.elems.size() > m_elem_map.size()) {
        m_elem_array.detach_elements();
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
      const auto &e_vert_select = info.resource("viewport.input.vert", "selection").read_only<std::vector<uint>>();
      const auto &e_vert_msover = info.resource("viewport.input.vert", "mouseover").read_only<std::vector<uint>>();
      
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