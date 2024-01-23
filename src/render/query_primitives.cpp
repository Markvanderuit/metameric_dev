#include <metameric/core/utility.hpp>
#include <metameric/render/query_primitives.hpp>

namespace met {
  PathQueryPrimitive::PathQueryPrimitive(PathQueryPrimitiveCreateInfo info) {
    met_trace_full();

    // Initialize program object
    m_program = {{ .type       = gl::ShaderType::eCompute,
                   .spirv_path = "resources/shaders/render/primitive_path.comp.spv",
                   .cross_path = "resources/shaders/render/primitive_path.comp.json",
                   .spec_const = {{ 0u, 256u           },
                                  { 1u, 1u             },
                                  { 2u, info.max_depth },
                                  { 3u, false          }} }};
  }

  const gl::Buffer &PathQueryPrimitive::query(const PathQuery &sensor, const Scene &scene) {
    met_trace_full();

    // Resize output buffer to accomodate requested nr. of paths
    size_t buffer_size = sensor.n_paths * sizeof(PathInfo);
    if (!m_paths.is_init() || m_paths.size() != buffer_size) {
      m_paths = {{ .size = buffer_size }};
    }

    


    return m_paths;
  }
} // namespace met