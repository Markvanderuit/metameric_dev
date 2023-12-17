#include <metameric/render/ray_primitives.hpp>

namespace met {
  constexpr uint isct_wg_size = 256;
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  DispatchDividePrimitive::DispatchDividePrimitive(uint div) {
    m_program = {{ .type       = gl::ShaderType::eCompute,
                   .spirv_path = "resources/shaders/render/primitive_dispatch_divide.comp.spv",
                   .cross_path = "resources/shaders/render/primitive_dispatch_divide.comp.json",
                   .spec_const = {{ 0, div }} }};
  }

  void DispatchDividePrimitive::invoke(
    const gl::Buffer &input, 
          gl::Buffer &output,
          size_t      offs, 
          size_t      size) {
    met_trace_full();

    m_program.bind();
    m_program.bind("b_input",  input);
    m_program.bind("b_output", output);
    
    gl::sync::memory_barrier(gl::BarrierFlags::eStorageBuffer);
    gl::dispatch_compute({ .groups_x = 1 });
  }
  
  void DispatchDividePrimitive::invoke(
    const gl::Buffer &input, 
          gl::Buffer &output,
    const gl::Buffer &count) {
    // Forward to other function, ignore count
    invoke(input, output, 0, 0);
  }

  RayIntersectPrimitive::RayIntersectPrimitive()
  : m_prim_ddiv(isct_wg_size) {
    met_trace_full();
    m_program = {{ .type       = gl::ShaderType::eCompute,
                   .spirv_path = "resources/shaders/render/primitive_ray_intersect.comp.spv",
                   .cross_path = "resources/shaders/render/primitive_ray_intersect.comp.json",
                   .spec_const = {{ 0, isct_wg_size   }} }};
    m_buffer_count     = {{ .size = sizeof(uint), .flags = buffer_create_flags }};
    m_buffer_count_map = m_buffer_count.map_as<BufferLayout>(buffer_access_flags).data();
  }

  void RayIntersectPrimitive::invoke(
    const gl::Buffer &input, 
          gl::Buffer &output,
          size_t      offs, 
          size_t      size) {
    met_trace_full();

    // Update internal count buffer
    // Ignore offset for now
    m_buffer_count_map->n = size;
    m_buffer_count.flush();

    // Forward to buffer invoke
    invoke(input, output, m_buffer_count);
  }
  
  void RayIntersectPrimitive::invoke(
    const gl::Buffer &input, 
          gl::Buffer &output,
    const gl::Buffer &count) {
    met_trace_full();

    // ...
    
    // Forward provided work count buffer and use for indirect dispatch
    gl::dispatch_compute({ .buffer = &m_prim_ddiv(count), .bindable_program = &m_program });
  }

  RayIntersectAnyPrimitive::RayIntersectAnyPrimitive()
  : m_prim_ddiv(isct_wg_size) {
    met_trace_full();
    m_program = {{ .type       = gl::ShaderType::eCompute,
                   .spirv_path = "resources/shaders/render/primitive_ray_intersect_any.comp.spv",
                   .cross_path = "resources/shaders/render/primitive_ray_intersect_any.comp.json",
                   .spec_const = {{ 0, isct_wg_size   }} }};
    m_buffer_count     = {{ .size = sizeof(eig::Array4u), .flags = buffer_create_flags }};
    m_buffer_count_map = m_buffer_count.map_as<BufferLayout>(buffer_access_flags, 1u).data();
  }

  void RayIntersectAnyPrimitive::invoke(
    const gl::Buffer &input, 
          gl::Buffer &output,
          size_t      offs, 
          size_t      size) {
    met_trace_full();

    // Update internal count buffer
    // Ignore offset for now
    m_buffer_count_map->n = size;
    m_buffer_count.flush();

    // Forward to buffer invoke
    invoke(input, output, m_buffer_count);
  }
  
  void RayIntersectAnyPrimitive::invoke(
    const gl::Buffer &input, 
          gl::Buffer &output,
    const gl::Buffer &count) {
    met_trace_full();
    // ...
  }
} // namespace met