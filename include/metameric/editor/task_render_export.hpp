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
#include <metameric/render/primitives_render.hpp>
#include <metameric/render/sensor.hpp>
#include <metameric/editor/detail/arcball.hpp>

namespace met {
  class RenderExportTask : public detail::TaskNode {
    Settings::RendererType m_render_type = Settings::RendererType::ePath;
    PathRenderPrimitive    m_render;
    Sensor                 m_sensor;
    fs::path               m_path;
    uint                   m_spp_trgt = 256u;
    uint                   m_spp_curr = 0u;
    uint                   m_view     = 0;
    bool                   m_in_prog  = false;
    detail::Arcball        m_arcball  = {{ .aspect = 1.f }};
    
  public:
    void init(SchedulerHandle &info) override;
    void eval(SchedulerHandle &info) override;
  };
} // namespace met