#include <metameric/core/data.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/viewport/task_draw_color_solid.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;

  // Mesh data for instanced billboard quad draw
  constexpr float quad_vert_size = .01f;
  constexpr std::array<float, 2 * 4> quad_vert_data = { -1.f, -1.f, 1.f, -1.f, 1.f,  1.f, -1.f,  1.f };
  constexpr std::array<uint, 2 * 3>  quad_elem_data = { 0, 1, 2, 2, 3, 0 };

  DrawColorSolidTask::DrawColorSolidTask(const std::string &parent)
  : m_parent(parent) { }
    
  void DrawColorSolidTask::init(SchedulerHandle &info) { 
    met_trace_full();

    // Get external resources
    const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();

    // Generate a uv sphere mesh to get an upper bound for convex hull buffer sizes
    auto sphere_mesh = generate_spheroid<HalfedgeMeshData>(3);

    // Allocate convex hull buffer objects with predetermined maximum sizes
    m_chull_verts = {{ .size = sphere_mesh.n_vertices() * sizeof(eig::AlArray3f), .flags = gl::BufferCreateFlags::eStorageDynamic }};
    m_chull_elems = {{ .size = sphere_mesh.n_faces() * sizeof(eig::Array3u), .flags = gl::BufferCreateFlags::eStorageDynamic }};

    // Allocate buffer objects for billboard quad draw
    m_quad_verts = {{ .data = cnt_span<const std::byte>(quad_vert_data) }};
    m_quad_elems = {{ .data = cnt_span<const std::byte>(quad_elem_data) }};

    // Create array objects for convex hull mesh draw and straightforward point draw
    m_cnstr_array = {{
      .buffers = {{ .buffer = &m_quad_verts, .index = 0, .stride = 2 * sizeof(float) }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e2 }},
      .elements = &m_quad_elems
    }};
    m_chull_array = {{
      .buffers = {{ .buffer = &m_chull_verts, .index = 0, .stride = sizeof(AlColr) }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
      .elements = &m_chull_elems
    }};
    m_point_array = {{
      .buffers = {{ .buffer = &m_chull_verts, .index = 0, .stride = sizeof(AlColr) }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }}
    }};

    // Load shader program objects
    m_cnstr_program = {{ .type       = gl::ShaderType::eVertex,   
                         .spirv_path = "resources/shaders/views/draw_point.vert.spv",
                         .cross_path = "resources/shaders/views/draw_point.vert.json" },
                       { .type       = gl::ShaderType::eFragment, 
                         .spirv_path = "resources/shaders/views/draw_point.frag.spv",
                         .cross_path = "resources/shaders/views/draw_point.frag.json" }};
    m_draw_program = {{ .type       = gl::ShaderType::eVertex,   
                        .spirv_path = "resources/shaders/views/draw_csys.vert.spv",
                        .cross_path = "resources/shaders/views/draw_csys.vert.json" },
                      { .type       = gl::ShaderType::eFragment, 
                        .spirv_path = "resources/shaders/views/draw_csys.frag.spv",
                        .cross_path = "resources/shaders/views/draw_csys.frag.json" }};
    m_srgb_program = {{ .type       = gl::ShaderType::eCompute,  
                        .glsl_path  = "resources/shaders/misc/texture_resample.comp",
                        .cross_path = "resources/shaders/misc/texture_resample.comp.json" }};

    // Create dispatch objects to summarize draw/compute operations
    m_cnstr_dispatch = { .type             = gl::PrimitiveType::eTriangles,
                         .vertex_count     = quad_elem_data.size(),
                         .bindable_array   = &m_cnstr_array,
                         .bindable_program = &m_cnstr_program };
    m_chull_dispatch = { .type             = gl::PrimitiveType::eTriangles,
                         .vertex_count     = (uint) (m_chull_elems.size() / sizeof(uint)),
                         .bindable_array   = &m_chull_array,
                         .bindable_program = &m_draw_program };
    m_point_dispatch = { .type             = gl::PrimitiveType::ePoints,
                         .vertex_count     = (uint) (m_chull_verts.size() / sizeof(eig::AlArray3f)),
                         .bindable_array   = &m_point_array,
                         .bindable_program = &m_draw_program };
    m_srgb_dispatch = { .bindable_program = &m_srgb_program };

    m_cnstr_uniform_buffer = {{ .size = sizeof(CnstrUniformBuffer), .flags = buffer_create_flags }};
    m_cnstr_uniform_map    = m_cnstr_uniform_buffer.map_as<CnstrUniformBuffer>(buffer_access_flags).data();
    m_cnstr_uniform_map->point_size  = quad_vert_size;
    m_cnstr_uniform_map->point_color = e_appl_data.color_mode == ApplicationData::ColorMode::eDark
                                     ? 1
                                     : eig::Vector4f { 0, 0, 0, 1 };

    m_draw_uniform_buffer = {{ .size = sizeof(DrawUniformBuffer), .flags = buffer_create_flags }};
    m_draw_uniform_map    = m_draw_uniform_buffer.map_as<DrawUniformBuffer>(buffer_access_flags).data();
    m_draw_uniform_map->alpha = 1.f;

    // Create sampler object used in gamma correction step
    // Instantiate objects for gamma correction step
    m_srgb_sampler = {{ .min_filter = gl::SamplerMinFilter::eNearest, .mag_filter = gl::SamplerMagFilter::eNearest }};
    m_srgb_uniform_buffer = {{ .size = sizeof(SrgbUniformBuffer), .flags = buffer_create_flags }};
    m_srgb_uniform_map    = m_srgb_uniform_buffer.map_as<SrgbUniformBuffer>(buffer_access_flags).data();
    m_srgb_uniform_map->lrgb_to_srgb = true;
  }

  bool DrawColorSolidTask::is_active(SchedulerHandle &info) {
    met_trace();

    // Verify that vertex and constraint are selected before continuing, as this draw operation
    // is otherwise not even visible
    const auto &e_vert_slct = info.resource("viewport.input.vert", "selection").read_only<std::vector<uint>>();
    const auto &e_cstr_slct = info.resource("viewport.overlay", "constr_selection").read_only<int>();
    return e_vert_slct.size() == 1 && e_cstr_slct != -1;
  }

  void DrawColorSolidTask::eval(SchedulerHandle &info) { 
    met_trace_full();

    // Get external resources
    const auto &e_vert_slct  = info.resource("viewport.input.vert", "selection").read_only<std::vector<uint>>();
    const auto &e_cstr_slct  = info.resource("viewport.overlay", "constr_selection").read_only<int>();
    const auto &e_appl_data  = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data  = e_appl_data.project_data;
    const auto &e_vert       = e_proj_data.verts[e_vert_slct[0]];
    const auto &e_proj_state = info.resource("state", "proj_state").read_only<ProjectState>();
    const auto &e_view_state = info.resource("state", "view_state").read_only<ViewportState>();
    const auto &e_arcball    = info.resource(m_parent, "arcball").read_only<detail::Arcball>();
    const auto &e_csol_cntr  = info.resource("gen_mismatch_solid", "chull_cntr").read_only<Colr>();

    // Get modified resources
    auto &e_lrgb_target = info.resource(m_parent, "lrgb_color_solid_target").writeable<gl::Texture2d4f>();
    auto &e_srgb_target = info.resource(m_parent, "srgb_color_solid_target").writeable<gl::Texture2d4f>();

    // (Re-)create framebuffers. Multisampled framebuffer uses multisampled renderbuffers as 
    // attachments, while the non-multisampled framebuffer targets the lrgb texture for output;
    // this way we draw into the ms framebuffer, and then blit into the regular texture target after
    if (!m_frame_buffer.is_init() || (e_lrgb_target.size() != m_color_buffer_ms.size()).any()) {
      m_color_buffer_ms = {{ .size = e_lrgb_target.size().max(1) }};
      m_depth_buffer_ms = {{ .size = e_lrgb_target.size().max(1) }};
      m_frame_buffer_ms = {{ .type = gl::FramebufferType::eColor, .attachment = &m_color_buffer_ms },
                           { .type = gl::FramebufferType::eDepth, .attachment = &m_depth_buffer_ms }};
      m_frame_buffer    = {{ .type = gl::FramebufferType::eColor, .attachment = &e_lrgb_target }};
      
      // Additionally, update gamma correction dispatch size to match srgb texture target
      eig::Array2u dispatch_n    = e_srgb_target.size();
      eig::Array2u dispatch_ndiv = ceil_div(e_srgb_target.size(), 16u);
      m_srgb_dispatch.groups_x = dispatch_ndiv.x();
      m_srgb_dispatch.groups_y = dispatch_ndiv.y();
      m_srgb_uniform_map->size = dispatch_n;
      m_srgb_uniform_buffer.flush();
    }

    // Stream data to vertex array if mesh data has changed; this change is on-line, so
    // we copy to existing buffers
    if (auto rsrc = info.resource("gen_mismatch_solid", "chull_mesh"); rsrc.is_mutated()) {
      const auto &[verts, elems, _norms, _uvs] = rsrc.read_only<AlignedMeshData>();
      if (verts.empty()) {
        m_chull_dispatch.vertex_count = 0;
        m_point_dispatch.vertex_count = 0;
      } else {
        // Copy data to buffers and adjust dispatch settings as the mesh may be smaller
        m_chull_verts.set(cnt_span<const std::byte>(verts), verts.size() * sizeof(decltype(verts)::value_type));
        m_chull_elems.set(cnt_span<const std::byte>(elems), elems.size() * sizeof(decltype(elems)::value_type));
        m_chull_dispatch.vertex_count = elems.size() * 3;
        m_point_dispatch.vertex_count = verts.size();
      }
    }

    eig::Array4f clear_colr = e_appl_data.color_mode == ApplicationData::ColorMode::eDark
                            ? eig::Array4f { 0, 0, 0, 1 } 
                            : ImGui::GetStyleColorVec4(ImGuiCol_ChildBg);
                            
    // Clear multisampled framebuffer and bind it for the coming draw operations
    m_frame_buffer_ms.clear(gl::FramebufferType::eColor, clear_colr);
    m_frame_buffer_ms.clear(gl::FramebufferType::eDepth, 1.f);
    m_frame_buffer_ms.bind();

    // Set OpenGL state for coming draw operations
    gl::state::set_viewport(m_color_buffer_ms.size());
    gl::state::set_point_size(8.f);
    gl::state::set_op(gl::CullOp::eBack);
    gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,      true),
                               gl::state::ScopedSet(gl::DrawCapability::eBlendOp,   true) };
    
    // Set model/camera translations to center viewport on convex hull
    eig::Affine3f transl(eig::Translation3f(-e_csol_cntr.matrix().eval()));
    m_draw_uniform_map->model_matrix = transl.matrix();
    m_draw_uniform_map->camera_matrix = e_arcball.full().matrix();
    m_draw_uniform_buffer.flush();
    m_draw_program.bind("b_uniform", m_draw_uniform_buffer);

    // Dispatch line draw of the full mesh, such that it is etched over the entire structure
    {
      // Capabilities set such that the framework is drawn over the entire mesh
      auto draw_capabilities_ = { gl::state::ScopedSet(gl::DrawCapability::eCullOp,    false),
                                  gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true) };
                                  
      gl::state::set_op(gl::DrawOp::eLine);
      gl::dispatch_draw(m_chull_dispatch);
    }

    // Dispatch mesh and point draw operations as follows;
    // 1. Do a line draw of the full mesh
    // 2. Do a point draw of the mesh vertices
    {
      // Capabilities set such that the front of the mesh is the only part that is drawn, and the back is blended in
      auto draw_capabilities_ = { gl::state::ScopedSet(gl::DrawCapability::eCullOp,    true),
                                  gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true) };
                                
      gl::state::set_op(gl::DrawOp::eFill);
      gl::dispatch_draw(m_chull_dispatch);
      gl::dispatch_draw(m_point_dispatch);
    }

    // Dispatch point draw for the current constraint's positione
    {
      // Capabilities set such that the framework is drawn over the entire mesh
      auto draw_capabilities_ = { gl::state::ScopedSet(gl::DrawCapability::eCullOp,    false),
                                  gl::state::ScopedSet(gl::DrawCapability::eDepthTest, false) };

      // Update uniform data for upcoming draw
      m_cnstr_uniform_map->model_matrix   = transl.matrix();
      m_cnstr_uniform_map->camera_matrix  = e_arcball.full().matrix();
      m_cnstr_uniform_map->point_position = e_vert.colr_j[e_cstr_slct];
      m_cnstr_uniform_map->point_aspect   = { 1.f, e_arcball.m_aspect };
      m_cnstr_uniform_buffer.flush();

      m_cnstr_program.bind("b_uniform", m_cnstr_uniform_buffer);
      gl::dispatch_draw(m_cnstr_dispatch);
    }

    // Given the previous draws into the multisampled framebuffer are complete, now blit into
    // the non-multisampled framebuffer, and therefore into the lrgb texture target.
    gl::sync::memory_barrier(gl::BarrierFlags::eFramebuffer);
    m_frame_buffer_ms.blit_to(m_frame_buffer, 
      e_lrgb_target.size(), 0u, e_lrgb_target.size(), 0u, 
      gl::FramebufferMaskFlags::eColor | gl::FramebufferMaskFlags::eDepth);

    // Bind relevant resources to texture/image/sampler units for the coming compute operation
    m_srgb_program.bind("b_uniform", m_srgb_uniform_buffer);
    m_srgb_program.bind("s_image_r", m_srgb_sampler);
    m_srgb_program.bind("s_image_r", e_lrgb_target);
    m_srgb_program.bind("i_image_w", e_srgb_target);

    // Dispatch gamma correction compute operation
    gl::dispatch_compute(m_srgb_dispatch);
  }
} // namespace met