#pragma once

#include <metameric/core/scene.hpp>
#include <metameric/render/sensor.hpp>
#include <metameric/render/ray_primitives.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/renderbuffer.hpp>

namespace met {
  namespace detail {
    // Renderer base class
    class BaseRenderer {
    protected:
      gl::Texture2d4f m_film; // Color render target

    public:
      const gl::Texture2d4f &film() const { return m_film; }

      virtual const gl::Texture2d4f &render(const Sensor &sensor, const Scene &scene) = 0;
      virtual void reset(const Sensor &sensor, const Scene &scene) = 0;
    };

    class BaseIntegrationRenderer : public BaseRenderer {
      struct SamplerState {
        alignas(4) uint spp_per_iter;
        alignas(4) uint spp_curr;
      };
      
    protected:
      uint          m_spp_max;
      gl::Buffer    m_sampler_data;
      gl::Buffer    m_sampler_state;
      SamplerState *m_sampler_state_map;

      void init_sampler_state(uint num_pixels);
      bool has_next_sampler_state() const;
      void next_sampler_state();

    public:
      BaseIntegrationRenderer();
    };

    /* // Render target base class; a render target can be anything
    // the renderer may wish to write to, such as a film, texture, or other image type. In our
    // case, it might also be path storage buffers for building cached raytracing kernels.
    struct BaseRenderTarget {

    };

    struct TextureRenderTarget : BaseRenderTarget {

    };

    struct PathKernelRenderTarget : BaseRenderTarget {

    }; */

    
    // Helper class to build a quick first-intersection gbuffer
    class GBuffer : public BaseRenderer {
      using Depthbuffer = gl::Renderbuffer<gl::DepthComponent, 1>;

      Depthbuffer       m_fbo_depth;
      gl::Framebuffer   m_fbo;
      gl::Program       m_program;
      gl::MultiDrawInfo m_draw;

    public:
      GBuffer();

      const gl::Texture2d4f &render(const Sensor &sensor, const Scene &scene) override;
      void reset(const Sensor &sensor, const Scene &scene) override;
    };
  } // namespace detail

  struct DirectRendererCreateInfo {
    // Number of samples per pixel when a renderer primitive is invoked
    uint spp_per_iter = 1;

    // Renderer primitives will accumulate up to this number. Afterwards
    // the target is left unmodified. If set to 0, no limit is imposed.
    uint spp_max = std::numeric_limits<uint>::max();
  };

  class DirectRenderer : public detail::BaseIntegrationRenderer {
    detail::GBuffer m_gbuffer;
    
  public:
    using InfoType = DirectRendererCreateInfo;
    
    gl::Program     m_program;
    gl::ComputeInfo m_dispatch;

  public:
    DirectRenderer(DirectRendererCreateInfo info);

    const gl::Texture2d4f &render(const Sensor &sensor, const Scene &scene) override;
    void reset(const Sensor &sensor, const Scene &scene) override;
  };

  /* struct PathRendererCreateInfo {
    // Number of samples per pixel when a renderer primitive is invoked
    uint spp = 1;

    // Renderer primitives will accumulate up to this number. Afterwards
    // the target is left unmodified. If set to 0, no limit is imposed.
    uint spp_max = std::numeric_limits<uint>::max();

    // ...
    uint path_depth = 10;
  }; */

  /* class PathRenderer {
    detail::GBuffer  m_gbuffer;
    // RayIntersectAnyPrimitive m_ray_intersect_any;
    // RayIntersectPrimitive    m_ray_intersect;

  public:
    using InfoType = PathRendererCreateInfo;

    PathRenderer(PathRendererCreateInfo info);

    void render(Sensor    &sensor, const Scene &scene) const;
    void render(PathCache &paths,  const Scene &scene) const;
  }; */
} // namespace met