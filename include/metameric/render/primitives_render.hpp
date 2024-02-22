#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/render/detail/primitives.hpp>

namespace met {
  // Helper struct for creation of GBufferPrimitive
  struct GBufferRenderPrimitiveInfo {
    // Program cache; enforced given the shader's long compile time
    ResourceHandle cache_handle;
  };

  // Helper struct for creation of PathRenderPrimitive
  struct PathRenderPrimitiveInfo {
    // Number of samples per pixel when a renderer primitive is invoked
    uint spp_per_iter = 1;

    // Renderer primitives will accumulate up to this number. Afterwards
    // the target is left unmodified. If set to 0, no limit is imposed.
    uint spp_max = std::numeric_limits<uint>::max();

    // Maximum path length
    uint max_depth = PathRecord::path_max_depth;

    // Program cache; enforced given the shader's long compile time
    ResourceHandle cache_handle;
  };

  // Rendering primitive; implementation of a simple gbuffer builder
  class GBufferPrimitive {

  public:

  };
  	
  // Rendering primitive; implementation of a unidirectional spectral path
  // tracer with next-event-estimation and four-wavelength sampling.
  class PathRenderPrimitive : public detail::IntegrationRenderPrimitive {
    // Handle to program cache, and key for relevant program
    ResourceHandle  m_cache_handle;
    std::string     m_cache_key; 
    gl::ComputeInfo m_dispatch;

  public:
    using InfoType = PathRenderPrimitiveInfo;
    
    PathRenderPrimitive(InfoType info);

    void reset(const Sensor &sensor, const Scene &scene) override;
    const gl::Texture2d4f &render(const Sensor &sensor, const Scene &scene) override;
  };
} // namespace met