#pragma once

#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>

namespace ImGui {
  // ImGuizmo wrapper object to make handling gizmos slightly easier
  class Gizmo {
    using trf = Eigen::Affine3f;

    bool m_is_active = false;
    trf  m_init_trf;
    trf  m_delta_trf;

  public:
    // 
    enum class Operation : met::uint {
      eTranslate = 7u,
      eRotate    = 120u,
      eScale     = 896u,
      eAll       = eTranslate | eRotate | eScale
    };
    
    // Begin/eval/end functions, s.t. eval() returns a delta transform applied to the current
    // transform over every frame, and the user can detect changes
    bool begin_delta(const met::detail::Arcball &arcball, trf init_trf, Operation op = Operation::eTranslate);
    std::pair<bool, trf> 
         eval_delta();
    bool end_delta();

    // eval function, s.t. the current_trf variable is modified over every frame
    void eval(const met::detail::Arcball &arcball, trf &current_trf, Operation op = Operation::eAll);

    bool is_over() const;                                  // True if a active gizmo is moused over
    bool is_active() const { return m_is_active; }         // Whether guizmo input is handled, ergo if begin_delta() was called
    void set_active(bool active) { m_is_active = active; }
  };
}