#pragma once

#include <metameric/core/data.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
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

  class ViewportInputTask : public detail::TaskNode {
  public:
    void init(SchedulerHandle &info) override {
      met_trace();
    
      // Add subtasks, share resources
      info.child_task("vert").init<ViewportInputVertTask>();
      info.resource("arcball").init<detail::Arcball>({ .dist = 10.f, .e_eye = 1.5f, .e_center = 0.5f });
    }

    void eval(SchedulerHandle &info) override {
      met_trace();

      // Get external resources
      const auto &e_vert_slct = info("viewport.input.vert", "selection").getr<std::vector<uint>>();
      const auto &e_cstr_slct = info("viewport.overlay", "constr_selection").getr<int>();
      const auto &e_window    = info.global("window").getr<gl::Window>();

      // Get modified resources
      auto &e_appl_data = info.global("appl_data").getw<ApplicationData>();
      auto &e_proj_data = e_appl_data.project_data;
      auto &io          = ImGui::GetIO();

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
        // Display mesh data, dependent on what data is available
        if (auto rsrc = info("gen_convex_weights", "delaunay"); rsrc.is_init()) {
          const auto &e_delaunay = rsrc.getr<AlDelaunay>();
          ImGui::Value("Vertices", static_cast<uint>(e_delaunay.verts.size()));
          ImGui::Value("Elements", static_cast<uint>(e_delaunay.elems.size()));
        } else {
          ImGui::Value("Vertices", static_cast<uint>(e_proj_data.verts.size()));
          ImGui::Value("Elements", static_cast<uint>(e_proj_data.elems.size()));
        }

        // Describe a button whichs adds a vertex
        if (ImGui::Button("Add vertex")) {
          // Apply data modification to project
          e_appl_data.touch({
            .name = "Add vertex",
            .redo = [](auto &data) { data.verts.push_back({ .colr_i = Colr(0.5), .csys_i = 0 }); },
            .undo = [](auto &data) { data.verts.pop_back(); }
          });

          // Select newly added vertex
          info.resource("viewport.input.vert", "selection").getw<std::vector<uint>>() = { static_cast<uint>(e_proj_data.verts.size() - 1) };
          info.resource("viewport.overlay", "constr_selection").getw<int>() = -1;
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
                data.verts.erase(data.verts.begin() + i);
            }, .undo = [edit = e_proj_data.verts](auto &data) { 
              data.verts = edit;
            }
          });

          // Clear selection after deleting vertex
          info.resource("viewport.input.vert", "selection").getw<std::vector<uint>>().clear();
          info.resource("viewport.overlay", "constr_selection").getw<int>() = -1;
        }
        if (e_vert_slct.empty()) ImGui::EndDisabled();
      }
      ImGui::End();

      // If window is not hovered, exit now instead of handling camera input
      guard(ImGui::IsItemHovered());

      // Get modified resources
      auto &i_arcball = info.resource("arcball").getw<detail::Arcball>();

      // Handle camera update: aspect ratio, scroll delta, move delta dependent on ImGui i/o
      i_arcball.set_aspect(viewport_size.x() / viewport_size.y());
      i_arcball.set_zoom_delta(-0.5f * io.MouseWheel);
      if (io.MouseDown[2] || (io.MouseDown[0] && io.KeyCtrl))
        i_arcball.set_ball_delta(eig::Array2f(io.MouseDelta) / viewport_size.array());
    }
  };
} // namespace met