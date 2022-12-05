#pragma once

#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/enum.hpp>
#include <metameric/components/views/viewport/task_viewport_input_vert.hpp>
#include <metameric/components/views/viewport/task_viewport_input_edge.hpp>
#include <metameric/components/views/viewport/task_viewport_input_elem.hpp>
#include <ImGuizmo.h>
#include <algorithm>
#include <functional>
#include <numeric>
#include <ranges>

namespace met {

  class ViewportInputTask : public detail::AbstractTask {
  public:
    ViewportInputTask(const std::string &name)
    : detail::AbstractTask(name, true) { }

    void init(detail::TaskInitInfo &info) override {
      met_trace_full();
    
      // Add subtasks
      info.emplace_task_after<ViewportInputVertTask>(name(), name() + "_vert");
      info.emplace_task_after<ViewportInputEdgeTask>(name(), name() + "_edge");
      info.emplace_task_after<ViewportInputElemTask>(name(), name() + "_elem");

      // Share resources
      info.emplace_resource<detail::ViewportInputMode>("mode", detail::ViewportInputMode::eVertex);
      info.emplace_resource<detail::Arcball>("arcball", { .e_eye = 1.5f, .e_center = 0.5f });
    }

    void dstr(detail::TaskDstrInfo &info) override {
      met_trace_full();

      // Remove subtasks
      info.remove_task(name() + "_vert");
      info.remove_task(name() + "_edge");
      info.remove_task(name() + "_elem");
    }

    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();
                      
      // Get shared resources
      auto &io               = ImGui::GetIO();
      auto &i_arcball        = info.get_resource<detail::Arcball>("arcball");
      auto &i_mode           = info.get_resource<detail::ViewportInputMode>("mode");
      auto &e_selection_vert = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection");
      auto &e_selection_elem = info.get_resource<std::vector<uint>>("viewport_input_elem", "selection");
      auto &e_app_data       = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_proj_data      = e_app_data.project_data;
      auto &e_verts          = e_app_data.project_data.gamut_colr_i;
      auto &e_elems          = e_app_data.project_data.gamut_elems;

      // Compute viewport offs, size minus ImGui's tab bars etc
      eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                 + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());     

      // Handle edit mode selection window
      constexpr auto window_flags = 
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar | 
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove  | ImGuiWindowFlags_NoFocusOnAppearing;
      eig::Array2f edit_size = { 300.f, 0.f };
      eig::Array2f edit_posi = { viewport_offs.x() + viewport_size.x() - edit_size.x() - 16.f, viewport_offs.y() + 16.f };
      ImGui::SetNextWindowPos(edit_posi);
      ImGui::SetNextWindowSize(edit_size);
      if (ImGui::Begin("Edit mode", nullptr, window_flags)) {
        // Handle edit mode flags
        int m = static_cast<int>(i_mode);
        ImGui::RadioButton("Vertex", &m, static_cast<int>(detail::ViewportInputMode::eVertex));
        ImGui::SameLine();
        ImGui::RadioButton("Edge",   &m, static_cast<int>(detail::ViewportInputMode::eEdge));
        ImGui::SameLine();
        ImGui::RadioButton("Face",   &m, static_cast<int>(detail::ViewportInputMode::eFace));
        if (auto mode = detail::ViewportInputMode(m); mode != i_mode) {
          // Reset selections
          info.get_resource<std::vector<uint>>("viewport_input_vert", "selection").clear();
          info.get_resource<std::vector<uint>>("viewport_input_elem", "selection").clear();
          i_mode = mode;
        }

        constexpr
        auto i_get = [](auto &v) { return [&v](const auto &i) -> auto& { return v[i]; }; };

        // Given vertex edit mode and a potential selection, display options
        if (i_mode == detail::ViewportInputMode::eVertex && e_selection_vert.size() == 1) {
          ImGui::Separator();
          if (ImGui::Button("Collapse vertex")) {
            // Generate openmesh representation to perform mesh operations
            auto mesh = generate_from_data<HalfedgeMeshTraits, Colr>(e_verts, e_elems);

            // Acquire handle to relevant center vertex and surrounding faces/verts
            auto vh = mesh.vertex_handle(e_selection_vert[0]);

            // Remove trio of faces around vertex and fill hole with a single face
            auto [vh0, vh1, vh2] = mesh.vv_range(vh).to_array<3>();
            mesh.delete_vertex(vh);
            mesh.add_face(vh1, vh0, vh2);

            // Shift project data to new positions, given deleted vertex
            std::vector<uint> vert_indices(mesh.n_vertices() - 1);
            std::ranges::transform(mesh.vertices(), vert_indices.begin(),
              [&](const auto &vh) { return vh.idx(); });
  
            // Return updated data from mesh
            mesh.garbage_collection();
            auto [verts, elems] = generate_data<HalfedgeMeshTraits, Colr>(mesh);

            // Shift project data to front vertices
            for (uint i = 0; i < vert_indices.size(); ++i) {
              uint j = vert_indices[i];
              e_proj_data.gamut_offs_j[i] = e_proj_data.gamut_offs_j[j];
              e_proj_data.gamut_mapp_i[i] = e_proj_data.gamut_mapp_i[j];
              e_proj_data.gamut_mapp_j[i] = e_proj_data.gamut_mapp_j[j];
            }

            // Remove last vertex from project data
            e_proj_data.gamut_elems = elems;
            e_proj_data.gamut_colr_i = verts;
            e_proj_data.gamut_offs_j.resize(verts.size());
            e_proj_data.gamut_mapp_i.resize(verts.size());
            e_proj_data.gamut_mapp_j.resize(verts.size());

            // Clear selection to prevent issues down the line with non-existent 
            // data still being selected
            e_selection_vert.clear();
            e_selection_elem.clear();
          }
        }
              
        // Given face edit mode and a potential selection, display options
        if (i_mode == detail::ViewportInputMode::eFace && !e_selection_elem.empty()) {
          ImGui::Separator();
          if (ImGui::Button("Subdivide face")) {
            // Generate openmesh representation to perform mesh operations
            auto mesh = generate_from_data<BaselineMeshTraits, Colr>(e_verts, e_elems);
            mesh.request_face_status();

            // Acquire handle to relevant subdividable face and vertices
            auto fh = mesh.face_handle(e_selection_elem[0]);
            auto [fv0, fv1, fv2] = mesh.fv_range(fh).to_array<3>();

            // Insert new vertex at face's centroid
            auto vh = mesh.add_vertex(mesh.calc_face_centroid(fh));

            // Insert new faces connecting to this vertex and delete the old one
            mesh.delete_face(fh);
            mesh.add_face(fv0, fv1, vh);
            mesh.add_face(fv1, fv2, vh);
            mesh.add_face(fv2, fv0, vh);

            // Return updated data from mesh
            mesh.garbage_collection();
            auto [verts, elems] = generate_data<BaselineMeshTraits, Colr>(mesh);

            // Apply data modification to project
            e_proj_data.gamut_elems  = elems;
            e_proj_data.gamut_colr_i = verts;
            e_proj_data.gamut_offs_j.resize(verts.size(), Colr(0));
            e_proj_data.gamut_mapp_i.resize(verts.size(), 0);
            e_proj_data.gamut_mapp_j.resize(verts.size(), 1);
            
            // Clear selection to prevent issues down the line with non-existent data being selected
            e_selection_vert.clear(); 
            e_selection_elem.clear();
          }
          if (ImGui::Button("Collapse face")) {

          }
        }
      }
      ImGui::End();

      // If window is not hovered, exit now instead of handling camera input
      guard(ImGui::IsItemHovered());

      // Handle camera update: aspect ratio, scroll delta, move delta dependent on ImGui i/o
      i_arcball.m_aspect = viewport_size.x() / viewport_size.y();
      i_arcball.set_dist_delta(-0.5f * io.MouseWheel);
      if (io.MouseDown[2] || (io.MouseDown[0] && io.KeyCtrl))
        i_arcball.set_pos_delta(eig::Array2f(io.MouseDelta) / viewport_size.array());
      i_arcball.update_matrices();
    }
  };
} // namespace met