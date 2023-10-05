#include <metameric/core/scene.hpp>
#include <metameric/components/views/mesh_viewport/task_draw_gbuffer.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/misc/detail/scene.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  bool MeshViewportDrawGBufferTask::is_active(SchedulerHandle &info) {
    met_trace();
    return info.relative("viewport_begin")("is_active").getr<bool>()
       && !info.global("scene").getr<Scene>().components.objects.empty();
  }

  void MeshViewportDrawGBufferTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Initialize program object
    m_program = {{ .type       = gl::ShaderType::eVertex,
                   .spirv_path = "resources/shaders/views/draw_mesh_gbuffer.vert.spv",
                   .cross_path = "resources/shaders/views/draw_mesh_gbuffer.vert.json" },
                 { .type       = gl::ShaderType::eFragment,
                   .spirv_path = "resources/shaders/views/draw_mesh_gbuffer.frag.spv",
                   .cross_path = "resources/shaders/views/draw_mesh_gbuffer.frag.json" }};

    // Initialize uniform camera buffer and corresponding mapping
    m_unif_buffer     = {{ .size = sizeof(UnifLayout), .flags = buffer_create_flags }};
    m_unif_buffer_map = m_unif_buffer.map_as<UnifLayout>(buffer_access_flags).data();

    // Initialize draw object
    m_draw = { 
      .type             = gl::PrimitiveType::eTriangles,
      .capabilities     = {/* { gl::DrawCapability::eMSAA,      true }, */
                           { gl::DrawCapability::eDepthTest, true },
                           { gl::DrawCapability::eCullOp,    true }},
      .draw_op          = gl::DrawOp::eFill,
      .bindable_program = &m_program,
    };

    /* 
      G-buffer layout primer
      - normals; 3 components
      - depth; 1 component, non-linear, allows recovery of position
      - UVs; 2 components
      - object_id; 1 component, uint32
     */
    info("gbuffer_norm_dp").set<gl::Texture2d4f>({ }); // normals and linearized depth
    info("gbuffer_txc_idx").set<gl::Texture2d4f>({ }); // txcords and object indices
  }
    
  void MeshViewportDrawGBufferTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get handle to relative task resource
    auto begin_handle = info.relative("viewport_begin");

    // Get shared resources 
    const auto &e_scene     = info.global("scene").getr<Scene>();
    const auto &e_objects   = e_scene.components.objects;
    const auto &e_arcball   = info.relative("viewport_input")("arcball").getr<detail::Arcball>();
    const auto &e_objc_data = info("scene_handler", "objc_data").getr<detail::RTObjectData>();
    const auto &e_mesh_data = info("scene_handler", "mesh_data").getr<detail::RTMeshData>();
    const auto &e_txtr_data = info("scene_handler", "txtr_data").getr<detail::RTTextureData>();
    const auto &e_uplf_data = info("scene_handler", "uplf_data").getr<detail::RTUpliftingData>();
    const auto &e_cmfs_data = info("scene_handler", "cmfs_data").getr<detail::RTObserverData>();
    const auto &e_illm_data = info("scene_handler", "illm_data").getr<detail::RTIlluminantData>();
    const auto &e_csys_data = info("scene_handler", "csys_data").getr<detail::RTColorSystemData>();

    // Output lrgb target provided by viewport task
    const auto &e_lrgb_target = begin_handle("lrgb_target").getr<gl::Texture2d4f>();
    
    // Rebuild framebuffer and g-buffer textures if necessary
    if (!m_fbo.is_init() || (m_fbo_depth.size() != e_lrgb_target.size()).any()) {
      // Get write-handles to g-buffer textures
      auto &i_gbuffer_norm_dp = info("gbuffer_norm_dp").getw<gl::Texture2d4f>();
      auto &i_gbuffer_txc_idx = info("gbuffer_txc_idx").getw<gl::Texture2d4f>();
      
      // Rebuild depth target and g-buffer textures
      m_fbo_depth       = {{ .size = e_lrgb_target.size().max(1).eval() }};
      i_gbuffer_norm_dp = {{ .size = e_lrgb_target.size().max(1).eval() }};
      i_gbuffer_txc_idx = {{ .size = e_lrgb_target.size().max(1).eval() }};

      // Rebuild framebuffer
      m_fbo = {{ .type = gl::FramebufferType::eColor, .index = 0, .attachment = &i_gbuffer_norm_dp },
               { .type = gl::FramebufferType::eColor, .index = 1, .attachment = &i_gbuffer_txc_idx },
               { .type = gl::FramebufferType::eDepth, .index = 0, .attachment = &m_fbo_depth       }};
    }

    // Push camera matrix to uniform data
    m_unif_buffer_map->camera_matrix = e_arcball.full().matrix();
    m_unif_buffer_map->z_near        = e_arcball.m_near_z;
    m_unif_buffer_map->z_far         = e_arcball.m_far_z;
    m_unif_buffer.flush();
    
    // Set fresh vertex array for draw data if it was updated
    if (is_first_eval() || info("scene_handler", "mesh_data").is_mutated())
      m_draw.bindable_array = &e_mesh_data.array;

    // Assemble appropriate draw data for each object in the scene
    if (is_first_eval() || info("scene_handler", "objc_data").is_mutated()) {
      m_draw.commands.resize(e_objects.size());
      rng::transform(e_objects, m_draw.commands.begin(), [&](const auto &comp) {
        guard(comp.value.is_active, gl::MultiDrawInfo::DrawCommand { });
        const auto &e_mesh_info = e_mesh_data.info.at(comp.value.mesh_i);
        return gl::MultiDrawInfo::DrawCommand {
          .vertex_count = e_mesh_info.elems_size * 3,
          .vertex_first = e_mesh_info.elems_offs * 3
        };
      });
    }

    // Prepare OpenGL state
    m_fbo.bind();
    m_fbo.clear(gl::FramebufferType::eColor, eig::Array4f { 0, 0, 0, 1 }, 0);
    m_fbo.clear(gl::FramebufferType::eColor, eig::Array4f { 0, 0, 0, 1 }, 1);
    m_fbo.clear(gl::FramebufferType::eDepth, 1.f);

    // Bind required resources to their corresponding targets
    m_program.bind("b_buff_unif", m_unif_buffer);
    m_program.bind("b_buff_objects", e_objc_data.info_gl);

    // Dispatch draw call to handle entire scene
    gl::dispatch_multidraw(m_draw);

    // Rebind prior framebuffer
    // TODO avoid unecessary state switches
    begin_handle("frame_buffer_ms").getr<gl::Framebuffer>().bind();
    
    // TODO remove debug view
    if (ImGui::Begin("GBuffer visualizer")) {
      const auto &i_gbuffer_norm_dp = info("gbuffer_norm_dp").getr<gl::Texture2d4f>();
      const auto &i_gbuffer_txc_idx = info("gbuffer_txc_idx").getr<gl::Texture2d4f>();
      ImGui::Image(ImGui::to_ptr(i_gbuffer_norm_dp.object()), { 512, 512 }, { 0, 1 }, { 1, 0 });
      ImGui::Image(ImGui::to_ptr(i_gbuffer_txc_idx.object()), { 512, 512 }, { 0, 1 }, { 1, 0 });
    }
    ImGui::End();
  }
} // namespace met