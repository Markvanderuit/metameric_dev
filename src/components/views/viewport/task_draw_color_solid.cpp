#include <metameric/core/data.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/viewport/task_draw_color_solid.hpp>

namespace met {
  constexpr uint n_sphere_subdivs  = 4;

  DrawColorSolidTask::DrawColorSolidTask(const std::string &name, const std::string &parent)
  : detail::AbstractTask(name, true),
    m_parent(parent) { }
    
  void DrawColorSolidTask::init(detail::TaskInitInfo &info) { 
    met_trace_full();

    // Generate a uv sphere mesh to get an upper bound for convex hull buffer sizes
    m_sphere_mesh = generate_spheroid<HalfedgeMeshTraits>(n_sphere_subdivs);

    // Allocate buffer objects with predetermined maximum sizes
    m_chull_verts = {{ .size = m_sphere_mesh.n_vertices() * sizeof(eig::AlArray3f), .flags = gl::BufferCreateFlags::eStorageDynamic }};
    m_chull_elems = {{ .size = m_sphere_mesh.n_faces() * sizeof(eig::Array3u), .flags = gl::BufferCreateFlags::eStorageDynamic }};

    // Create array objects for convex hull mesh draw and straightforward point draw
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
    m_draw_program = {{ .type = gl::ShaderType::eVertex,   .path = "resources/shaders/viewport/draw_color_array.vert" },
                      { .type = gl::ShaderType::eFragment, .path = "resources/shaders/viewport/draw_color_uniform_alpha.frag" }};
    m_srgb_program = {{ .type = gl::ShaderType::eCompute,  .path = "resources/shaders/misc/texture_resample.comp" }};

    // Create dispatch objects to summarize draw/compute operations
    m_chull_dispatch = { .type = gl::PrimitiveType::eTriangles,
                         .vertex_count = (uint) (m_chull_elems.size() / sizeof(uint)),
                         .bindable_array = &m_chull_array,
                         .bindable_program = &m_draw_program };
    m_point_dispatch = { .type = gl::PrimitiveType::ePoints,
                         .vertex_count = (uint) (m_chull_verts.size() / sizeof(eig::AlArray3f)),
                         .bindable_array = &m_point_array,
                         .bindable_program = &m_draw_program };
    m_srgb_dispatch = { .bindable_program = &m_srgb_program };

    // Create sampler object used in gamma correction step
    m_srgb_sampler = {{ .min_filter = gl::SamplerMinFilter::eNearest, .mag_filter = gl::SamplerMagFilter::eNearest }};

    // Set these uniforms once
    m_draw_program.uniform("u_alpha", 1.f);
    m_srgb_program.uniform("u_sampler", 0);
    m_srgb_program.uniform("u_lrgb_to_srgb", true);
  }

  void DrawColorSolidTask::eval(detail::TaskEvalInfo &info) { 
    met_trace_full();
  
    // Verify that vertex and constraint are selected before continuing, as this draw operation
    // is otherwise not even visible
    auto &e_vert_slct = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection");
    auto &e_cstr_slct = info.get_resource<int>("viewport_overlay", "constr_selection");
    guard(e_vert_slct.size() == 1 && e_cstr_slct >= 0);

    // Get shared resources
    auto &e_appl_data   = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_pipe_state  = info.get_resource<ProjectState>("state", "pipeline_state");
    auto &e_view_state  = info.get_resource<ViewportState>("state", "viewport_state");
    auto &e_lrgb_target = info.get_resource<gl::Texture2d4f>(m_parent, "lrgb_color_solid_target");
    auto &e_srgb_target = info.get_resource<gl::Texture2d4f>(m_parent, "srgb_color_solid_target");
    auto &e_arcball     = info.get_resource<detail::Arcball>(m_parent, "arcball");
    auto &e_csol_cntr   = info.get_resource<Colr>("gen_color_solids", "csol_cntr");

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
      m_srgb_program.uniform("u_size", dispatch_n);
    }

    // (Re-)create convex hull mesh data. If the selected vertex/constraint has in any way changed, a new
    // convex hull mesh needs to be computed and uploaded to the chull/point buffers
    if (e_view_state.vert_selection || e_view_state.cstr_selection || e_pipe_state.verts[e_vert_slct[0]].any) {
      // Get color solid data, if available
      auto &e_csol_data = info.get_resource<std::vector<eig::AlArray3f>>("gen_color_solids", "csol_data");
      guard(!e_csol_data.empty());

      // Generate convex hull mesh and convert to buffer format
      m_csolid_mesh = generate_convex_hull<HalfedgeMeshTraits, eig::AlArray3f>(e_csol_data, m_sphere_mesh);
      auto [verts, elems] = generate_data<HalfedgeMeshTraits, eig::AlArray3f>(m_csolid_mesh);

      // Copy data to buffers and adjust dispatch settings as the mesh may be smaller
      m_chull_verts.set(cnt_span<const std::byte>(verts), verts.size() * sizeof(decltype(verts)::value_type));
      m_chull_elems.set(cnt_span<const std::byte>(elems), elems.size() * sizeof(decltype(elems)::value_type));
      m_chull_dispatch.vertex_count = elems.size() * 3;
      m_point_dispatch.vertex_count = verts.size();
    }

    // Clear multisampled framebuffer and bind it for the coming draw operations
    m_frame_buffer_ms.clear(gl::FramebufferType::eColor, eig::Array4f(0, 0, 0, 1));
    m_frame_buffer_ms.clear(gl::FramebufferType::eDepth, 1.f);
    m_frame_buffer_ms.bind();

    // Set OpenGL state for coming draw operations
    gl::state::set_viewport(m_color_buffer_ms.size());
    gl::state::set_point_size(8.f);
    gl::state::set_op(gl::CullOp::eFront);
    gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,      true),
                               gl::state::ScopedSet(gl::DrawCapability::eBlendOp,   true),
                               gl::state::ScopedSet(gl::DrawCapability::eCullOp,    true),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true) };
    
    // Set model/camera translations to center viewport on convex hull
    eig::Affine3f transl(eig::Translation3f(-e_csol_cntr.matrix().eval()));
    m_draw_program.uniform("u_model_matrix",  transl.matrix());
    m_draw_program.uniform("u_camera_matrix", e_arcball.full().matrix());  

    // Dispatch mesh and point draw operations as follows;
    // 1. Do a line draw of the full mesh
    // 2. Do a face draw of the full mesh
    // 3. Do a point draw of the mesh vertices
    gl::state::set_op(gl::DrawOp::eLine);
    gl::dispatch_draw(m_chull_dispatch);
    gl::state::set_op(gl::DrawOp::eFill);
    m_draw_program.uniform("u_alpha", .66f);
    gl::dispatch_draw(m_chull_dispatch);
    m_draw_program.uniform("u_alpha", 1.f);
    gl::dispatch_draw(m_point_dispatch);

    // Given the previous draws into the multisampled framebuffer are complete, now blit into
    // the non-multisampled framebuffer, and therefore into the lrgb texture target.
    gl::sync::memory_barrier(gl::BarrierFlags::eFramebuffer);
    m_frame_buffer_ms.blit_to(m_frame_buffer, 
      e_lrgb_target.size(), 0u, e_lrgb_target.size(), 0u, 
      gl::FramebufferMaskFlags::eColor | gl::FramebufferMaskFlags::eDepth);

    // Bind relevant resources to texture/image/sampler units for the coming compute operation
    e_lrgb_target.bind_to(gl::TextureTargetType::eTextureUnit,    0);
    e_srgb_target.bind_to(gl::TextureTargetType::eImageWriteOnly, 0);
    m_srgb_sampler.bind_to(0);

    // Dispatch gamma correction compute operation
    gl::dispatch_compute(m_srgb_dispatch);
  }
} // namespace met