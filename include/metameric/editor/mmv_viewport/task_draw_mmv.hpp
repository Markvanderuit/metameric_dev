#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/render/sensor.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class DrawMMVTask : public detail::TaskNode {
    struct UnifLayout {  float alpha; };

    std::string  m_program_key;
    Sensor       m_sensor;
    gl::Buffer   m_unif_buffer;
    UnifLayout  *m_unif_buffer_map;
    gl::DrawInfo m_dispatch;

    void eval_draw_constraint(SchedulerHandle &info);
    void eval_draw_volume(SchedulerHandle &info);

  public:
    bool is_active(SchedulerHandle &info) override;
    void init(SchedulerHandle &info) override;
    void eval(SchedulerHandle &info) override;
  };
} // namespace met