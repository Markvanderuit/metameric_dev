#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/utility.hpp>

namespace met {
  class ViewportDrawMVCTask : public detail::AbstractTask {
    struct BarycentricBuffer {
      eig::AlArray3f sub;
      eig::Array44f  inv;
    };

    gl::Buffer      m_baryc_posi_buffer;
    gl::Buffer      m_baryc_colr_buffer;
    gl::Program     m_baryc_program;
    gl::ComputeInfo m_baryc_dispatch;
    gl::Buffer      m_baryc_uniform_buffer;
    gl::Buffer      m_baryc_bary_buffer;
    gl::Buffer      m_points_vert_buffer;
    gl::Buffer      m_points_elem_buffer;
    gl::Array       m_points_array;
    gl::DrawInfo    m_points_dispatch;
    gl::Program     m_points_program;

    BarycentricBuffer *m_baryc_bary_map;
  public:
    ViewportDrawMVCTask(const std::string &name);
    
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met