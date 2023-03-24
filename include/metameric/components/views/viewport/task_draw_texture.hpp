#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class ViewportDrawTextureTask : public detail::TaskNode {
    gl::Array    m_array;
    gl::Buffer   m_data;
    gl::DrawInfo m_draw;
    gl::Program  m_program;

  public:
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };
} // namespace met
