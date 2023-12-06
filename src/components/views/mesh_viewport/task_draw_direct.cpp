#include <metameric/core/scene.hpp>
#include <metameric/components/views/mesh_viewport/task_draw_direct.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/misc/detail/scene.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/dispatch.hpp>

namespace met {
  constexpr uint n_iters_per_dispatch = 64u;
  constexpr uint n_iters_max          = 4096u;
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  bool MeshViewportDrawDirectTask::is_active(SchedulerHandle &info) {
    met_trace();

    auto is_objc_present = !info.global("scene").getr<Scene>().components.objects.empty();
    auto is_view_present = info.relative("viewport_begin")("is_active").getr<bool>();
    auto is_objc_updated = info("scene_handler", "objc_data").is_mutated();
    auto is_view_updated = info.relative("viewport_begin")("lrgb_target").is_mutated()
                       ||  info.relative("viewport_input")("arcball").is_mutated();

    return is_objc_present && (is_objc_updated || is_view_updated);
  }

  void MeshViewportDrawDirectTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Initialize program object
    m_program = {{ .type       = gl::ShaderType::eCompute,
                   .spirv_path = "resources/shaders/views/draw_mesh_direct.comp.spv",
                   .cross_path = "resources/shaders/views/draw_mesh_direct.comp.json" }};

    // Initialize uniform buffers and corresponding mappings
    m_unif_buffer     = {{ .size = sizeof(UnifLayout), .flags = buffer_create_flags }};
    m_unif_buffer_map = m_unif_buffer.map_as<UnifLayout>(buffer_access_flags).data();
    m_sampler_buffer     = {{ .size = sizeof(SamplerLayout), .flags = buffer_create_flags }};
    m_sampler_buffer_map = m_sampler_buffer.map_as<SamplerLayout>(buffer_access_flags).data();
    m_sampler_buffer_map->n_iters_per_dispatch = n_iters_per_dispatch;

    // Internal target texture; can be differently sized
    info("target").set<gl::Texture2d4f>({ });
  }
    
  void MeshViewportDrawDirectTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get handles to relative task resource
    auto begin_handle   = info.relative("viewport_begin");
    auto target_handle  = begin_handle("lrgb_target");
    auto arcball_handle = info.relative("viewport_input")("arcball");
    auto object_handle  = info("scene_handler", "objc_data");

    // Get shared resources 
    const auto &e_scene     = info.global("scene").getr<Scene>();
    const auto &e_objc_data = info("scene_handler", "objc_data").getr<detail::RTObjectData>();
    const auto &e_mesh_data = info("scene_handler", "mesh_data").getr<detail::RTMeshData>();
    const auto &e_txtr_data = info("scene_handler", "txtr_data").getr<detail::RTTextureData>();
    const auto &e_uplf_data = info("scene_handler", "uplf_data").getr<detail::RTUpliftingData>();
    const auto &e_cmfs_data = info("scene_handler", "cmfs_data").getr<detail::RTObserverData>();
    const auto &e_illm_data = info("scene_handler", "illm_data").getr<detail::RTIlluminantData>();
    const auto &e_csys_data = info("scene_handler", "csys_data").getr<detail::RTColorSystemData>();
    const auto &e_gbuffer   = info.relative("viewport_draw_gbuffer")("gbuffer").getr<gl::Texture2d4f>();

    // Get modified resources
    auto &i_target = info("target").getw<gl::Texture2d4f>();

    // Some state flags to test when to restart sampling
    bool rebuild_frame = !m_state_buffer.is_init() || target_handle.is_mutated();
    bool restart_frame = rebuild_frame || target_handle.is_mutated() || arcball_handle.is_mutated() || object_handle.is_mutated();

    // Re-initialize state if target viewport is resized or needs initializing
    if (rebuild_frame) {
      // Resize internal state buffer and target accordingly
      const auto &e_target = target_handle.getr<gl::Texture2d4f>();
      m_state_buffer = {{ .size = e_target.size().prod() * sizeof(eig::Array2u) }};
      i_target       = {{ .size = e_target.size() }};
    }

    // Re-start state if something like camera/scene changed
    if (restart_frame) {
      // Push fresh camera matrix to uniform data
      const auto &e_arcball = arcball_handle.getr<detail::Arcball>();
      m_unif_buffer_map->trf = e_arcball.full().matrix();
      m_unif_buffer_map->inv = e_arcball.full().matrix().inverse();

      // Set cumulative frame to 0
      i_target.clear();
      m_iter = 0;
    }

    // Early-out; the maximum sample count has been reached, and we
    // can save a bit on the energy bill
    if (m_iter >= n_iters_max)
      return;

    // Set sampler uniform
    m_sampler_buffer_map->iter = m_iter;
    m_sampler_buffer.flush();

    // Specify dispatch size
    auto dispatch_n    = i_target.size();
    auto dispatch_ndiv = ceil_div(dispatch_n, 16u);

    // Push miscellaneous uniforms
    m_unif_buffer_map->viewport_size = dispatch_n;
    m_unif_buffer.flush();

    // Bind required resources to their corresponding targets
    m_program.bind("b_buff_unif",    m_unif_buffer);
    m_program.bind("b_buff_sampler", m_sampler_buffer);
    m_program.bind("b_buff_state",   m_state_buffer);
    m_program.bind("b_buff_objects", e_objc_data.info_gl);
    m_program.bind("b_buff_uplifts", e_uplf_data.info_gl);
    m_program.bind("b_spec_4f",      e_uplf_data.spectra_gl_texture);
    m_program.bind("b_cmfs_3f",      e_cmfs_data.cmfs_gl_texture);
    m_program.bind("b_illm_1f",      e_illm_data.illm_gl_texture);
    m_program.bind("b_csys_3f",      e_csys_data.csys_gl_texture);
    m_program.bind("b_gbuffer",      e_gbuffer);
    m_program.bind("b_target_4f",    i_target);

    // Bind atlas resources that may not be initialized
    if (e_txtr_data.info_gl.is_init())
      m_program.bind("b_buff_textures", e_txtr_data.info_gl);
    if (e_txtr_data.atlas_1f.texture().is_init())
      m_program.bind("b_txtr_1f", e_txtr_data.atlas_1f.texture());
    if (e_txtr_data.atlas_3f.texture().is_init())
      m_program.bind("b_txtr_3f", e_txtr_data.atlas_3f.texture());
    if (e_objc_data.atlas_4f.texture().is_init())
      m_program.bind("b_uplf_4f", e_objc_data.atlas_4f.texture());

    // Dispatch compute shader
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderImageAccess  |
                             gl::BarrierFlags::eTextureFetch       |
                             gl::BarrierFlags::eClientMappedBuffer |
                             gl::BarrierFlags::eUniformBuffer      );
    gl::dispatch_compute({ .groups_x         = dispatch_ndiv.x(),
                           .groups_y         = dispatch_ndiv.y(),
                           .bindable_program = &m_program      });

    m_iter += n_iters_per_dispatch;
  }
} // namespace met