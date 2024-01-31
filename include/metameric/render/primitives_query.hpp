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
  
  // Primitive to query light transport along a single ray and get information
  // on each path
  struct FullPathQueryPrimitive : public detail::BaseQueryPrimitive {
    // Handle to program cache, and key for relevant program
    ResourceHandle  m_cache_handle;
    std::string     m_cache_key; 

    // Output data mappings and sync objects
    uint                   *m_output_head_map;
    std::span<Path>         m_output_data_map;
    mutable gl::sync::Fence m_output_sync;

  public:
    using InfoType = PathQueryPrimitiveCreateInfo;

    FullPathQueryPrimitive() = default;
    FullPathQueryPrimitive(InfoType info);
    
    std::span<const Path> data() const;

    const gl::Buffer &query(const RaySensor &sensor, const Scene &scene) override;
  };

  // Primitive to query light transport along a single ray and get information
  // on each path, with reflectances factored out
  class PartialPathQueryPrimitive : public detail::BaseQueryPrimitive {
    // Handle to program cache, and key for relevant program
    ResourceHandle  m_cache_handle;
    std::string     m_cache_key; 

    // Output data mappings and sync objects
    uint                   *m_output_head_map;
    std::span<Path>         m_output_data_map;
    mutable gl::sync::Fence m_output_sync;

  public:
    using InfoType = PathQueryPrimitiveCreateInfo;

    PartialPathQueryPrimitive() = default;
    PartialPathQueryPrimitive(InfoType info);

    std::span<const Path> data() const;

    const gl::Buffer &query(const RaySensor &sensor, const Scene &scene) override;
  };

  // Helper object for creation of RayQueryPrimitive
  struct RayQueryPrimitiveCreateInfo {
    // Program cache; enforced given the shader's long compile time
    ResourceHandle cache_handle;
  };

  // Primitive to perform a simple raycast
  struct RayQueryPrimitive : public detail::BaseQueryPrimitive {
    // GLSL-side ray structure
    struct Ray {
      eig::Vector3f o;
      float         t;
      eig::Vector3f d;
      uint          data;
    };
    static_assert(sizeof(Ray) == 32);

  private:
    // Handle to program cache, and key for relevant program
    ResourceHandle  m_cache_handle;
    std::string     m_cache_key; 

    // Output data mappings and sync objects
    uint                   *m_output_head_map;
    std::span<Ray>          m_output_data_map;
    mutable gl::sync::Fence m_output_sync;

  public:
    using InfoType = RayQueryPrimitiveCreateInfo;

    RayQueryPrimitive() = default;
    RayQueryPrimitive(InfoType info);

    std::span<const Ray> data() const;

    const gl::Buffer &query(const RaySensor &sensor, const Scene &scene) override;
  };
} // namespace met