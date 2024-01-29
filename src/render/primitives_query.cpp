#include <metameric/core/utility.hpp>
#include <metameric/render/primitives_query.hpp>

namespace met {
  static gl::ProgramCache<gl::ShaderLoadSPIRVInfo> program_cache;

  constexpr static auto buffer_create_flags = gl::BufferCreateFlags::eMapReadPersistent;
  constexpr static auto buffer_access_flags = gl::BufferAccessFlags::eMapReadPersistent;

  FullPathQueryPrimitive::FullPathQueryPrimitive(PathQueryPrimitiveCreateInfo info) {
    met_trace_full();

    // Initialize program object
    m_program_key = program_cache.set({ 
      .type       = gl::ShaderType::eCompute,
      .spirv_path = "resources/shaders/render/primitive_query_path_full.comp.spv",
      .cross_path = "resources/shaders/render/primitive_query_path_full.comp.json",
      .spec_const = {{ 0u, 256u           },
                     { 1u, info.max_depth }}
    });
  }

  const gl::Buffer &FullPathQueryPrimitive::query(const RaySensor &sensor, const Scene &scene) {
    met_trace_full();

    // Resize output buffer to accomodate requested nr. of paths
    // and generate read-only map
    size_t buffer_size = 4 
                       * sizeof(uint) 
                       + sensor.n_paths 
                       * (path_max_depth - 1)  // Given NEE, several paths are generated per path
                       * sizeof(Path);
    if (!m_output.is_init() || m_output.size() != buffer_size) {
      m_output = {{ .size = buffer_size, .flags = buffer_create_flags }};
      auto map = m_output.map(buffer_access_flags);
      m_output_head_map = cast_span<uint>(map).data();
      m_output_data_map = cast_span<Path>(map.subspan(4 * sizeof(uint)));
    }

    // Clear output data, specifically the buffer's head
    m_output.clear({ }, 1u, 4 * sizeof(uint));

    // Get relevant program
    auto &program = program_cache.at(m_program_key);

    // Bind required resources to their corresponding targets
    program.bind();
    program.bind("b_buff_path_data",      m_output);
    program.bind("b_buff_sensor",         sensor.buffer());
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
      program.bind("b_buff_meshes",        scene.resources.meshes.gl.mesh_info);
      program.bind("b_buff_bvhs_node",     scene.resources.meshes.gl.bvh_nodes);
      program.bind("b_buff_bvhs_prim",     scene.resources.meshes.gl.bvh_prims);
    }

    // Dispatch compute shader
    gl::sync::memory_barrier( gl::BarrierFlags::eImageAccess   | gl::BarrierFlags::eTextureFetch  |
                              gl::BarrierFlags::eUniformBuffer | gl::BarrierFlags::eStorageBuffer | 
                              gl::BarrierFlags::eBufferUpdate                                     );
    gl::dispatch_compute({ .groups_x = ceil_div(sensor.n_paths, 256u) });

    // Insert memory barrier and submit a fence object to ensure
    // output data is made visible in mapped region
    gl::sync::memory_barrier(gl::BarrierFlags::eClientMappedBuffer);
    m_output_sync = gl::sync::Fence(gl::sync::time_s(1));

    return m_output;
  }

  std::span<const Path> FullPathQueryPrimitive::data() {
    // Wait for output data to be visible, and then
    // return generated output data
    m_output_sync.cpu_wait();
    return m_output_data_map.subspan(0, *m_output_head_map);
  }

  PartialPathQueryPrimitive::PartialPathQueryPrimitive(PathQueryPrimitiveCreateInfo info) {
    met_trace_full();

    // Initialize program object
    m_program_key = program_cache.set({ 
      .type       = gl::ShaderType::eCompute,
      .spirv_path = "resources/shaders/render/primitive_query_path_partial.comp.spv",
      .cross_path = "resources/shaders/render/primitive_query_path_partial.comp.json",
      .spec_const = {{ 0u, 256u           },
                     { 1u, info.max_depth }}
    });
  }

  const gl::Buffer &PartialPathQueryPrimitive::query(const RaySensor &sensor, const Scene &scene) {
    met_trace_full();

    // Resize output buffer to accomodate requested nr. of paths
    // and generate read-only map
    size_t buffer_size = 4 
                       * sizeof(uint) 
                       + sensor.n_paths 
                       * (path_max_depth - 1)  // Given NEE, several paths are generated per path
                       * sizeof(Path);
    if (!m_output.is_init() || m_output.size() != buffer_size) {
      m_output = {{ .size = buffer_size, .flags = buffer_create_flags }};
      auto map = m_output.map(buffer_access_flags);
      m_output_head_map = cast_span<uint>(map).data();
      m_output_data_map = cast_span<Path>(map.subspan(4 * sizeof(uint)));
    }

    // Clear output data, specifically the buffer's head
    m_output.clear({ }, 1u, 4 * sizeof(uint));

    // Get relevant program
    auto &program = program_cache.at(m_program_key);

    // Bind required resources to their corresponding targets
    program.bind();
    program.bind("b_buff_path_data",      m_output);
    program.bind("b_buff_sensor",         sensor.buffer());
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
      program.bind("b_buff_meshes",        scene.resources.meshes.gl.mesh_info);
      program.bind("b_buff_bvhs_node",     scene.resources.meshes.gl.bvh_nodes);
      program.bind("b_buff_bvhs_prim",     scene.resources.meshes.gl.bvh_prims);
    }

    // Dispatch compute shader
    gl::sync::memory_barrier( gl::BarrierFlags::eImageAccess   | gl::BarrierFlags::eTextureFetch  |
                              gl::BarrierFlags::eUniformBuffer | gl::BarrierFlags::eStorageBuffer | 
                              gl::BarrierFlags::eBufferUpdate                                     );
    gl::dispatch_compute({ .groups_x = ceil_div(sensor.n_paths, 256u) });

    // Insert memory barrier and submit a fence object to ensure
    // output data is made visible in mapped region
    gl::sync::memory_barrier(gl::BarrierFlags::eClientMappedBuffer);
    m_output_sync = gl::sync::Fence(gl::sync::time_s(1));

    return m_output;
  }

  std::span<const Path> PartialPathQueryPrimitive::data() {
    // Wait for output data to be visible, and then
    // return generated output data
    m_output_sync.cpu_wait();
    return m_output_data_map.subspan(0, *m_output_head_map);
  }
} // namespace met