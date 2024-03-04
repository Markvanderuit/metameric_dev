#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/render/sensor.hpp>
#include <metameric/render/primitives_query.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/texture.hpp>
#include <ImGuizmo.h>

namespace met {
  static constexpr float selector_near_distance = 12.f;

  // Helper class to define active uplifting/constraint selection
  class InputSelection {
    constexpr static uint invalid_data = 0xFFFFFFFF;

  public:
    uint uplifting_i  = invalid_data;
    uint constraint_i = 0;
  
  public:
    bool is_valid() const { return uplifting_i != invalid_data; }

    static InputSelection invalid() { return InputSelection(); }
    
    friend auto operator<=>(const InputSelection &, const InputSelection &) = default;
  };

  // TODO remove
  /* namespace detail {
    template <typename T> struct engaged_t {
      template <typename... Ts>
      constexpr bool operator()(const std::variant<Ts...> &variant) const {
        return std::holds_alternative<T>(variant);
      }
      template <typename... Ts>
      constexpr bool operator()(std::variant<Ts...> variant) const {
        return std::holds_alternative<T>(variant);
      }
    };
    template <typename T> inline constexpr auto engaged = engaged_t<T>{};
    
    template <typename T> struct variant_get_t {
      template <typename... Ts>
      constexpr decltype(auto) operator()(const std::variant<Ts...> &variant) const {
        return std::get<T>(variant);
      }
      template <typename... Ts>
      constexpr decltype(auto) operator()(std::variant<Ts...> variant) const {
        return std::get<T>(variant);
      }
    };
    template <typename T> inline constexpr auto variant_get = variant_get_t<T>{};

    template <typename T>
    inline constexpr auto variant_filter_view = [](rng::viewable_range auto &&r) {
      return r | vws::filter(engaged<T>)
               | vws::transform(variant_get<T>);
    };
  } // namespace detail */

  class MeshViewportEditorInputTask : public detail::TaskNode {
    RayQueryPrimitive  m_ray_prim;
    RaySensor          m_ray_sensor;
    RayRecord          m_ray_result;
    PathQueryPrimitive m_path_prim;
    PixelSensor        m_path_sensor;
    bool               m_is_gizmo_used;
    SurfaceInfo        m_gizmo_prev_si;

    RayRecord eval_ray_query(SchedulerHandle &info, const Ray &ray);
    void eval_indirect_data(SchedulerHandle &info, const InputSelection &is, IndirectSurfaceConstraint &cstr);

  public:
    bool is_active(SchedulerHandle &info) override;
    void init(SchedulerHandle &info) override;
    void eval(SchedulerHandle &info) override;
  };
} // namespace met