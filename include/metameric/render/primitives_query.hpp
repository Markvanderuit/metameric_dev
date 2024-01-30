#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/render/detail/primitives.hpp>
#include <metameric/render/path.hpp>

namespace met {
  struct PathQueryPrimitiveCreateInfo {
    // Maximum path length
    uint max_depth = path_max_depth;
    
    // Program cache; enforced given the shader's long compile time
    ResourceHandle cache_handle;
  };
  
  // Primitive to query light transport along a single ray and get information
  // on each path
  struct FullPathQueryPrimitive : public detail::BaseQueryPrimitive {
    // Handle to program cache, and key for relevant program
    ResourceHandle  m_cache_handle;
    std::string     m_cache_key; 

    // Output data mappings and sync objects
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
    // Handle to program cache, and key for relevant program
    ResourceHandle  m_cache_handle;
    std::string     m_cache_key; 

    // Output data mappings and sync objects
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