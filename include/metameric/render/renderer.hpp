#pragma once

#include <metameric/core/scene.hpp>
#include <metameric/render/sensor.hpp>
#include <metameric/render/ray_primitives.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/texture.hpp>

namespace met {
  namespace detail {
    // Renderer base class
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

    // Render target base class; a render target can be anything
    // the renderer may wish to write to, such as a film, texture, or other image type. In our
    // case, it might also be path storage buffers for building cached raytracing kernels.
    struct BaseRenderTarget {

    };

    struct TextureRenderTarget : BaseRenderTarget {

    };

    struct PathKernelRenderTarget : BaseRenderTarget {

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
      // GBufferRenderer(const Scene &scene, const Sensor &sensor);
      
      // void render(SceneResourceHandles scene_handles);
    };
  } // namespace detail

  struct DirectRendererCreateInfo {
    // Relevant scene data
    const Scene  &scene;
    const Sensor &sensor;

    // Number of samples taken on calls of renderer.sample().
    uint samples_per_iter = 1;

    // The renderer will only render up to 'max_samples' on calls of renderer.sample(). Afterwards,
    // the current rendered image will simply be returned immediately. If set to 0, this does not
    // occur.
    uint max_samples = 0;
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