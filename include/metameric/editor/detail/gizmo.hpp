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

#include <metameric/editor/detail/imgui.hpp>
#include <metameric/editor/detail/arcball.hpp>

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