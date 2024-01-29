#pragma once

#include <metameric/render/detail/primitives.hpp>
#include <metameric/render/path.hpp>

namespace met {
  struct PathQueryPrimitiveCreateInfo {
    // Maximum path length
    uint max_depth = path_max_depth;
  };
  
  // Primitive to query light transport along a single ray and get information
  // on each path
  struct FullPathQueryPrimitive : public detail::BaseQueryPrimitive {
    gl::Program     m_program;
    uint           *m_output_head_map;
    std::span<Path> m_output_data_map;
    gl::sync::Fence m_output_sync;

  public:
    using InfoType = PathQueryPrimitiveCreateInfo;

    FullPathQueryPrimitive() = default;
    FullPathQueryPrimitive(InfoType info);
    
    std::span<const Path> data();

    const gl::Buffer &query(const RaySensor &sensor, const Scene &scene) override;
  };

  // Primitive to query light transport along a single ray and get information
  // on each path, with reflectances factored out
  class PartialPathQueryPrimitive : public detail::BaseQueryPrimitive {
    gl::Program     m_program;
    uint           *m_output_head_map;
    std::span<Path> m_output_data_map;
    gl::sync::Fence m_output_sync;

  public:
    using InfoType = PathQueryPrimitiveCreateInfo;

    PartialPathQueryPrimitive() = default;
    PartialPathQueryPrimitive(InfoType info);

    std::span<const Path> data();

    const gl::Buffer &query(const RaySensor &sensor, const Scene &scene) override;
  };
} // namespace met