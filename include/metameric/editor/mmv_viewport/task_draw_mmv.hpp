// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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