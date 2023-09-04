#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/spectrum.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class MeshViewportDrawTask : public detail::TaskNode {
    struct UnifLayout {
      alignas(64) eig::Matrix4f camera_matrix;
      alignas(64) eig::Matrix4f model_matrix;
      alignas(4)  bool          use_diffuse_texture;
      alignas(16) AlColr        diffuse_value; 
    };

    UnifLayout  *m_unif_buffer_map;
    gl::Buffer   m_unif_buffer;
    gl::Program  m_program;
    gl::DrawInfo m_draw;
    
  public:
    bool is_active(SchedulerHandle &info) override;
    void init(SchedulerHandle &info)      override;
    void eval(SchedulerHandle &info)      override;
  };
} // namespace met