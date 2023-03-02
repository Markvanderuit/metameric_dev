#pragma once

#include <metameric/core/spectrum.hpp>
#include <metameric/core/data.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/enum.hpp>
#include <metameric/components/views/viewport/task_viewport_input_vert.hpp>
#include <small_gl/window.hpp>
#include <ImGuizmo.h>
#include <algorithm>
#include <functional>
#include <numeric>
#include <ranges>

namespace met {
  // Size constants, independent of window scale
  const float overlay_padding = 8.f;
  const float overlay_width   = 192.f;

  // Flags for the input overlay window
  constexpr auto window_flags = 
    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking |
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing;

  class ViewportInputTask : public detail::AbstractTask {
  public:
    ViewportInputTask(const std::string &name)
    : detail::AbstractTask(name, true) { }

    void init(detail::TaskInitInfo &info) override {
      met_trace_full();
    
      // Add subtasks, share resources
      info.emplace_task_after<ViewportInputVertTask>(name(), name() + "_vert");
      info.emplace_resource<detail::Arcball>("arcball", { .dist = 10.f, .e_eye = 1.5f, .e_center = 0.5f });
    }

    void dstr(detail::TaskDstrInfo &info) override {
      met_trace_full();
      info.remove_task(name() + "_vert");
    }

    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();
                      
      // Get shared resources
      auto &io          = ImGui::GetIO();
      auto &e_window    = info.get_resource<gl::Window>(global_key, "window");
      auto &i_arcball   = info.get_resource<detail::Arcball>("arcball");
      auto &e_vert_slct = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection");
      auto &e_cstr_slct = info.get_resource<int>("viewport_overlay", "constr_selection");
      auto &e_appl_data = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_proj_data = e_appl_data.project_data;

      // Compute viewport offs, size minus ImGui's tab bars etc
      eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                 + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());     

      // Compute overlay window dimensions, taking into account padding in the top-right corner
      float actual_padding = overlay_padding * e_window.content_scale();
      eig::Array2f overlay_size = { overlay_width * e_window.content_scale() , 0.f };
      eig::Array2f overlay_offs = { viewport_offs.x() - actual_padding + viewport_size.x() - overlay_size.x(), 
                                    viewport_offs.y() + actual_padding };

      ImGui::SetNextWindowPos(overlay_offs);
      ImGui::SetNextWindowSize(overlay_size);

      if (ImGui::Begin("Vertex editing", nullptr, window_flags)) {
        ImGui::Value("Vertices", static_cast<uint>(e_proj_data.vertices.size()));

        // Describe a button whichs adds a vertex
        if (ImGui::Button("Add vertex")) {
          // Apply data modification to project
          e_appl_data.touch({
            .name = "Add vertex",
            .redo = [](auto &data) { data.vertices.push_back({ .colr_i = Colr(0.5), .csys_i = 0 }); },
            .undo = [](auto &data) { data.vertices.pop_back(); }
          });

          // Select newly added vertex
          e_vert_slct = { static_cast<uint>(e_proj_data.vertices.size() - 1) };
          e_cstr_slct = -1;
        }

        ImGui::SameLine();

        // Describe a button which removes one or more vertices, visible only if vertices are selected
        if (e_vert_slct.empty()) ImGui::BeginDisabled();
        if (ImGui::Button("Remove vertex")) {
          // Collect back-to-front indices of deleted vertices, s.t. they can be removed from std::vector without affecting order
          std::vector<uint> indices = e_vert_slct;
          std::ranges::sort(indices, std::ranges::greater());

          // Apply data modification to project
          e_appl_data.touch({
            .name = "Remove vertex",
            .redo = [edit = indices](auto &data) { 
              for (uint i : edit)
                data.vertices.erase(data.vertices.begin() + i);
            }, .undo = [edit = e_proj_data.vertices](auto &data) { 
              data.vertices = edit;
            }
          });

          // Clear selection after deleting vertex
          e_vert_slct.clear();
          e_cstr_slct = -1;
        }
        if (e_vert_slct.empty()) ImGui::EndDisabled();
      }
      ImGui::End();

     /*  if (ImGui::Begin("Edit mode", nullptr, window_flags)) {
        // Handle edit mode flags
        int m = static_cast<int>(i_mode);
        ImGui::RadioButton("Vertex", &m, static_cast<int>(detail::ViewportInputMode::eVertex));
        ImGui::SameLine();
        ImGui::RadioButton("Face",   &m, static_cast<int>(detail::ViewportInputMode::eFace));

        ImGui::Separator();

        ImGui::Value("Vertices", static_cast<uint>(e_verts.size()));
        ImGui::Value("Faces", static_cast<uint>(e_elems.size()));

        // Reset selections if edit mode was changed
        if (auto mode = detail::ViewportInputMode(m); mode != i_mode) {
          e_vert_slct.clear();
          e_selection_elem.clear();
          i_mode = mode;
        }

        // Given vertex edit mode and a potential selection, display options
        if (i_mode == detail::ViewportInputMode::eVertex && e_vert_slct.size() == 1) {
          ImGui::Separator();
          if (ImGui::Button("Collapse vertex")) {
            // Obtain mesh data with the collapsed vertex
            std::vector<Colr> colrs_i;
            std::ranges::transform(e_verts, std::back_inserter(colrs_i), [](const auto &v) { return v.colr_i; });
            auto [_, elems] = detail::collapse_vert(colrs_i, e_elems, e_vert_slct[0]);
            
            // Apply data modification to project
            e_appl_data.touch({
              .name = "Collapse vertex",
              .redo = [elems = elems,
                       i     = e_vert_slct[0]](auto &data) {
                data.gamut_elems  = elems;
                data.vertices.erase(data.vertices.begin() + i);
              },
              .undo = [elems  = e_elems,
                       verts  = e_verts](auto &data) {
                data.gamut_elems  = elems;
                data.vertices  = verts;
              }
            });

            // Clear selection to prevent issues down the line with non-existent 
            // data still being selected
            e_vert_slct.clear();
            e_selection_elem.clear();
          }
        }
              
        // Given face edit mode and a potential selection, display options
        if (i_mode == detail::ViewportInputMode::eFace && !e_selection_elem.empty()) {
          ImGui::Separator();
          if (ImGui::Button("Subdivide face")) {
            // Obtain mesh data with the subdivided face
            std::vector<Colr> colrs_i;
            std::ranges::transform(e_verts, std::back_inserter(colrs_i), [](const auto &v) { return v.colr_i; });
            auto [verts, elems] = detail::subdivide_elem(colrs_i, e_elems, e_selection_elem[0]);

            // Apply data modification to project
            e_appl_data.touch({
              .name = "Subdivide face",
              .redo = [verts = verts, 
                       elems = elems](auto &data) {
                data.gamut_elems  = elems;
                data.vertices.resize(verts.size(), {
                  .colr_i = verts[verts.size() - 1],
                  .csys_i = 0,
                  .colr_j = { },
                  .csys_j = { }
                });
              },
              .undo = [elems  = e_elems,
                       verts = e_verts](auto &data) {
                data.gamut_elems  = elems;
                data.vertices  = verts;
              },
            });

            // Clear selection to prevent issues down the line with non-existent data being selected
            e_vert_slct.clear(); 
            e_selection_elem.clear();
          }
        }

        ImGui::Separator();
        if (ImGui::Button("Print hull to console")) {
          std::vector<Colr> verts(e_verts.size());
          std::ranges::transform(e_verts, verts.begin(), [](const auto &v) { return v.colr_i; });
          fmt::print("chull_verts = np.array({})\nchull_elems = np.array({})\n", verts, e_elems);
        }
      }
      ImGui::End(); */

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