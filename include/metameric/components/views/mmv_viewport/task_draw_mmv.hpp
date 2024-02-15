#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/render/sensor.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  struct DrawMMVTask : public detail::TaskNode {
    
    struct UnifLayout {
      alignas(4) float alpha;
    };

    Sensor       m_sensor;
    gl::Buffer   m_unif_buffer;
    UnifLayout  *m_unif_buffer_map;
    gl::Program  m_program;
    gl::DrawInfo m_dispatch;

  public:
    void init(SchedulerHandle &info) override;
    void eval(SchedulerHandle &info) override;
    bool is_active(SchedulerHandle &info) override;
  };
} // namespace met