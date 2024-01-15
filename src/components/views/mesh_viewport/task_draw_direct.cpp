#include <metameric/core/scene.hpp>
#include <metameric/components/views/mesh_viewport/task_draw_direct.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/render/renderer.hpp>
#include <metameric/render/sensor.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/dispatch.hpp>

namespace met {
  constexpr uint n_iters_per_dispatch = 1u;
  constexpr uint n_iters_max          = 65536u;
  constexpr auto buffer_create_flags  = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags  = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  bool MeshViewportDrawDirectTask::is_active(SchedulerHandle &info) {
    met_trace();
    auto is_objc_present = !info.global("scene").getr<Scene>().components.objects.empty();
    auto is_view_present = info.relative("viewport_begin")("is_active").getr<bool>();
    return is_objc_present && is_view_present; 
  }

  void MeshViewportDrawDirectTask::init(SchedulerHandle &info) {
    met_trace_full();

    /* // Initialize program object
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
    info("target").set<gl::Texture2d4f>({ }); */

    info("direct_renderer").init<PathRenderer>({ .spp_per_iter = n_iters_per_dispatch,  .spp_max = n_iters_max });
  }
    
  void MeshViewportDrawDirectTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get handles to relative task resource
    auto begin_handle   = info.relative("viewport_begin");
    auto target_handle  = begin_handle("lrgb_target");
    auto arcball_handle = info.relative("viewport_input")("arcball");

    // Get shared resources 
    const auto &e_scene   = info.global("scene").getr<Scene>();
    // const auto &e_gbuffer = info.relative("viewport_draw_gbuffer")("gbuffer").getr<gl::Texture2d4f>();
    // const auto &e_gbuffer = info.relative("viewport_draw_gbuffer")("gbuffer_renderer").getr<detail::GBuffer>();
    const auto &e_sensor  = info.relative("viewport_draw_gbuffer")("gbuffer_sensor").getr<Sensor>();

    // Get modified resources
    // auto &i_target = info("target").getw<gl::Texture2d4f>();
    auto &i_renderer = info("direct_renderer").getw<PathRenderer>();

   /*  // Some state flags to test when to restart sampling
    bool rebuild_frame = !m_state_buffer.is_init() || target_handle.is_mutated();
    bool restart_frame = arcball_handle.is_mutated() || e_scene.components.objects || e_scene.components.settings; */

    // Offload rendering
    bool rerender = target_handle.is_mutated() 
      || arcball_handle.is_mutated() 
      || e_scene.components.objects 
      || e_scene.components.emitters 
      || e_scene.components.settings;    
    
    if (rerender) {
      i_renderer.reset(e_sensor, e_scene);
    }
    i_renderer.render(e_sensor, e_scene);

    /* // Re-initialize state if target viewport is resized or needs initializing
    if (rebuild_frame) {
      // Resize internal state buffer and target accordingly
      const auto &e_target = target_handle.getr<gl::Texture2d4f>();
      m_state_buffer = {{ .size = e_target.size().prod() * sizeof(uint) }};
      i_target       = {{ .size = e_target.size() }};
    }

    // Re-start state if something like camera/scene changed
    if (rebuild_frame || restart_frame) {
      // Push fresh camera matrix to uniform data
      const auto &e_arcball = arcball_handle.getr<detail::Arcball>();
      m_unif_buffer_map->trf = e_arcball.full().matrix();

      // Set cumulative frame to 0
      i_target.clear();
      m_iter = 0;
    }

    // Early-out; the maximum sample count has been reached, and we
    // can save a bit on the energy bill
    guard(m_iter < n_iters_max);

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
    m_program.bind("b_buff_sensor",    e_sensor.buffer());
    m_program.bind("b_buff_sampler",   m_sampler_buffer);
    m_program.bind("b_buff_state",     m_state_buffer);
    m_program.bind("b_target_4f",      i_target);
    m_program.bind("b_gbuffer",        e_gbuffer.film());
    m_program.bind("b_buff_weights",   e_scene.components.upliftings.gl.texture_weights.buffer());
    m_program.bind("b_bary_4f",        e_scene.components.upliftings.gl.texture_weights.texture());
    m_program.bind("b_spec_4f",        e_scene.components.upliftings.gl.texture_spectra);
    m_program.bind("b_buff_objects",   e_scene.components.objects.gl.object_info);
    m_program.bind("b_cmfs_3f",        e_scene.resources.observers.gl.cmfs_texture);
    m_program.bind("b_buff_bvhs_info", e_scene.resources.meshes.gl.mesh_info);
    m_program.bind("b_buff_bvhs_node", e_scene.resources.meshes.gl.bvh_nodes);
    m_program.bind("b_buff_bvhs_prim", e_scene.resources.meshes.gl.bvh_prims);
    m_program.bind("b_buff_mesh_vert", e_scene.resources.meshes.gl.mesh_verts);
    m_program.bind("b_buff_mesh_elem", e_scene.resources.meshes.gl.mesh_elems_al);
    m_program.bind("b_buff_textures",  e_scene.resources.images.gl.texture_info);
    m_program.bind("b_txtr_1f",        e_scene.resources.images.gl.texture_atlas_1f.texture());
    m_program.bind("b_txtr_3f",        e_scene.resources.images.gl.texture_atlas_3f.texture());
    m_program.bind("b_illm_1f",        e_scene.resources.illuminants.gl.spec_texture);

    // Dispatch compute shader
    gl::sync::memory_barrier(gl::BarrierFlags::eImageAccess  |
                             gl::BarrierFlags::eTextureFetch       |
                             gl::BarrierFlags::eClientMappedBuffer |
                             gl::BarrierFlags::eUniformBuffer      );
    gl::dispatch_compute({ .groups_x         = dispatch_ndiv.x(),
                           .groups_y         = dispatch_ndiv.y(),
                           .bindable_program = &m_program      });

    m_iter += n_iters_per_dispatch; */
  }
} // namespace met