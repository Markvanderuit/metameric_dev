#pragma once

#include <metameric/core/ray.hpp> // TODO discard
#include <metameric/core/scheduler.hpp>
#include <metameric/render/detail/primitives.hpp>
#include <metameric/render/path.hpp>

namespace met {
  // Helper object for creation of FullPathQueryPrimitive and PartialPathQueryPrimitive
  struct PathQueryPrimitiveCreateInfo {
    // Maximum path length
    uint max_depth = path_max_depth;
    
    // Program cache; enforced given the shader's long compile time
    ResourceHandle cache_handle;
  };

  // Helper object for invocation of FullPathQueryPrimitive and PartialPathQueryPrimitive
  struct PathQueryPrimitiveInvokeInfo {
    // Samples to take on invocation
    uint spp = 1u;

    // Necessary references
    const RaySensor &sensor;
    const Scene     &scene;
  };
  
  // Primitive to query light transport along a single ray and get information
  // on each path
  struct FullPathQueryPrimitive : public detail::BaseQueryPrimitive {
    ResourceHandle m_cache_handle;
    std::string    m_cache_key; 
    uint           m_max_depth;

    // Output data mappings and sync objects
    uint                   *m_output_head_map;
    std::span<PathRecord>         m_output_data_map;
    mutable gl::sync::Fence m_output_sync;
    
  public:
    using InfoType = PathQueryPrimitiveCreateInfo;

    FullPathQueryPrimitive() = default;
    FullPathQueryPrimitive(InfoType info);
    
    // Take n samples and return output buffer
    const gl::Buffer &query(const RaySensor &sensor, const Scene &scene, uint spp);

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
    std::span<PathRecord>         m_output_data_map;
    mutable gl::sync::Fence m_output_sync;

  public:
    using InfoType = PathQueryPrimitiveCreateInfo;

    PartialPathQueryPrimitive() = default;
    PartialPathQueryPrimitive(InfoType info);

    // Take n samples and return output buffer
    const gl::Buffer &query(const RaySensor &sensor, const Scene &scene, uint spp);

    // Wait for sync object, and then return output data
    std::span<const PathRecord> data() const;
  };

  // Helper object for creation of RayQueryPrimitive
  struct RayQueryPrimitiveCreateInfo {
    // Program cache; enforced given the shader's long compile time
    ResourceHandle cache_handle;
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
    using InfoType = RayQueryPrimitiveCreateInfo;

    RayQueryPrimitive() = default;
    RayQueryPrimitive(InfoType info);

    // Take 1 sample and return output buffer
    const gl::Buffer &query(const RaySensor &sensor, const Scene &scene);

    // Wait for sync object, and then return output data
    const RayRecord &data() const;
  };
} // namespace met