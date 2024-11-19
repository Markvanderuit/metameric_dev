#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/scene/scene.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/render/sensor.hpp>
#include <metameric/render/primitives_query.hpp>
#include <metameric/editor/detail/imgui.hpp>
#include <metameric/editor/detail/gizmo.hpp>
#include <metameric/editor/detail/arcball.hpp>
#include <small_gl/texture.hpp>
#include <ImGuizmo.h>

namespace met {
  class ViewportEditorInputTask : public detail::TaskNode {
    ImGui::Gizmo       m_gizmo;
    SurfaceInfo        m_gizmo_curr_p;
    Uplifting::Vertex  m_gizmo_prev_v;

    RayQueryPrimitive  m_ray_prim;
    RaySensor          m_ray_sensor;
    RayRecord          m_ray_result;

    PathQueryPrimitive m_path_prim;
    PixelSensor        m_path_sensor;

  private:
    // Helper; shoot a ray and return hiti data
    RayRecord eval_ray_query(SchedulerHandle &info, const Ray &ray);

    // Helper; shoot n paths and return cached path data
    std::span<const PathRecord> eval_path_query(SchedulerHandle &info, uint spp);

    // Helper; shoot n paths, reduce to a power series, and build an indirect constraint from this
    void build_indirect_constraint(SchedulerHandle &info, const ConstraintRecord &is, NLinearConstraint &cstr);

  public:
    bool is_active(SchedulerHandle &info) override;
    void init(SchedulerHandle &info) override;
    void eval(SchedulerHandle &info) override;
  };
} // namespace met