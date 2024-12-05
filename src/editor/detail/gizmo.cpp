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

#include <metameric/editor/detail/gizmo.hpp>
#include <ImGuizmo.h>

namespace ImGui {
  bool Gizmo::begin_delta(const met::detail::Arcball &arcball, trf init_trf, Operation op) {
    met_trace();

    using namespace met;
    
    // Reset internal state
    if (!m_is_active) {
      m_init_trf  = init_trf;
      m_delta_trf = trf::Identity();
    }

    // Compute viewport offset and size, minus ImGui's tab bars etc
    eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                               + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
    eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                               - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
    
    // Specify ImGuizmo settings for current viewport and insert the gizmo
    ImGuizmo::SetRect(viewport_offs[0], viewport_offs[1], viewport_size[0], viewport_size[1]);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::Manipulate(arcball.view().data(), arcball.proj().data(), 
      static_cast<ImGuizmo::OPERATION>(op), ImGuizmo::MODE::LOCAL, 
      m_init_trf.data(), m_delta_trf.data());

    guard(!m_is_active && ImGuizmo::IsUsing(), false);
    m_is_active = true;
    return true;
  }

  std::pair<bool, Gizmo::trf> Gizmo::eval_delta() {
    met_trace();
    guard(m_is_active && ImGuizmo::IsUsing(), {false, trf::Identity()});
    return { true, m_delta_trf };
  }

  bool Gizmo::end_delta() {
    met_trace();
    guard(m_is_active && !ImGuizmo::IsUsing(), false);
    m_is_active = false;
    return true;
  }

  void Gizmo::eval(const met::detail::Arcball &arcball, trf &current_trf, Operation op) {
    met_trace();

    using namespace met;
    
    // Reset internal state
    m_delta_trf = trf::Identity();

    // Compute viewport offset and size, minus ImGui's tab bars etc
    eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                               + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
    eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                               - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
    
    // Specify ImGuizmo settings for current viewport and insert the gizmo
    ImGuizmo::SetRect(viewport_offs[0], viewport_offs[1], viewport_size[0], viewport_size[1]);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::Manipulate(arcball.view().data(), arcball.proj().data(), 
      static_cast<ImGuizmo::OPERATION>(op), ImGuizmo::MODE::LOCAL, current_trf.data(), m_delta_trf.data());

    // Setup phase
    if (!m_is_active && ImGuizmo::IsUsing()) {
      m_is_active = true;
    }

    // Move phase
    if (ImGuizmo::IsUsing()) {

    }

    // Teardown phase
    if (m_is_active && !ImGuizmo::IsUsing()) {
      m_is_active = false;

    }
  }

  bool Gizmo::is_over() const {
    return ImGuizmo::IsOver();
  }
} // namespace ImGui