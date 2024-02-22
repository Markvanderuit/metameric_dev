#pragma once

#include <metameric/core/ray.hpp> // TODO discard
#include <metameric/core/scene.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/render/detail/primitives.hpp>

namespace met {
  // Helper object for creation of FullPathQueryPrimitive and PartialPathQueryPrimitive
  struct PathQueryPrimitiveInfo {
    // Maximum path length
    uint max_depth = PathRecord::path_max_depth;
    
    // Program cache; enforced given the shader's long compile time
    ResourceHandle cache_handle;
  };

  // Helper object for creation of RayQueryPrimitive
  struct RayQueryPrimitiveInfo {
    // Program cache; enforced given the shader's long compile time
    ResourceHandle cache_handle;
  };
  
  // Primitive to query light transport along a single ray and get information
  // on each path
  struct FullPathQueryPrimitive : public detail::BaseQueryPrimitive {
    ResourceHandle m_cache_handle;
    std::string    m_cache_key; 
    uint           m_max_depth;

    // Output data mappings and sync objects
    uint                   *m_output_head_map;
    std::span<PathRecord>   m_output_data_map;
    mutable gl::sync::Fence m_output_sync;
    
  public:
    using InfoType = PathQueryPrimitiveInfo;

    FullPathQueryPrimitive() = default;
    FullPathQueryPrimitive(InfoType info);
    
    // Take n samples and return output buffer
    const gl::Buffer &query(const PixelSensor &sensor, const Scene &scene, uint spp);

    // Wait for sync object, and then return output data
    std::span<const PathRecord> data() const;
  };

  // Primitive to query light transport along a single ray and get information
  // on each path, with reflectances factored out
  class PartialPathQueryPrimitive : public detail::BaseQueryPrimitive {
    ResourceHandle m_cache_handle;
    std::string    m_cache_key; 
    uint           m_max_depth;

    // Output data mappings and sync objects
    uint                   *m_output_head_map;
    std::span<PathRecord>   m_output_data_map;
    mutable gl::sync::Fence m_output_sync;

  public:
    using InfoType = PathQueryPrimitiveInfo;

    PartialPathQueryPrimitive() = default;
    PartialPathQueryPrimitive(InfoType info);

    // Take n samples and return output buffer
    const gl::Buffer &query(const PixelSensor &sensor, const Scene &scene, uint spp);

    // Wait for sync object, and then return output data
    std::span<const PathRecord> data() const;
  };

  // Primitive to perform a simple raycast
  class RayQueryPrimitive : public detail::BaseQueryPrimitive {
    // Handle to program cache, and key for relevant program
    ResourceHandle  m_cache_handle;
    std::string     m_cache_key; 

    // Output data mappings and sync objects
    RayRecord              *m_output_map;
    mutable gl::sync::Fence m_output_sync;

  public:
    using InfoType = RayQueryPrimitiveInfo;

    RayQueryPrimitive() = default;
    RayQueryPrimitive(InfoType info);

    // Take 1 sample and return output buffer
    const gl::Buffer &query(const RaySensor &sensor, const Scene &scene);

    // Wait for sync object, and then return output data
    const RayRecord &data() const;
  };
} // namespace met