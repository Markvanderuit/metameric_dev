#pragma once

#include <metameric/core/scene.hpp>
#include <metameric/render/path.hpp>
#include <metameric/render/sensor.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/utility.hpp>

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
      
      // Rolling set of mapped buffers that track incrementing sampler state over several frames
      std::array<gl::Buffer,      6> m_sampler_state_buffs;
      std::array<SamplerState *,  6> m_sampler_state_mapps;
      std::array<gl::sync::Fence, 6> m_sampler_state_syncs;
      uint                           m_sampler_state_i;

    protected:
      virtual void reset(const Sensor &sensor, const Scene &scene) override;

      void advance_sampler_state();
      const gl::Buffer &get_sampler_state();

      uint          m_spp_max;
      uint          m_spp_curr;
      uint          m_spp_per_iter;

    public:
      BaseIntegrationRenderer();
    };
    
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

  struct DirectRenderPrimitiveCreateInfo {
    // Number of samples per pixel when a renderer primitive is invoked
    uint spp_per_iter = 1;

    // Renderer primitives will accumulate up to this number. Afterwards
    // the target is left unmodified. If set to 0, no limit is imposed.
    uint spp_max = std::numeric_limits<uint>::max();
  };

  class DirectRenderPrimitive : public detail::BaseIntegrationRenderer {
    detail::GBuffer m_gbuffer;
    
  public:
    using InfoType = DirectRenderPrimitiveCreateInfo;
    
    gl::Program     m_program;
    gl::ComputeInfo m_dispatch;

  public:
    DirectRenderPrimitive(DirectRenderPrimitiveCreateInfo info);

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
  };

  class PathRenderPrimitive : public detail::BaseIntegrationRenderer {
    gl::ComputeInfo m_dispatch;
    gl::Program     m_program;

  public:
    using InfoType = PathRenderPrimitiveCreateInfo;
    
    PathRenderPrimitive(PathRenderPrimitiveCreateInfo info);

    void reset(const Sensor &sensor, const Scene &scene) override;
    const gl::Texture2d4f &render(const Sensor &sensor, const Scene &scene) override;
  };
} // namespace met