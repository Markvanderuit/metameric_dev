#pragma once

#include <metameric/core/scene.hpp>
#include <metameric/render/sensor.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met::detail {
  // Renderer base class
  class BaseRenderPrimitive {
  protected:
    gl::Texture2d4f m_film; // Color render target

  public:
    const gl::Texture2d4f &film() const { return m_film; }

    virtual const gl::Texture2d4f &render(const Sensor &sensor, const Scene &scene) = 0;
    virtual void reset(const Sensor &sensor, const Scene &scene) = 0;
  };

  // Query base class; queries track and return one or more paths or rays
  class BaseQueryPrimitive {
  protected:
    gl::Buffer m_output; // Query render target

  public:
    const gl::Buffer &output() const { return m_output; }
    virtual const gl::Buffer &query(const RaySensor &sensor, const Scene &scene) = 0;
  };

  // Repeated sampling renderer base class
  class IntegrationRenderPrimitive : public BaseRenderPrimitive {
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
    virtual const gl::Texture2d4f &render(const Sensor &sensor, const Scene &scene) override; // default-implemented
    virtual void reset(const Sensor &sensor, const Scene &scene) override;

    void advance_sampler_state();
    const gl::Buffer &get_sampler_state();

    uint m_spp_max;
    uint m_spp_curr;
    uint m_spp_per_iter;

  public:
    IntegrationRenderPrimitive();

    bool has_next_sample_state() const {
      return m_spp_max == 0 || m_spp_curr < m_spp_max;
    }
  };
  
  // Helper class to build a quick first-intersection gbuffer
  class GBufferRenderPrimitive : public BaseRenderPrimitive {
    using Depthbuffer = gl::Renderbuffer<gl::DepthComponent, 1>;

    Depthbuffer       m_fbo_depth;
    gl::Framebuffer   m_fbo;
    gl::Program       m_program;
    gl::MultiDrawInfo m_draw;

  public:
    GBufferRenderPrimitive();

    const gl::Texture2d4f &render(const Sensor &sensor, const Scene &scene) override;
    void reset(const Sensor &sensor, const Scene &scene) override;
  };
} // namespace met::detail