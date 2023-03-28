#pragma once

#include <metameric/core/mesh.hpp>
#include <metameric/core/scheduler.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>

namespace met {
  class DrawColorSolidTask : public detail::TaskNode {
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

    struct CnstrUniformBuffer {
      alignas(64) eig::Matrix4f model_matrix;
      alignas(64) eig::Matrix4f camera_matrix;
      alignas(16) eig::Vector4f point_color;
      alignas(16) eig::Vector3f point_position;
      alignas(8)  eig::Vector2f point_aspect;
      alignas(4)  float         point_size;
    };

    struct DrawUniformBuffer {
      alignas(64) eig::Matrix4f model_matrix;
      alignas(64) eig::Matrix4f camera_matrix;
      alignas(4)  float alpha;
    };

    struct SrgbUniformBuffer {
      alignas(8) eig::Array2u size;
      alignas(4) uint lrgb_to_srgb;
    };

    // State information
    std::string m_parent;

    // Constraint-point draw components
    gl::Buffer      m_quad_verts;
    gl::Buffer      m_quad_elems;
    
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
    gl::Array     m_cnstr_array;
    gl::DrawInfo  m_cnstr_dispatch;
    gl::DrawInfo  m_point_dispatch;
    gl::DrawInfo  m_chull_dispatch;
    gl::Program   m_cnstr_program;
    gl::Program   m_draw_program;
    gl::Buffer    m_draw_uniform_buffer;
    gl::Buffer    m_cnstr_uniform_buffer;
    DrawUniformBuffer *m_draw_uniform_map;
    CnstrUniformBuffer *m_cnstr_uniform_map;

    // Gamma correction components
    gl::ComputeInfo m_srgb_dispatch;
    gl::Program     m_srgb_program;
    gl::Sampler     m_srgb_sampler;
    gl::Buffer      m_srgb_uniform_buffer;
    SrgbUniformBuffer *m_srgb_uniform_map;

  public:
    DrawColorSolidTask(const std::string &parent);
    void init(SchedulerHandle &info) override;
    void eval(SchedulerHandle &info) override;
  };
} // namespace met