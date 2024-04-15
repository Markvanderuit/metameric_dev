#include <metameric/core/utility.hpp>
#include <metameric/render/primitives_query.hpp>

namespace met {
  constexpr static auto buffer_create_flags_read  = gl::BufferCreateFlags::eMapReadPersistent;
  constexpr static auto buffer_access_flags_read  = gl::BufferAccessFlags::eMapReadPersistent;
  constexpr static auto buffer_create_flags_write = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr static auto buffer_access_flags_write = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  namespace detail {
    BaseQueryPrimitive::BaseQueryPrimitive() {
      met_trace_full();

      m_query     = {{ .size = sizeof(QueryUnifLayout), .flags = buffer_create_flags_write }};
      m_query_map = m_query.map_as<QueryUnifLayout>(buffer_access_flags_write).data();
    }
  } // namespace detail

  PathQueryPrimitive::PathQueryPrimitive(PathQueryPrimitiveInfo info)
  : detail::BaseQueryPrimitive(),
    m_cache_handle(info.cache_handle),
    m_max_depth(info.max_depth) {
    met_trace_full();

    // Initialize program object
    std::tie(m_cache_key, std::ignore) = m_cache_handle.getw<gl::ProgramCache>().set({ 
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
      m_output = {{ .size = buffer_size, .flags = buffer_create_flags_read }};
      auto map = m_output.map(buffer_access_flags_read);
      m_output_head_map = cast_span<uint>(map).data();
      m_output_data_map = cast_span<PathRecord>(map.subspan(4 * sizeof(uint)));
    }

    // Clear output data, specifically the buffer's head
    eig::Array4f clear_value = 0.f;
    m_output.clear(obj_span<const std::byte>(clear_value), 1u, sizeof(decltype(clear_value)));

    // Flush sample count to query buffer
    m_query_map->spp = spp;
    m_query.flush();

    // Draw relevant program from cache
    auto &program = m_cache_handle.getw<gl::ProgramCache>().at(m_cache_key);

    // Bind required resources to their corresponding targets
    program.bind();
    program.bind("b_buff_output",         m_output);
    program.bind("b_buff_query",          m_query);
    program.bind("b_buff_sensor",         sensor.buffer());
    program.bind("b_buff_objects",        scene.components.objects.gl.object_info);
    program.bind("b_buff_emitters",       scene.components.emitters.gl.emitter_info);
    program.bind("b_buff_barycentrics",   scene.components.upliftings.gl.texture_barycentrics.buffer());
    program.bind("b_buff_wvls_distr",     scene.components.colr_systems.gl.wavelength_distr_buffer);
    program.bind("b_buff_emitters_distr", scene.components.emitters.gl.emitter_distr_buffer);
    program.bind("b_bary_4f",             scene.components.upliftings.gl.texture_barycentrics.texture());
    program.bind("b_coef_4f",             scene.components.upliftings.gl.texture_coefficients.texture());
    program.bind("b_spec_4f",             scene.components.upliftings.gl.texture_spectra);
    program.bind("b_warp_1f",             scene.components.upliftings.gl.texture_warp);
    program.bind("b_cmfs_3f",             scene.resources.observers.gl.cmfs_texture);
    program.bind("b_illm_1f",             scene.resources.illuminants.gl.spec_texture);
    if (!scene.resources.images.empty()) {
      program.bind("b_buff_textures", scene.resources.images.gl.texture_info);
      program.bind("b_txtr_1f",       scene.resources.images.gl.texture_atlas_1f.texture());
      program.bind("b_txtr_3f",       scene.resources.images.gl.texture_atlas_3f.texture());
    }
    if (!scene.resources.meshes.empty()) {
      program.bind("b_buff_meshes",        scene.resources.meshes.gl.mesh_info);
      program.bind("b_buff_bvhs_node",     scene.resources.meshes.gl.bvh_nodes);
      program.bind("b_buff_bvhs_prim",     scene.resources.meshes.gl.bvh_prims);
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
    std::tie(m_cache_key, std::ignore) = m_cache_handle.getw<gl::ProgramCache>().set({ 
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
      m_output     = {{ .size = buffer_size, .flags = buffer_create_flags_read }};
      m_output_map = m_output.map_as<RayRecord>(buffer_access_flags_read).data();
    }

    // Draw relevant program from cache
    auto &program = m_cache_handle.getw<gl::ProgramCache>().at(m_cache_key);

    // Bind required resources to their corresponding targets
    program.bind();
    program.bind("b_buff_output",         m_output);
    program.bind("b_buff_sensor",         sensor.buffer());
    program.bind("b_buff_objects",        scene.components.objects.gl.object_info);
    program.bind("b_buff_emitters",       scene.components.emitters.gl.emitter_info);
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