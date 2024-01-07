#include <metameric/core/utility.hpp>
#include <metameric/render/renderer.hpp>
#include <small_gl/program.hpp>

constexpr static auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
constexpr static auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

namespace met {
  namespace detail {
    BaseIntegrationRenderer::BaseIntegrationRenderer() {
      met_trace_full();
      
      m_sampler_state     = {{ .size = sizeof(SamplerState), .flags = buffer_create_flags }};
      m_sampler_state_map = m_sampler_state.map_as<SamplerState>(buffer_access_flags).data();
    }

    void BaseIntegrationRenderer::init_sampler_state(uint n) {
      met_trace_full();
      if (size_t size = n * sizeof(uint); !m_sampler_data.is_init() || m_sampler_data.size() != size) {
        m_sampler_data = {{ .size = size }};
      }
      m_sampler_state_map->spp_curr = 0; // Overflow to 0 for first sample oh god why
      m_sampler_state.flush();
    }

    bool BaseIntegrationRenderer::has_next_sampler_state() const {
      met_trace();
      return m_sampler_state_map->spp_curr < m_spp_max;
    }

    void BaseIntegrationRenderer::next_sampler_state() {
      met_trace_full();
      m_sampler_state_map->spp_curr += m_sampler_state_map->spp_per_iter;
      m_sampler_state.flush();
    }

    GBuffer::GBuffer() {
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

    void GBuffer::reset(const Sensor &sensor, const Scene &scene) {
      met_trace_full();
      
      // Rebuild framebuffer and target texture if necessary
      if (!m_film.is_init() || !m_film.size().isApprox(sensor.film_size)) {
        m_film      = {{ .size = sensor.film_size.max(1).eval() }};
        m_fbo_depth = {{ .size = sensor.film_size.max(1).eval() }};
        m_fbo = {{ .type = gl::FramebufferType::eColor, .attachment = &m_film      },
                 { .type = gl::FramebufferType::eDepth, .attachment = &m_fbo_depth }};
      }
    }
    
    const gl::Texture2d4f &GBuffer::render(const Sensor &sensor, const Scene &scene) {
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

      // Specify draw state for next subask
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

  DirectRenderer::DirectRenderer(DirectRendererCreateInfo info)
  : detail::BaseIntegrationRenderer() {
    met_trace_full();

    // Initialize program object
    m_program = {{ .type       = gl::ShaderType::eCompute,
                   .spirv_path = "resources/shaders/render/primitive_direct.comp.spv",
                   .cross_path = "resources/shaders/render/primitive_direct.comp.json" }};

    // Assign sampler configuration
    m_spp_max                         = info.spp_max;
    m_sampler_state_map->spp_per_iter = info.spp_per_iter;
  }

  const gl::Texture2d4f &DirectRenderer::render(const Sensor &sensor, const Scene &scene) {
    met_trace_full();
    
    if (!m_film.is_init() || !m_film.size().isApprox(sensor.film_size))
      reset(sensor, scene);

    // Return early if sampler state has reached end
    guard(has_next_sampler_state(), m_film);

    // Render or reuse gbuffer frame
    const auto &gbuffer = m_sampler_state_map->spp_curr == 0
                        ? m_gbuffer.render(sensor, scene)
                        : m_gbuffer.film();

    // Bind required resources to their corresponding targets
    m_program.bind();
    m_program.bind("b_film",               m_film);
    m_program.bind("b_gbuffer",            gbuffer);
    m_program.bind("b_buff_sensor",        sensor.buffer());
    m_program.bind("b_buff_sampler_state", m_sampler_state);
    m_program.bind("b_buff_sampler_data",  m_sampler_data);
    m_program.bind("b_buff_objects",       scene.components.objects.gl.object_info);
    m_program.bind("b_buff_meshes",        scene.resources.meshes.gl.mesh_info);
    m_program.bind("b_buff_bvhs_node",     scene.resources.meshes.gl.bvh_nodes);
    m_program.bind("b_buff_bvhs_prim",     scene.resources.meshes.gl.bvh_prims);
    m_program.bind("b_buff_mesh_vert",     scene.resources.meshes.gl.mesh_verts);
    m_program.bind("b_buff_mesh_elem",     scene.resources.meshes.gl.mesh_elems_al);
    m_program.bind("b_buff_textures",      scene.resources.images.gl.texture_info);
    m_program.bind("b_buff_weights",       scene.components.upliftings.gl.texture_weights.buffer());
    m_program.bind("b_bary_4f",            scene.components.upliftings.gl.texture_weights.texture());
    m_program.bind("b_spec_4f",            scene.components.upliftings.gl.texture_spectra);
    m_program.bind("b_cmfs_3f",            scene.resources.observers.gl.cmfs_texture);
    m_program.bind("b_txtr_1f",            scene.resources.images.gl.texture_atlas_1f.texture());
    m_program.bind("b_txtr_3f",            scene.resources.images.gl.texture_atlas_3f.texture());
    m_program.bind("b_illm_1f",            scene.resources.illuminants.gl.spec_texture);
    
    // Dispatch compute shader
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderImageAccess  | gl::BarrierFlags::eTextureFetch |
                             gl::BarrierFlags::eClientMappedBuffer | gl::BarrierFlags::eUniformBuffer);
    gl::dispatch_compute(m_dispatch);

    // Advance sampler state for next iteration
    next_sampler_state();

    return m_film;
  }

  void DirectRenderer::reset(const Sensor &sensor, const Scene &scene) {
    met_trace_full();
    
    // Rebuild target texture if necessary
    if (!m_film.is_init() || !m_film.size().isApprox(sensor.film_size)) {
      m_film = {{ .size = sensor.film_size.max(1).eval() }};
    }

    // Reset relevant internal components
    m_gbuffer.reset(sensor, scene);
    m_film.clear();

    // Restart or rebuild sampler state
    init_sampler_state(sensor.film_size.prod());

    // Update dispatch object
    auto dispatch_ndiv  = ceil_div(sensor.film_size, 16u);
    m_dispatch.groups_x = dispatch_ndiv.x();
    m_dispatch.groups_y = dispatch_ndiv.y();
  }
} // namespace met