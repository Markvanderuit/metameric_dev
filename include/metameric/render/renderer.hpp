#pragma once

#include <metameric/render/ray_primitives.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/texture.hpp>

namespace met {
  // Virtual base class
  namespace detail {
    class BaseRenderer {
    protected:
      gl::Texture2d4f m_output;

    public: // Output target management
      const gl::Texture2d4f &output() const {
        return m_output;
      }

      void resize_output(const eig::Array2u &size) {
        m_output = {{ .size = size }};
      }
    };

    struct BaseIntegrationRenderer : public BaseRenderer {
      // Reset internal state so the output image is blank
      // and the next sample is the first to be taken
      virtual void reset() = 0;

      // Take a sample and add it into the output image
      // virtual void sample(SceneResourceHandles scene_handles) = 0;
    };
    
    class GBufferRenderer : public BaseRenderer {
      using Depthbuffer = gl::Renderbuffer<gl::DepthComponent, 1>;
      
      struct UnifLayout { eig::Matrix4f trf; };

      UnifLayout       *m_unif_buffer_map;
      gl::Buffer        m_unif_buffer;
      Depthbuffer       m_fbo_depth;
      gl::Framebuffer   m_fbo;
      gl::Program       m_program;
      gl::MultiDrawInfo m_draw;

    public:
      // void render(SceneResourceHandles scene_handles);
    };
  } // namespace detail

  struct DirectRendererCreateInfo {
    // ...
  };

  struct PathRendererCreateInfo {
    uint max_depth = 10;
    // ...
  };

  struct DirectRenderer : public detail::BaseIntegrationRenderer {
    detail::GBufferRenderer m_gbuffer;

  public:
    using InfoType = DirectRendererCreateInfo;

  public:
    DirectRenderer(DirectRendererCreateInfo info);

    void reset() override;
    // void sample(SceneResourceHandles scene_handles) override;
  };

  struct PathRenderer : public detail::BaseIntegrationRenderer {
    detail::GBufferRenderer  m_gbuffer;
    // RayIntersectAnyPrimitive m_ray_intersect_any;
    // RayIntersectPrimitive    m_ray_intersect;

  public:
    using InfoType = PathRendererCreateInfo;

  public:
    PathRenderer(PathRendererCreateInfo info);
    
    void reset() override;
    // void sample(SceneResourceHandles scene_handles) override;
  };
} // namespace met