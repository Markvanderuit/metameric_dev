#include <metameric/core/distribution.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/render/primitives_query.hpp>
#include <small_gl/detail/program_cache.hpp>

namespace met {
  namespace detail {
    BaseQueryPrimitive::BaseQueryPrimitive() {
      met_trace_full();
      std::tie(m_query, m_query_map) = gl::Buffer::make_flusheable_object<QueryUnifLayout>();
    }
  } // namespace detail

  PathQueryPrimitive::PathQueryPrimitive(PathQueryPrimitiveInfo info)
  : detail::BaseQueryPrimitive(),
    m_cache_handle(info.cache_handle),
    m_max_depth(info.max_depth) {
    met_trace_full();

    // Initialize program object
    std::tie(m_cache_key, std::ignore) = m_cache_handle.getw<gl::detail::ProgramCache>().set({ 
      .type       = gl::ShaderType::eCompute,
      .spirv_path = "resources/shaders/render/primitive_query_path.comp.spv",
      .cross_path = "resources/shaders/render/primitive_query_path.comp.json",
      .spec_const = {{ 0u, 256u        },
                     { 1u, m_max_depth }}
    });
  }

  const gl::Buffer &PathQueryPrimitive::query(const PixelSensor &sensor, const Scene &scene, uint spp) {
    met_trace_full();

    // Resize output buffer to accomodate requested nr. of paths
    // and generate read-only map
    size_t buffer_size = 4 * sizeof(uint) 
                       + spp * (m_max_depth - 1) * sizeof(PathRecord);
    if (!m_output.is_init() || m_output.size() < buffer_size) {
      constexpr auto buffer_create_flags_read  = gl::BufferCreateFlags::eMapReadPersistent;
      constexpr auto buffer_access_flags_read  = gl::BufferAccessFlags::eMapReadPersistent;
      m_output = {{ .size = buffer_size, .flags = buffer_create_flags_read }};
      auto map = m_output.map(buffer_access_flags_read);
      m_output_head_map = cast_span<uint>(map).data();
      m_output_data_map = cast_span<PathRecord>(map.subspan(4 * sizeof(uint)));
    }

    // (Re)generate sampling distribution for wavelengths
    bool rebuild_wavelength_distr 
      = !m_wavelength_distr_buffer.is_init() 
      || scene.components.settings.state.view_i
      || scene.components.views[scene.components.settings->view_i].state.observer_i
      || scene.resources.observers
      || scene.resources.illuminants; 
    if (rebuild_wavelength_distr) {
      // Get scene observer, and list of (scaled) active emitter SPDs in the scene
      CMFS observer = scene.primary_observer();
      auto emitters = scene.components.emitters
                    | vws::filter([](const auto &comp) { return comp.value.is_active; });
      auto illums = emitters
                  | vws::transform([ ](const auto &comp) { return comp.value.illuminant_i; })
                  | vws::transform([&](uint i) { return *scene.resources.illuminants[i];   })
                  | rng::to<std::vector>();
      auto scales = emitters
                  | vws::transform([](const auto &comp) { return comp.value.illuminant_scale; })
                  | rng::to<std::vector>();
  
      // Generate average over scaled distributions
      m_wavelength_distr = 1.f;
      if (!illums.empty()) {
        m_wavelength_distr = 0.f;
        for (uint i = 0; i < illums.size(); ++i)
          m_wavelength_distr += illums[i] * scales[i];
      }
      m_wavelength_distr /= m_wavelength_distr.maxCoeff();

      // Scale by observer, then add defensive sampling for very small values
      m_wavelength_distr *= observer.array().rowwise().sum();
      m_wavelength_distr += (Spec(1) - m_wavelength_distr) * 0.01f;
      
      // Push sampling distribution to buffer
      m_wavelength_distr_buffer = Distribution(cnt_span<float>(m_wavelength_distr)).to_buffer_std140();
    }

    // Clear output data, specifically the buffer's head
    eig::Array4f clear_value = 0.f;
    m_output.clear(obj_span<const std::byte>(clear_value), 1u, sizeof(decltype(clear_value)));

    // Flush sample count to query buffer
    m_query_map->spp = spp;
    m_query.flush();

    // Draw relevant program from cache
    auto &program = m_cache_handle.getw<gl::detail::ProgramCache>().at(m_cache_key);

    // Bind required resources to their corresponding targets
    program.bind();
    program.bind("b_buff_output",         m_output);
    program.bind("b_buff_query",          m_query);
    program.bind("b_buff_sensor",         sensor.buffer());
    program.bind("b_buff_objects",        scene.components.objects.gl.object_info);
    program.bind("b_buff_emitters",       scene.components.emitters.gl.emitter_info);
    program.bind("b_buff_coeffs",         scene.components.upliftings.gl.texture_coefficients.buffer()); 
    program.bind("b_buff_wvls_distr",     m_wavelength_distr_buffer);
    program.bind("b_buff_emitters_distr", scene.components.emitters.gl.emitter_distr_buffer);
    program.bind("b_buff_envmap_info",    scene.components.emitters.gl.emitter_envm_info);
    program.bind("b_coef_4f",             scene.components.upliftings.gl.texture_coefficients.texture());
    program.bind("b_coef_4f",             m_sampler);
    program.bind("b_bsis_1f",             scene.components.upliftings.gl.texture_basis);
    program.bind("b_bsis_1f",             m_sampler);
    program.bind("b_cmfs_3f",             scene.resources.observers.gl.cmfs_texture);
    program.bind("b_cmfs_3f",             m_sampler);
    program.bind("b_illm_1f",             scene.resources.illuminants.gl.spec_texture);
    program.bind("b_illm_1f",             m_sampler);
    if (!scene.resources.meshes.empty()) {
      program.bind("b_buff_meshes",    scene.resources.meshes.gl.mesh_info);
      program.bind("b_buff_bvhs_node", scene.resources.meshes.gl.bvh_nodes);
      program.bind("b_buff_bvhs_prim", scene.resources.meshes.gl.bvh_prims);
    }

    // Dispatch compute shader
    gl::sync::memory_barrier( gl::BarrierFlags::eImageAccess   | gl::BarrierFlags::eTextureFetch  |
                              gl::BarrierFlags::eUniformBuffer | gl::BarrierFlags::eStorageBuffer | 
                              gl::BarrierFlags::eBufferUpdate                                     );
    gl::dispatch_compute({ .groups_x = ceil_div(spp, 256u) });

    // Insert memory barrier and submit a fence object to ensure
    // output data is made visible in mapped region
    gl::sync::memory_barrier(gl::BarrierFlags::eClientMappedBuffer);
    m_output_sync = gl::sync::Fence(gl::sync::time_s(1));

    return m_output;
  }

  std::span<const PathRecord> PathQueryPrimitive::data() const {
    // Wait for output data to be visible, and then
    // return generated output data
    guard(m_output_sync.is_init(), std::span<const PathRecord>());
    m_output_sync.cpu_wait();
    return m_output_data_map.subspan(0, *m_output_head_map);
  }

  RayQueryPrimitive::RayQueryPrimitive(RayQueryPrimitiveInfo info)
  :  m_cache_handle(info.cache_handle) {
    met_trace_full();

    // Initialize program object
    std::tie(m_cache_key, std::ignore) = m_cache_handle.getw<gl::detail::ProgramCache>().set({ 
      .type       = gl::ShaderType::eCompute,
      .spirv_path = "resources/shaders/render/primitive_query_ray.comp.spv",
      .cross_path = "resources/shaders/render/primitive_query_ray.comp.json",
      .spec_const = {{ 0u, 1u }} // Tiny workgroup
    });
  }
  
  const gl::Buffer &RayQueryPrimitive::query(const RaySensor &sensor, const Scene &scene) {
    met_trace_full();

    // Resize output buffer to accomodate requested nr. of paths
    // and generate read-only map
    size_t buffer_size = sizeof(RayRecord);
    if (!m_output.is_init() || m_output.size() != buffer_size) {
      constexpr auto buffer_create_flags_read  = gl::BufferCreateFlags::eMapReadPersistent;
      constexpr auto buffer_access_flags_read  = gl::BufferAccessFlags::eMapReadPersistent;
      m_output     = {{ .size = buffer_size, .flags = buffer_create_flags_read }};
      m_output_map = m_output.map_as<RayRecord>(buffer_access_flags_read).data();
    }

    // Draw relevant program from cache
    auto &program = m_cache_handle.getw<gl::detail::ProgramCache>().at(m_cache_key);

    // Bind required resources to their corresponding targets
    program.bind();
    program.bind("b_buff_output",         m_output);
    program.bind("b_buff_sensor",         sensor.buffer());
    program.bind("b_buff_objects",        scene.components.objects.gl.object_info);
    program.bind("b_buff_emitters",       scene.components.emitters.gl.emitter_info);
    program.bind("b_buff_envmap_info",    scene.components.emitters.gl.emitter_envm_info);
    if (!scene.resources.meshes.empty()) {
      program.bind("b_buff_meshes",        scene.resources.meshes.gl.mesh_info);
      program.bind("b_buff_bvhs_node",     scene.resources.meshes.gl.bvh_nodes);
      program.bind("b_buff_bvhs_prim",     scene.resources.meshes.gl.bvh_prims);
    }

    // Dispatch compute shader
    gl::sync::memory_barrier( gl::BarrierFlags::eImageAccess   | gl::BarrierFlags::eTextureFetch  |
                              gl::BarrierFlags::eUniformBuffer | gl::BarrierFlags::eStorageBuffer | 
                              gl::BarrierFlags::eBufferUpdate                                     );
    gl::dispatch_compute({ .groups_x = 1u });

    // Insert memory barrier and submit a fence object to ensure
    // output data is made visible in mapped region
    gl::sync::memory_barrier(gl::BarrierFlags::eClientMappedBuffer);
    m_output_sync = gl::sync::Fence(gl::sync::time_s(1));

    return m_output;
  }

  const RayRecord &RayQueryPrimitive::data() const {
    // Wait for output data to be visible, and then
    // return generated output data
    guard(m_output_sync.is_init(), *m_output_map);
    m_output_sync.cpu_wait();
    return *m_output_map;
  }
} // namespace met