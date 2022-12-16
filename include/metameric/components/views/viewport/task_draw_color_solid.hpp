#pragma once

#include <metameric/core/mesh.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>

namespace met {
  class DrawColorSolidTask : public detail::AbstractTask {
    // Shorthands for multisampled framebuffer attachment types
    using Colorbuffer = gl::Renderbuffer<
      float, 
      4, 
      gl::RenderbufferType::eMultisample
    >;
    using Depthbuffer = gl::Renderbuffer<
      gl::DepthComponent, 
      1,
      gl::RenderbufferType::eMultisample
    >;

    // State information
    std::string m_parent;

    // Convex hull mesh generation data
    HalfedgeMesh m_sphere_mesh;
    HalfedgeMesh m_csolid_mesh;

    // (Multisampled) framebuffer and attachments
    Colorbuffer     m_color_buffer_ms;
    Depthbuffer     m_depth_buffer_ms;
    gl::Framebuffer m_frame_buffer_ms;
    gl::Framebuffer m_frame_buffer;
    
    // Mesh draw components
    gl::Buffer    m_chull_verts;
    gl::Buffer    m_chull_elems;
    gl::Array     m_point_array;
    gl::Array     m_chull_array;
    gl::DrawInfo  m_point_dispatch;
    gl::DrawInfo  m_chull_dispatch;
    gl::Program   m_draw_program;

    // Gamma correction components
    gl::ComputeInfo m_srgb_dispatch;
    gl::Program     m_srgb_program;
    gl::Sampler     m_srgb_sampler;

  public:
    DrawColorSolidTask(const std::string &name, const std::string &parent);
    void init(detail::TaskInitInfo &info) override;
    void eval(detail::TaskEvalInfo &info) override;
  };
} // namespace met