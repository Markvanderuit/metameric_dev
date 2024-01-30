#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/render/detail/primitives.hpp>
#include <metameric/render/path.hpp>

namespace met {
  struct DirectRenderPrimitiveCreateInfo {
    // Number of samples per pixel when a renderer primitive is invoked
    uint spp_per_iter = 1;

    // Renderer primitives will accumulate up to this number. Afterwards
    // the target is left unmodified. If set to 0, no limit is imposed.
    uint spp_max = std::numeric_limits<uint>::max();
  };

  class DirectRenderPrimitive : public detail::IntegrationRenderPrimitive {
    detail::GBufferRenderPrimitive m_gbuffer;
    
  public:
    using InfoType = DirectRenderPrimitiveCreateInfo;
    
    gl::Program     m_program;
    gl::ComputeInfo m_dispatch;

  public:
    DirectRenderPrimitive(InfoType info);

    void reset(const Sensor &sensor, const Scene &scene) override;
    const gl::Texture2d4f &render(const Sensor &sensor, const Scene &scene) override;
  };

  struct PathRenderPrimitiveCreateInfo {
    // Number of samples per pixel when a renderer primitive is invoked
    uint spp_per_iter = 1;

    // Renderer primitives will accumulate up to this number. Afterwards
    // the target is left unmodified. If set to 0, no limit is imposed.
    uint spp_max = std::numeric_limits<uint>::max();

    // Maximum path length
    uint max_depth = path_max_depth;

    // Program cache; enforced given the shader's long compile time
    ResourceHandle cache_handle;
  };

  class PathRenderPrimitive : public detail::IntegrationRenderPrimitive {
    // Handle to program cache, and key for relevant program
    ResourceHandle  m_cache_handle;
    std::string     m_cache_key; 
    gl::ComputeInfo m_dispatch;

  public:
    using InfoType = PathRenderPrimitiveCreateInfo;
    
    PathRenderPrimitive(InfoType info);

    void reset(const Sensor &sensor, const Scene &scene) override;
    const gl::Texture2d4f &render(const Sensor &sensor, const Scene &scene) override;
  };
} // namespace met