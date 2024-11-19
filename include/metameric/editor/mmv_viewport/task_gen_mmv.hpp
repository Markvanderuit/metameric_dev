#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/buffer.hpp>

namespace met {
  class GenMMVTask : public detail::TaskNode  {
    gl::Buffer  m_chull_verts;
    gl::Buffer  m_chull_elems;
    gl::Buffer  m_chull_colors;

  public:
    void init(SchedulerHandle &info) override;
    void eval(SchedulerHandle &info) override;
  };
} // namespace met