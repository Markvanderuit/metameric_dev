#include <metameric/core/utility.hpp>
#include <metameric/render/primitives_render.hpp>

namespace met {
  static gl::ProgramCache program_cache;

  constexpr static auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr static auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
  
  namespace detail {
    IntegrationRenderPrimitive::IntegrationRenderPrimitive()
    : m_sampler_state_i(0) {
      met_trace_full();
      for (uint i = 0; i < m_sampler_state_buffs.size(); ++i) {
        auto &buff = m_sampler_state_buffs[i];
        auto &mapp = m_sampler_state_mapps[i];
        buff = {{ .size = sizeof(SamplerState), .flags = buffer_create_flags }};
        mapp = buff.map_as<SamplerState>(buffer_access_flags).data();
      }
    }

    void IntegrationRenderPrimitive::reset(const Sensor &sensor, const Scene &scene) {
      met_trace_full();

      // Reset current sample count
      m_spp_curr = 0;
      
      // Push sample count to next available buffer and add sync object for flush operation
      m_sampler_state_i = (m_sampler_state_i + 1) % m_sampler_state_buffs.size();
      m_sampler_state_mapps[m_sampler_state_i]->spp_per_iter = m_spp_per_iter;
      m_sampler_state_mapps[m_sampler_state_i]->spp_curr     = m_spp_curr;
      m_sampler_state_buffs[m_sampler_state_i].flush();
      m_sampler_state_syncs[m_sampler_state_i] = gl::sync::Fence(gl::sync::time_s(1));
    }

    void IntegrationRenderPrimitive::advance_sampler_state() {
      met_trace_full();

      // Advance current sample count by previous nr. of taken samples
      m_spp_curr += m_spp_per_iter;

      // Push sample count to next available buffer and add sync object for flush operation
      m_sampler_state_i = (m_sampler_state_i + 1) % m_sampler_state_buffs.size();
      m_sampler_state_mapps[m_sampler_state_i]->spp_per_iter = m_spp_per_iter;
      m_sampler_state_mapps[m_sampler_state_i]->spp_curr     = m_spp_curr;
      m_sampler_state_buffs[m_sampler_state_i].flush();
      m_sampler_state_syncs[m_sampler_state_i] = gl::sync::Fence(gl::sync::time_s(1));
    }

    const gl::Buffer & IntegrationRenderPrimitive::get_sampler_state() {
      met_trace_full();

      // Block if flush operation has not completed
      if (auto &sync = m_sampler_state_syncs[m_sampler_state_i]; sync.is_init())
        sync.gpu_wait();
      
      return m_sampler_state_buffs[m_sampler_state_i];
    }

    const gl::Texture2d4f & IntegrationRenderPrimitive::render(const Sensor &sensor, const Scene &scene) {
      met_trace();
      return m_film;
    }


    GBufferRenderPrimitive::GBufferRenderPrimitive() {
      // Initialize program object
      m_program = {{ .type       = gl::ShaderType::eVertex,
                     .spirv_path = "resources/shaders/render/primitive_gbuffer.vert.spv",
                     .cross_path = "resources/shaders/render/primitive_gbuffer.vert.json" },
                   { .type       = gl::ShaderType::eFragment,
                     .spirv_path = "resources/shaders/render/primitive_gbuffer.frag.spv",
                     .cross_path = "resources/shaders/render/primitive_gbuffer.frag.json" }};

      // Initialize draw object
      m_draw = { 
        .type         = gl::PrimitiveType::eTriangles,
        .capabilities = {{ gl::DrawCapability::eDepthTest, true },
                         { gl::DrawCapability::eCullOp,    true }},
        .draw_op      = gl::DrawOp::eFill
      };
    }

    void GBufferRenderPrimitive::reset(const Sensor &sensor, const Scene &scene) {
      met_trace_full();
      
      // Rebuild framebuffer and target texture if necessary
      if (!m_film.is_init() || !m_film.size().isApprox(sensor.film_size)) {
        m_film      = {{ .size = sensor.film_size.max(1).eval() }};
        m_fbo_depth = {{ .size = sensor.film_size.max(1).eval() }};
        m_fbo = {{ .type = gl::FramebufferType::eColor, .attachment = &m_film      },
                 { .type = gl::FramebufferType::eDepth, .attachment = &m_fbo_depth }};
      }
    }
    
    const gl::Texture2d4f &GBufferRenderPrimitive::render(const Sensor &sensor, const Scene &scene) {
      met_trace_full();

      if (!m_film.is_init() || !m_film.size().isApprox(sensor.film_size))
        reset(sensor, scene);
      
      const auto &objects = scene.components.objects;
      const auto &meshes  = scene.resources.meshes;
      
      // Assemble appropriate draw data for each object in the scene
      m_draw.bindable_array = &scene.resources.meshes.gl.array;
      m_draw.commands.resize(objects.size());
      rng::transform(objects, m_draw.commands.begin(), [&](const auto &comp) {
        guard(comp.value.is_active, gl::MultiDrawInfo::DrawCommand { });
        return meshes.gl.draw_commands[comp.value.mesh_i];
      });

      // Specify draw state for next subtask
      gl::state::set_viewport(sensor.film_size);    
      gl::state::set_depth_range(0.f, 1.f);
      gl::state::set_op(gl::DepthOp::eLessOrEqual);
      gl::state::set_op(gl::CullOp::eBack);
      gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);

      // Prepare framebuffer state
      eig::Array4f fbo_colr_value = { 0, 0, 0, std::bit_cast<float>(0xFFFFFFFFu) };
      m_fbo.bind();
      m_fbo.clear(gl::FramebufferType::eColor, fbo_colr_value, 0);
      m_fbo.clear(gl::FramebufferType::eDepth, 1.f);

      // Prepare program state
      m_program.bind();
      m_program.bind("b_buff_sensor",  sensor.buffer());
      m_program.bind("b_buff_objects", objects.gl.object_info);
      m_program.bind("b_buff_meshes",  meshes.gl.mesh_info);
      
      // Dispatch draw call with appropriate barriers
      gl::sync::memory_barrier(gl::BarrierFlags::eFramebuffer        | 
                               gl::BarrierFlags::eTextureFetch       |
                               gl::BarrierFlags::eClientMappedBuffer |
                               gl::BarrierFlags::eUniformBuffer      );
      gl::dispatch_multidraw(m_draw);

      // Return film, which is bound to the framebuffer as color target
      return m_film;
    }
  } // namespace detail

  DirectRenderPrimitive::DirectRenderPrimitive(DirectRenderPrimitiveCreateInfo info)
  : detail::IntegrationRenderPrimitive() {
    met_trace_full();

    // Initialize program object
    m_program = {{ .type       = gl::ShaderType::eCompute,
                   .spirv_path = "resources/shaders/render/primitive_direct.comp.spv",
                   .cross_path = "resources/shaders/render/primitive_direct.comp.json" }};

    // Assign sampler configuration
    m_spp_curr     = 0;
    m_spp_max      = info.spp_max;
    m_spp_per_iter = info.spp_per_iter;
  }

  const gl::Texture2d4f &DirectRenderPrimitive::render(const Sensor &sensor, const Scene &scene) {
    met_trace_full();
    
    // If the film object is stale, run a reset()
    if (!m_film.is_init() || !m_film.size().isApprox(sensor.film_size))
      reset(sensor, scene);

    // Return converged film if sample state was exhausted
    guard(has_next_sample_state(), m_film);

    // Either render or reuse the current gbuffer frame as an initial hit
    const auto &gbuffer = m_spp_curr ? m_gbuffer.film() : m_gbuffer.render(sensor, scene);

    // Bind required resources to their corresponding targets
    m_program.bind();
    m_program.bind("b_film",               m_film);
    m_program.bind("b_gbuffer",            gbuffer);
    m_program.bind("b_buff_sensor",        sensor.buffer());
    m_program.bind("b_buff_sampler_state", get_sampler_state());
    m_program.bind("b_buff_objects",       scene.components.objects.gl.object_info);
    m_program.bind("b_buff_meshes",        scene.resources.meshes.gl.mesh_info);
    m_program.bind("b_buff_bvhs_node",     scene.resources.meshes.gl.bvh_nodes);
    m_program.bind("b_buff_bvhs_prim",     scene.resources.meshes.gl.bvh_prims);
    m_program.bind("b_buff_mesh_vert",     scene.resources.meshes.gl.mesh_verts);
    m_program.bind("b_buff_mesh_elem",     scene.resources.meshes.gl.mesh_elems_al);
    m_program.bind("b_buff_textures",      scene.resources.images.gl.texture_info);
    m_program.bind("b_buff_wvls_distr",    scene.components.colr_systems.gl.wavelength_distr_buffer);
    m_program.bind("b_buff_barycentrics",  scene.components.upliftings.gl.texture_barycentrics.buffer());
    m_program.bind("b_bary_4f",            scene.components.upliftings.gl.texture_barycentrics.texture());
    m_program.bind("b_spec_4f",            scene.components.upliftings.gl.texture_spectra);
    m_program.bind("b_cmfs_3f",            scene.resources.observers.gl.cmfs_texture);
    m_program.bind("b_txtr_1f",            scene.resources.images.gl.texture_atlas_1f.texture());
    m_program.bind("b_txtr_3f",            scene.resources.images.gl.texture_atlas_3f.texture());
    m_program.bind("b_illm_1f",            scene.resources.illuminants.gl.spec_texture);

    // Dispatch compute shader
    gl::sync::memory_barrier( gl::BarrierFlags::eImageAccess   | gl::BarrierFlags::eTextureFetch  |
                              gl::BarrierFlags::eUniformBuffer | gl::BarrierFlags::eStorageBuffer | 
                              gl::BarrierFlags::eBufferUpdate                                     );
    gl::dispatch_compute(m_dispatch);

    // Advance sampler state for next render() call
    advance_sampler_state();

    return m_film;
  }

  void DirectRenderPrimitive::reset(const Sensor &sensor, const Scene &scene) {
    met_trace_full();
    IntegrationRenderPrimitive::reset(sensor, scene);
    m_gbuffer.reset(sensor, scene);

    // Rebuild target texture if necessary
    if (!m_film.is_init() || !m_film.size().isApprox(sensor.film_size)) {
      m_film = {{ .size = sensor.film_size.max(1).eval() }};
      auto dispatch_ndiv  = ceil_div(m_film.size(), 16u);
      m_dispatch.groups_x = dispatch_ndiv.x();
      m_dispatch.groups_y = dispatch_ndiv.y();
    }
    m_film.clear();
  }

  PathRenderPrimitive::PathRenderPrimitive(PathRenderPrimitiveCreateInfo info)
  : detail::IntegrationRenderPrimitive(),
    m_cache_handle(info.cache_handle) {
    met_trace_full();

    // Initialize program object, if it doesn't yet exist
    std::tie(m_cache_key, std::ignore) = m_cache_handle.getw<gl::ProgramCache>().set({ 
      .type       = gl::ShaderType::eCompute,
      .spirv_path = "resources/shaders/render/primitive_render_path.comp.spv",
      .cross_path = "resources/shaders/render/primitive_render_path.comp.json",
      .spec_const = {{ 0u, 16u            },
                     { 1u, 16u            },
                     { 2u, info.max_depth }}
    });

    // Assign sampler configuration
    m_spp_curr     = 0;
    m_spp_max      = info.spp_max;
    m_spp_per_iter = info.spp_per_iter;
  }

  void PathRenderPrimitive::reset(const Sensor &sensor, const Scene &scene) {
    met_trace_full();
    IntegrationRenderPrimitive::reset(sensor, scene);

    // Rebuild target texture if necessary
    if (!m_film.is_init() || !m_film.size().isApprox(sensor.film_size)) {
      m_film = {{ .size = sensor.film_size.max(1).eval() }};
      auto dispatch_ndiv  = ceil_div(m_film.size(), 16u);
      m_dispatch.groups_x = dispatch_ndiv.x();
      m_dispatch.groups_y = dispatch_ndiv.y();
    }

    // Set film to black
    m_film.clear();
  }

  const gl::Texture2d4f &PathRenderPrimitive::render(const Sensor &sensor, const Scene &scene) {
    met_trace_full();
    
    // If the film object is stale, run a reset()
    if (!m_film.is_init() || !m_film.size().isApprox(sensor.film_size))
      reset(sensor, scene);

    // Return early if sample count has reached specified maximum
    guard(has_next_sample_state(), m_film);

    // Draw relevant program from cache
    auto &program = m_cache_handle.getw<gl::ProgramCache>().at(m_cache_key);

    // Bind required resources to their corresponding targets
    program.bind();
    program.bind("b_film",                m_film);
    program.bind("b_buff_sensor",         sensor.buffer());
    program.bind("b_buff_sampler_state",  get_sampler_state());
    program.bind("b_buff_objects",        scene.components.objects.gl.object_info);
    program.bind("b_buff_emitters",       scene.components.emitters.gl.emitter_info);
    program.bind("b_buff_barycentrics",   scene.components.upliftings.gl.texture_barycentrics.buffer());
    program.bind("b_buff_wvls_distr",     scene.components.colr_systems.gl.wavelength_distr_buffer);
    program.bind("b_buff_emitters_distr", scene.components.emitters.gl.emitter_distr_buffer);
    program.bind("b_bary_4f",             scene.components.upliftings.gl.texture_barycentrics.texture());
    program.bind("b_spec_4f",             scene.components.upliftings.gl.texture_spectra);
    program.bind("b_cmfs_3f",             scene.resources.observers.gl.cmfs_texture);
    program.bind("b_illm_1f",             scene.resources.illuminants.gl.spec_texture);
    if (!scene.resources.images.empty()) {
      program.bind("b_buff_textures", scene.resources.images.gl.texture_info);
      program.bind("b_txtr_1f",       scene.resources.images.gl.texture_atlas_1f.texture());
      program.bind("b_txtr_3f",       scene.resources.images.gl.texture_atlas_3f.texture());
    }
    if (!scene.resources.meshes.empty()) {
      program.bind("b_buff_meshes",    scene.resources.meshes.gl.mesh_info);
      program.bind("b_buff_bvhs_node", scene.resources.meshes.gl.bvh_nodes);
      program.bind("b_buff_bvhs_prim", scene.resources.meshes.gl.bvh_prims);
    }

    // Dispatch compute shader
    gl::sync::memory_barrier( gl::BarrierFlags::eImageAccess   | gl::BarrierFlags::eTextureFetch  |
                              gl::BarrierFlags::eUniformBuffer | gl::BarrierFlags::eStorageBuffer | 
                              gl::BarrierFlags::eBufferUpdate                                     );
    gl::dispatch_compute(m_dispatch);

    // Advance sampler state for next render() call
    advance_sampler_state();

    return m_film;
  }
} // namespace met