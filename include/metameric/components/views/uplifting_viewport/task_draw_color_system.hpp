#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/render/sensor.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class DrawColorSystemTask : public detail::TaskNode {
    struct UnifLayout { float alpha; };

    uint          m_uplifting_i;
    Sensor        m_sensor;
    gl::Program   m_program;
    gl::Array     m_array;
    gl::Buffer    m_unif_settings;
    UnifLayout   *m_unif_settings_map;
    gl::Buffer    m_buff_verts;
    gl::Buffer    m_buff_elems;

  public:
    DrawColorSystemTask(uint uplifting_i)
    : m_uplifting_i(uplifting_i) { }

    bool is_active(SchedulerHandle &) override;
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };
} // namespace met