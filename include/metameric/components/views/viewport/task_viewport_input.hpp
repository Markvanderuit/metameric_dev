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
  namespace detail {
    using MeshReturnType = std::pair<
      std::vector<eig::Array3f>, 
      std::vector<eig::Array3u>
    >;

    MeshReturnType subdivide_elem(const std::vector<eig::Array3f> &verts,
                                  const std::vector<eig::Array3u> &elems,
                                  uint i) {
      // Generate openmesh representation to perform mesh operations
      auto mesh = generate_from_data<HalfedgeMeshTraits, Colr>(verts, elems);

      // Acquire handle to relevant subdividable face and vertices
      auto fh = mesh.face_handle(i);
      auto [fv0, fv1, fv2] = mesh.fv_range(fh).to_array<3>();

      // Insert new vertex at face's centroid
      auto vh = mesh.add_vertex(mesh.calc_face_centroid(fh));

      // Insert new faces connecting to this vertex and delete the old one
      mesh.delete_face(fh);
      mesh.add_face(fv0, fv1, vh);
      mesh.add_face(fv1, fv2, vh);
      mesh.add_face(fv2, fv0, vh);

      mesh.garbage_collection();
      return generate_data(mesh);
    }

    /* MeshReturnType collapse_elem(const std::vector<eig::Array3f> &verts,
                                 const std::vector<eig::Array3u> &elems,
                                 uint i) {
      // Generate openmesh representation to perform mesh operations
      auto mesh = generate_from_data<HalfedgeMeshTraits, Colr>(verts, elems);
                      
      // Acquire handle to relevant collapsable face and vertices
      auto fh = mesh.face_handle(i);
      auto [vh0, vh1, vh2] = mesh.fv_range(fh).to_array<3>();

      auto [eh0, eh1, eh2] = mesh.fe_range(fh).to_array<3>();

      mesh.garbage_collection();
      return generate_data(mesh);           
    } */

    MeshReturnType collapse_vert(const std::vector<eig::Array3f> &verts,
                                 const std::vector<eig::Array3u> &elems,
                                 uint i) {
      // Generate openmesh representation to perform mesh operations
      auto mesh = generate_from_data<HalfedgeMeshTraits, Colr>(verts, elems);

      // Acquire handle to relevant center vertex and surrounding faces/verts
      auto vh = mesh.vertex_handle(i);

      // Remove trio of faces around vertex and fill hole with a single face
      auto [vh0, vh1, vh2] = mesh.vv_range(vh).to_array<3>();
      mesh.delete_vertex(vh);
      mesh.add_face(vh1, vh0, vh2);

      mesh.garbage_collection();
      return generate_data(mesh);
    }
  } // namespace detail

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
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing;
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

        ImGui::Separator();
        ImGui::Value("Vertices", static_cast<uint>(e_verts.size()));
        ImGui::Value("Faces", static_cast<uint>(e_elems.size()));

        // Reset selections if edit mode was changed
        if (auto mode = detail::ViewportInputMode(m); mode != i_mode) {
          e_selection_vert.clear();
          e_selection_elem.clear();
          i_mode = mode;
        }

        // Given vertex edit mode and a potential selection, display options
        if (i_mode == detail::ViewportInputMode::eVertex && e_selection_vert.size() == 1) {
          ImGui::Separator();
          if (ImGui::Button("Collapse vertex")) {
            // Obtain mesh data with the collapsed vertex
            auto [verts, elems] = detail::collapse_vert(e_verts, e_elems, e_selection_vert[0]);
            
            // Apply data modification to project
            e_app_data.touch({
              .name = "Collapse vertex",
              .redo = [colr_i = verts,
                       elems  = elems,
                       i      = e_selection_vert[0]](auto &data) {
                data.gamut_elems  = elems;
                data.gamut_colr_i = colr_i;
                data.gamut_offs_j.erase(data.gamut_offs_j.begin() + i);
                data.gamut_mapp_i.erase(data.gamut_mapp_i.begin() + i);
                data.gamut_mapp_j.erase(data.gamut_mapp_j.begin() + i);
              },
              .undo = [elems  = e_elems,
                       colr_i = e_verts,
                       offs_j = e_proj_data.gamut_offs_j,
                       mapp_i = e_proj_data.gamut_mapp_i,
                       mapp_j = e_proj_data.gamut_mapp_j](auto &data) {
                data.gamut_elems  = elems;
                data.gamut_colr_i = colr_i;
                data.gamut_offs_j = offs_j;
                data.gamut_mapp_i = mapp_i;
                data.gamut_mapp_j = mapp_j;
              }
            });

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
            // Obtain mesh data with the subdivided face
            auto [verts, elems] = detail::subdivide_elem(e_verts, e_elems, e_selection_elem[0]);

            // Apply data modification to project
            e_app_data.touch({
              .name = "Subdivide face",
              .redo = [colr_i = verts, 
                       elems  = elems](auto &data) {
                data.gamut_elems  = elems;
                data.gamut_colr_i = colr_i;
                data.gamut_offs_j.resize(colr_i.size(), Colr(0));
                data.gamut_mapp_i.resize(colr_i.size(), 0);
                data.gamut_mapp_j.resize(colr_i.size(), 1);
              },
              .undo = [elems  = e_elems,
                       colr_i = e_verts,
                       offs_j = e_proj_data.gamut_offs_j,
                       mapp_i = e_proj_data.gamut_mapp_i,
                       mapp_j = e_proj_data.gamut_mapp_j](auto &data) {
                data.gamut_elems  = elems;
                data.gamut_colr_i = colr_i;
                data.gamut_offs_j = offs_j;
                data.gamut_mapp_i = mapp_i;
                data.gamut_mapp_j = mapp_j;
              },
            });

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