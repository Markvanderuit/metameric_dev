#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/render/primitives_render.hpp>
#include <metameric/render/sensor.hpp>
#include <metameric/components/views/detail/arcball.hpp>

namespace met {
  class MeshViewportExportTask : public detail::TaskNode {
    PathRenderPrimitive m_render;
    Sensor              m_sensor;
    fs::path            m_path;
    uint                m_spp_trgt = 256u;
    uint                m_spp_curr = 0u;
    uint                m_view     = 0;
    bool                m_in_prog  = false;
    detail::Arcball     m_arcball  = {{ .aspect = 1.f }};
    
  public:
    void init(SchedulerHandle &info) override;
    void eval(SchedulerHandle &info) override;
  };
} // namespace met