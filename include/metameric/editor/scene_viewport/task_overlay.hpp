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

#include <metameric/core/scheduler.hpp>
#include <small_gl/array.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/program.hpp>

namespace met {
  class ViewportOverlayTask : public detail::TaskNode {
    using Depthbuffer = gl::Renderbuffer<gl::DepthComponent, 1>;

    std::string     m_program_key; 
    gl::Array       m_vao;
    gl::Framebuffer m_fbo;
    Depthbuffer     m_dbo;

    void eval_draw_constraints(SchedulerHandle &info);
    void eval_draw_path_queries(SchedulerHandle &info);
    void eval_draw_frustrum(SchedulerHandle &info);
    void eval_draw_info(SchedulerHandle &info);

  public:
    bool is_active(SchedulerHandle &info) override;
    void init(SchedulerHandle &info)      override;
    void eval(SchedulerHandle &info)      override;
  };
} // namespace met