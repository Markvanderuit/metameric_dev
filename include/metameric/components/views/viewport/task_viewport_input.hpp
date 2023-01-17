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
#include <metameric/components/views/viewport/task_viewport_input_edge.hpp>
#include <metameric/components/views/viewport/task_viewport_input_elem.hpp>
#include <metameric/components/views/viewport/task_viewport_input_sample.hpp>
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
      // info.emplace_task_after<ViewportInputEdgeTask>(name(), name() + "_edge");
      info.emplace_task_after<ViewportInputElemTask>(name(), name() + "_elem");
      info.emplace_task_after<ViewportInputSampleTask>(name(), name() + "_samp");

      // Share resources
      info.emplace_resource<detail::ViewportInputMode>("mode", detail::ViewportInputMode::eVertex);
      info.emplace_resource<detail::Arcball>("arcball", { .dist = 2.f, .e_eye = 1.5f, .e_center = 0.5f });
    }

    void dstr(detail::TaskDstrInfo &info) override {
      met_trace_full();

      // Remove subtasks
      info.remove_task(name() + "_vert");
      // info.remove_task(name() + "_edge");
      info.remove_task(name() + "_elem");
      info.remove_task(name() + "_samp");
    }

    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();
                      
      // Get shared resources
      auto &io               = ImGui::GetIO();
      auto &e_window         = info.get_resource<gl::Window>(global_key, "window");
      auto &i_arcball        = info.get_resource<detail::Arcball>("arcball");
      auto &i_mode           = info.get_resource<detail::ViewportInputMode>("mode");
      auto &e_selection_vert = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection");
      auto &e_selection_elem = info.get_resource<std::vector<uint>>("viewport_input_elem", "selection");
      auto &e_selection_samp = info.get_resource<std::vector<uint>>("viewport_input_samp", "selection");
      auto &e_appl_data      = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_proj_data      = e_appl_data.project_data;
      auto &e_verts          = e_appl_data.project_data.gamut_verts;
      auto &e_elems          = e_appl_data.project_data.gamut_elems;
      auto &e_samples        = e_appl_data.project_data.sample_verts;

      // Compute viewport offs, size minus ImGui's tab bars etc
      eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                 + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());     

      // Handle edit mode selection window
      constexpr auto window_flags = 
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing;

      float actual_padding = overlay_padding * e_window.content_scale();
      eig::Array2f overlay_size = { overlay_width * e_window.content_scale() , 0.f };
      eig::Array2f overlay_offs = { viewport_offs.x() - actual_padding + viewport_size.x() - overlay_size.x(), 
                                    viewport_offs.y() + actual_padding };

      ImGui::SetNextWindowPos(overlay_offs);
      ImGui::SetNextWindowSize(overlay_size);
      
      if (ImGui::Begin("Edit mode", nullptr, window_flags)) {
        // Handle edit mode flags
        int m = static_cast<int>(i_mode);
        ImGui::RadioButton("Vertex", &m, static_cast<int>(detail::ViewportInputMode::eVertex));
        ImGui::SameLine();
        ImGui::RadioButton("Face",   &m, static_cast<int>(detail::ViewportInputMode::eFace));
        ImGui::SameLine();
        ImGui::RadioButton("Sample",   &m, static_cast<int>(detail::ViewportInputMode::eSample));

        ImGui::Separator();
        ImGui::Value("Vertices", static_cast<uint>(e_verts.size()));
        ImGui::Value("Faces", static_cast<uint>(e_elems.size()));
        ImGui::Value("Samples", static_cast<uint>(e_samples.size()));

        // Reset selections if edit mode was changed
        if (auto mode = detail::ViewportInputMode(m); mode != i_mode) {
          e_selection_vert.clear();
          e_selection_elem.clear();
          e_selection_samp.clear();
          i_mode = mode;
        }

        // Given vertex edit mode and a potential selection, display options
        if (i_mode == detail::ViewportInputMode::eVertex && e_selection_vert.size() == 1) {
          ImGui::Separator();
          if (ImGui::Button("Collapse vertex")) {
            // Obtain mesh data with the collapsed vertex
            std::vector<Colr> colrs_i;
            std::ranges::transform(e_verts, std::back_inserter(colrs_i), [](const auto &v) { return v.colr_i; });
            auto [_, elems] = detail::collapse_vert(colrs_i, e_elems, e_selection_vert[0]);
            
            // Apply data modification to project
            e_appl_data.touch({
              .name = "Collapse vertex",
              .redo = [elems = elems,
                       i     = e_selection_vert[0]](auto &data) {
                data.gamut_elems  = elems;
                data.gamut_verts.erase(data.gamut_verts.begin() + i);
              },
              .undo = [elems  = e_elems,
                       verts  = e_verts](auto &data) {
                data.gamut_elems  = elems;
                data.gamut_verts  = verts;
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
            std::vector<Colr> colrs_i;
            std::ranges::transform(e_verts, std::back_inserter(colrs_i), [](const auto &v) { return v.colr_i; });
            auto [verts, elems] = detail::subdivide_elem(colrs_i, e_elems, e_selection_elem[0]);

            // Apply data modification to project
            e_appl_data.touch({
              .name = "Subdivide face",
              .redo = [verts = verts, 
                       elems = elems](auto &data) {
                data.gamut_elems  = elems;
                data.gamut_verts.resize(verts.size(), {
                  .colr_i = verts[verts.size() - 1],
                  .csys_i = 0,
                  .colr_j = { },
                  .csys_j = { }
                });
              },
              .undo = [elems  = e_elems,
                       verts = e_verts](auto &data) {
                data.gamut_elems  = elems;
                data.gamut_verts  = verts;
              },
            });

            // Clear selection to prevent issues down the line with non-existent data being selected
            e_selection_vert.clear(); 
            e_selection_elem.clear();
          }
        }

        if (i_mode == detail::ViewportInputMode::eSample) {
          // Add sample button
          ImGui::Separator();
          if (ImGui::Button("Add sample")) {
            ProjectData::Vert sample = { .colr_i = 0.5, .csys_i = 0 };
            e_appl_data.touch({
              .name = "Add sample",
              .redo = [sample = sample](auto &data) { data.sample_verts.push_back(sample); },
              .undo = [sample = sample](auto &data) { data.sample_verts.pop_back(); }
            });
          }

          // Remove sample button, disabled on empty samples
          ImGui::SameLine();
          if (e_selection_samp.empty()) ImGui::BeginDisabled();
          if (ImGui::Button("Remove sample(s)")) {
            debug::check_expr_rel(false, "Unimplemented!");
          }
          if (e_selection_samp.empty()) ImGui::EndDisabled();

          // Solver button
          ImGui::Separator();
          if (ImGui::Button("Fit convex hull")) {
            e_appl_data.refit_convex_hull();
          }
          if (!e_samples.empty()) {
            ImGui::SameLine();
            if (ImGui::Button("Fit samples")) {
              e_appl_data.solve_samples();
            }
          }
        }

        ImGui::Separator();
        if (ImGui::Button("Print hull to console")) {
          std::vector<Colr> verts(e_verts.size());
          std::ranges::transform(e_verts, verts.begin(), [](const auto &v) { return v.colr_i; });
          fmt::print("verts = np.array({})\nelems = np.array({})\n", verts, e_elems);
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