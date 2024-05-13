#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/render/detail/primitives.hpp>
#include <small_gl/texture.hpp>

namespace met {
  // Helper struct for creation of GBufferPrimitive
  struct GBufferPrimitiveInfo {
    // Program cache; enforced given the shader's long compile time
    ResourceHandle cache_handle;
  };

  // Helper struct for creation of GBufferViewPrimitive
  struct GBufferViewPrimitiveInfo {
    enum class GBufferViewType : uint {
      ePosition  = 0,
      eNormals   = 1,
      eTexcoords = 2,
      eObjectIDs = 3
    } view_type = GBufferViewType::ePosition;

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

    // Render pixels each other frame, alternating between checkerboards
    bool pixel_checkerboard = false;

    // Program cache; enforced given the shader's long compile time
    ResourceHandle cache_handle;
  };

  // Helper struct for creation of RGBPathRenderPrimitive
  struct RGBPathRenderPrimitiveInfo {
    // Number of samples per pixel when a renderer primitive is invoked
    uint spp_per_iter = 1;

    // Renderer primitives will accumulate up to this number. Afterwards
    // the target is left unmodified. If set to 0, no limit is imposed.
    uint spp_max = std::numeric_limits<uint>::max();

    // Maximum path length
    uint max_depth = PathRecord::path_max_depth;

    // Render pixels each other frame, alternating between checkerboards
    bool pixel_checkerboard = false;

    // Program cache; enforced given the shader's long compile time
    ResourceHandle cache_handle;
  };

  // Rendering primitive; implementation of a simple gbuffer builder
  class GBufferPrimitive : public detail::BaseRenderPrimitive {
    using DepthBuffer = gl::Renderbuffer<gl::DepthComponent, 1>;

    // Handle to program cache, and key for relevant program
    ResourceHandle  m_cache_handle;
    std::string     m_cache_key; 

    // Internal GL objects
    gl::MultiDrawInfo m_draw;
    gl::ComputeInfo   m_dispatch;
    gl::Framebuffer   m_framebuffer;
        DepthBuffer   m_depthbuffer;

  public:
    using InfoType = GBufferPrimitiveInfo;

    GBufferPrimitive(InfoType info);

    void reset(const Sensor &sensor, const Scene &scene) override;
    const gl::Texture2d4f &render(const Sensor &sensor, const Scene &scene) override;
  };

  // Rendering primitive; ingests an existing gbuffer's film and outputs a
  // film with positions/normals/texcoords or other specified output
  class GBufferViewPrimitive : public detail::BaseRenderPrimitive {
    // Handle to program cache, and key for relevant program
    ResourceHandle  m_cache_handle;
    std::string     m_cache_key; 

    // Internal GL objects
    gl::ComputeInfo m_dispatch;

  private:
    const gl::Texture2d4f &render(const Sensor &sensor, const Scene &scene) override;

  public:
    using InfoType = GBufferViewPrimitiveInfo;

    GBufferViewPrimitive(InfoType info);

    void reset(const Sensor &sensor, const Scene &scene) override;

    // Extensions of render(sensor, scene) s.t. an existing gbuffer is required
    const gl::Texture2d4f &render(const GBufferPrimitive &gbuffer, const Sensor &sensor, const Scene &scene);
    const gl::Texture2d4f &render(const gl::Texture2d4f  &gbuffer, const Sensor &sensor, const Scene &scene);
  };
  	
  // Rendering primitive; implementation of a unidirectional spectral path
  // tracer with next-event-estimation and four-wavelength sampling.
  class PathRenderPrimitive : public detail::IntegrationRenderPrimitive {
    // Handle to program cache, and key for relevant program
    ResourceHandle  m_cache_handle;
    std::string     m_cache_key; 

    // Internal GL objects
    gl::ComputeInfo m_dispatch;

  public:
    using InfoType = PathRenderPrimitiveInfo;
    
    PathRenderPrimitive() = default;
    PathRenderPrimitive(InfoType info);

    void reset(const Sensor &sensor, const Scene &scene) override;
    const gl::Texture2d4f &render(const Sensor &sensor, const Scene &scene) override;
  };
  	
  // Rendering primitive; implementation of a unidirectional spectral path
  // tracer with next-event-estimation and four-wavelength sampling.
  class RGBPathRenderPrimitive : public detail::IntegrationRenderPrimitive {
    using texture_type = gl::Texture1d<float, 3, gl::TextureType::eImageArray>;

    // Handle to program cache, and key for relevant program
    ResourceHandle  m_cache_handle;
    std::string     m_cache_key; 

    // Internal GL objects
    gl::ComputeInfo m_dispatch;

  private: // Helpers for gl-side RGB value overrides for illuminants
    gl::Buffer              m_illm_colr_buffer;
    std::span<eig::Array4f> m_illm_colr_buffer_map;
    texture_type            m_illm_colr_texture;

  public:
    using InfoType = RGBPathRenderPrimitiveInfo;
    
    RGBPathRenderPrimitive() = default;
    RGBPathRenderPrimitive(InfoType info);

    void reset(const Sensor &sensor, const Scene &scene) override;
    const gl::Texture2d4f &render(const Sensor &sensor, const Scene &scene) override;
  };
} // namespace met