#include <metameric/components/views/task_gamut_editor.hpp>
#include <metameric/components/views/gamut_viewport/task_draw_begin.hpp>
#include <metameric/components/views/gamut_viewport/task_draw_metamer_ocs.hpp>
#include <metameric/components/views/gamut_viewport/task_draw_end.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <ImGuizmo.h>

namespace met {
  GamutEditorTask::GamutEditorTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void GamutEditorTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Share resources
    info.insert_resource<gl::Texture2d3f>("draw_texture", gl::Texture2d3f());
    info.emplace_resource<detail::Arcball>("arcball", { .e_eye = 1.0f, .e_center = 0.0f });

    // Add subtasks in reverse order
    info.emplace_task_after<DrawEndTask>(name(), name() + "_draw_end", name());
    info.emplace_task_after<DrawMetamerOCSTask>(name(), name() + "_draw_metamer_ocs", name());
    info.emplace_task_after<DrawBeginTask>(name(), name() + "_draw_begin", name());

    // Start with gizmo inactive
    m_is_gizmo_used = false;
  }

  void GamutEditorTask::dstr(detail::TaskDstrInfo &info) {
    // Remove subtasks
    info.remove_task(name() + "_draw_begin");
    info.remove_task(name() + "_draw_metamer_ocs");
    info.remove_task(name() + "_draw_end");
  }

  void GamutEditorTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();
    
    // Get shared resources
    auto &i_draw_texture = info.get_resource<gl::Texture2d3f>("draw_texture");
    auto &e_gamut_idx    = info.get_resource<int>("viewport", "gamut_selection");
    
    if (ImGui::Begin("Gamut editor")) {
      // Compute viewport size minus ImGui's tab bars etc
      // (Re-)create viewport texture if necessary; attached framebuffers are resized separately
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f texture_size  = viewport_size.x();
      eig::Array2f plot_size     = viewport_size * eig::Array2f(.67f, .1f);

      if (!i_draw_texture.is_init() || (i_draw_texture.size() != viewport_size.cast<uint>()).all()) {
        i_draw_texture = {{ .size = texture_size.cast<uint>() }};
      }

      if (e_gamut_idx >= 0) {
        // Get shared resources
        auto &e_app_data     = info.get_resource<ApplicationData>(global_key, "app_data");
        auto &e_gamut_colr   = e_app_data.project_data.gamut_colr_i;
        auto &e_gamut_offs   = e_app_data.project_data.gamut_offs_j;
        auto &e_gamut_spec   = e_app_data.project_data.gamut_spec;
        auto &e_gamut_mapp_i = e_app_data.project_data.gamut_mapp_i;
        auto &e_gamut_mapp_j = e_app_data.project_data.gamut_mapp_j;
        auto &e_mappings     = e_app_data.loaded_mappings;
        auto &e_mapping_data = e_app_data.project_data.mappings;

        // Obtain selected reflectance and colors
        Spec &gamut_spec   = e_gamut_spec[e_gamut_idx];
        Colr &gamut_colr_i = e_gamut_colr[e_gamut_idx];
        Colr &gamut_offs_j = e_gamut_offs[e_gamut_idx];

        // Compute resulting color and error
        Colr gamut_actual = e_mappings[e_gamut_mapp_i[e_gamut_idx]].apply_color(gamut_spec);
        Colr gamut_error  = (gamut_actual - gamut_colr_i).abs();

        ImGui::PlotLines("Reflectance", gamut_spec.data(), gamut_spec.rows(), 0, nullptr, 0.f, 1.f, plot_size);

        ImGui::Separator();

        // Local copies of gamut mapping indices
        uint l_gamut_mapp_i = e_gamut_mapp_i[e_gamut_idx];
        uint l_gamut_mapp_j = e_gamut_mapp_j[e_gamut_idx];
        
        // Names of selected mappings
        const auto &mapping_name_i = e_mapping_data[l_gamut_mapp_i].first;
        const auto &mapping_name_j = e_mapping_data[l_gamut_mapp_j].first;

        // Selector for first mapping index
        if (ImGui::BeginCombo("Mapping 0", mapping_name_i.c_str())) {
          for (uint i = 0; i < e_mapping_data.size(); ++i) {
            auto &[key, _] = e_mapping_data[i];
            if (ImGui::Selectable(key.c_str(), i == l_gamut_mapp_i)) {
              l_gamut_mapp_i = i;
            }
          }
          ImGui::EndCombo();
        }

        // Selector for second mapping index
        if (ImGui::BeginCombo("Mapping 1", mapping_name_j.c_str())) {
          for (uint i = 0; i < e_mapping_data.size(); ++i) {
            auto &[key, _] = e_mapping_data[i];
            if (ImGui::Selectable(key.c_str(), i == l_gamut_mapp_j)) {
              l_gamut_mapp_j = i;
            }
          }
          ImGui::EndCombo();
        }

        // If changes to local copies were made, register a data edit
        if (l_gamut_mapp_i != e_gamut_mapp_i[e_gamut_idx]) {
          e_app_data.touch({
            .name = "Change gamut mapping 0",
            .redo = [edit = l_gamut_mapp_i,              i = e_gamut_idx](auto &data) { data.gamut_mapp_i[i] = edit; },
            .undo = [edit = e_gamut_mapp_i[e_gamut_idx], i = e_gamut_idx](auto &data) { data.gamut_mapp_i[i] = edit; }
          });
        }
        if (l_gamut_mapp_j != e_gamut_mapp_j[e_gamut_idx]) {
          e_app_data.touch({
            .name = "Change gamut mapping 1",
            .redo = [edit = l_gamut_mapp_j,              i = e_gamut_idx](auto &data) { data.gamut_mapp_j[i] = edit; },
            .undo = [edit = e_gamut_mapp_j[e_gamut_idx], i = e_gamut_idx](auto &data) { data.gamut_mapp_j[i] = edit; }
          });
        }

        ImGui::Separator();
        
        ImGui::ColorEdit3("Color, coords", gamut_colr_i.data(), ImGuiColorEditFlags_Float);
        ImGui::ColorEdit3("Color, actual", gamut_actual.data(), ImGuiColorEditFlags_Float);
        ImGui::ColorEdit3("Color, error",  gamut_error.data(), ImGuiColorEditFlags_Float);

        ImGui::Separator();

        ImGui::SliderFloat3("Color offset", gamut_offs_j.data(), -1.f, 1.f);

        ImGui::Separator();

        // Compute resulting other color and error
        Colr other_expected = gamut_colr_i + gamut_offs_j;
        Colr other_actual   = e_mappings[e_gamut_mapp_j[e_gamut_idx]].apply_color(gamut_spec);
        Colr other_error = (other_actual - other_expected).abs();

        ImGui::ColorEdit3("Other, coords", other_expected.data(), ImGuiColorEditFlags_Float);
        ImGui::ColorEdit3("Other, actual", other_actual.data(), ImGuiColorEditFlags_Float);
        ImGui::ColorEdit3("Other, error", other_error.data(), ImGuiColorEditFlags_Float);

        ImGui::Separator();

        // Insert image, applying viewport texture to viewport; texture can be safely drawn 
        // to later in the render loop. Flip y-axis UVs to obtain the correct orientation.
        ImGui::Image(ImGui::to_ptr(i_draw_texture.object()), texture_size, eig::Vector2f(0, 1), eig::Vector2f(1, 0));

        // Handle input
        if (ImGui::IsItemHovered()) {
          if (!ImGuizmo::IsUsing()) {
            eval_camera(info);
          }
          eval_gizmo(info);
        }
      }

      ImGui::End();
    }
  }
  
  void GamutEditorTask::eval_camera(detail::TaskEvalInfo &info) {
    met_trace_full();
    
    // Get shared resources
    auto &i_arcball    = info.get_resource<detail::Arcball>("arcball");
    auto &i_texture    = info.get_resource<gl::Texture2d3f>("draw_texture");

    // Adjust arcball to viewport's new size
    i_arcball.m_aspect = i_texture.size().x() / i_texture.size().y();

    // Apply scroll delta: scroll wheel only for now
    auto &io = ImGui::GetIO();
    i_arcball.set_dist_delta(-0.5f * io.MouseWheel);

    // Apply move delta: middle mouse OR left mouse + ctrl
    if (io.MouseDown[2] || (io.MouseDown[0] && io.KeyCtrl)) {
      i_arcball.set_pos_delta(eig::Array2f(io.MouseDelta) / i_texture.size().cast<float>());
    }

    i_arcball.update_matrices();
  }

  void GamutEditorTask::eval_gizmo(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &i_arcball    = info.get_resource<detail::Arcball>("arcball");
    auto &e_gamut_idx  = info.get_resource<int>("viewport", "gamut_selection");
    auto &e_ocs_centr  = info.get_resource<Colr>("gen_metamer_ocs", fmt::format("ocs_center_{}", e_gamut_idx));
    auto &e_app_data   = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_gamut_colr = e_app_data.project_data.gamut_colr_i;
    auto &e_gamut_offs = e_app_data.project_data.gamut_offs_j;

    // Anchor position is colr + offset, minus center offset 
    eig::Array3f anchor_pos = e_gamut_colr[e_gamut_idx] 
                            + e_gamut_offs[e_gamut_idx]
                            - e_ocs_centr;
    auto anchor_trf = eig::Affine3f(eig::Translation3f(anchor_pos));
    auto pre_pos    = anchor_trf * eig::Vector3f(0, 0, 0);

    // Insert ImGuizmo manipulator at anchor position
    eig::Vector2f rmin = ImGui::GetItemRectMin(), rmax = ImGui::GetItemRectSize();
    ImGuizmo::SetRect(rmin.x(), rmin.y(), rmax.x(), rmax.y());
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::Manipulate(i_arcball.view().data(), 
                         i_arcball.proj().data(),
                         ImGuizmo::OPERATION::TRANSLATE, 
                         ImGuizmo::MODE::LOCAL, 
                         anchor_trf.data());

    // After transformation update, we transform a second point to obtain translation distance
    auto post_pos = anchor_trf * eig::Vector3f(0, 0, 0);
    auto transl   = (post_pos - pre_pos).eval();

    // Start gizmo drag
    if (ImGuizmo::IsUsing() && !m_is_gizmo_used) {
      m_is_gizmo_used = true;
      m_offs_prev = e_gamut_offs;
    }

    // Halfway gizmo drag
    if (ImGuizmo::IsUsing()) {
      e_gamut_offs[e_gamut_idx] = (e_gamut_offs[e_gamut_idx] + transl.array()).eval();
    }

    // End gizmo drag
    if (!ImGuizmo::IsUsing() && m_is_gizmo_used) {
      m_is_gizmo_used = false;
      
      // Register data edit as drag finishes
      e_app_data.touch({ 
        .name = "Move gamut offsets", 
        .redo = [edit = e_gamut_offs](auto &data) { data.gamut_offs_j = edit; }, 
        .undo = [edit = m_offs_prev](auto &data) { data.gamut_offs_j = edit; }
      });
    }
  }
} // namespace met