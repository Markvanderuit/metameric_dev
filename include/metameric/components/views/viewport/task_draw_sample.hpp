#pragma once

#include <metameric/core/spectrum.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class ViewportDrawSampleTask : public detail::AbstractTask {
    // Local buffer to store individual sizes for selection/mouseover;
    // and positions for sample locations on-screen
    // each buffer is mapped for flushable changes
    gl::Buffer        m_samp_posi_buffer;
    gl::Buffer        m_samp_size_buffer;
    std::span<AlColr> m_samp_posi_map;
    std::span<float>  m_samp_size_map;

    // Graphics draw components
    gl::Buffer   m_inst_vert_buffer;
    gl::Buffer   m_inst_elem_buffer;
    gl::Array    m_samp_array;
    gl::Program  m_samp_program;
    gl::DrawInfo m_samp_draw;

  public:
    ViewportDrawSampleTask(const std::string &);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
    void dstr(detail::TaskDstrInfo &) override;
  };
} // namespace met