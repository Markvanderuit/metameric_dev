#include <metameric/components/views/task_gamut_editor.hpp>
#include <metameric/components/views/gamut_viewport/task_draw_begin.hpp>
#include <metameric/components/views/gamut_viewport/task_draw_ocs.hpp>
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
    info.emplace_resource<detail::Arcball>("arcball", { .e_eye = 1.5f, .e_center = 0.5f });

    // Add subtasks in reverse order
    info.emplace_task_after<DrawEndTask>(name(), name() + "_draw_end", name());
    info.emplace_task_after<DrawOcsTask>(name(), name() + "_draw_ocs", name());
    info.emplace_task_after<DrawBeginTask>(name(), name() + "_draw_begin", name());

    // Start with gizmo inactive
    m_is_gizmo_used = false;
  }

  void GamutEditorTask::dstr(detail::TaskDstrInfo &info) {
    // Remove subtasks
    info.remove_task(name() + "_draw_begin");
    info.remove_task(name() + "_draw_ocs");
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
        auto &e_ocs_center   = info.get_resource<Colr>("gen_ocs", "ocs_centr");
        auto &e_gamut_colr   = e_app_data.project_data.gamut_colr_i;
        auto &e_gamut_offs   = e_app_data.project_data.gamut_colr_j;
        auto &e_gamut_spec   = e_app_data.project_data.gamut_spec;
        auto &e_gamut_mapp_i = e_app_data.project_data.gamut_mapp_i;
        auto &e_gamut_mapp_j = e_app_data.project_data.gamut_mapp_j;
        auto &e_mappings     = e_app_data.loaded_mappings;

        // Obtain colors at gamut's point positions
        std::array<Colr, 4> spectra_to_colors;
        std::ranges::transform(e_gamut_spec, spectra_to_colors.begin(),
          [](const auto &s) { return reflectance_to_color(s, { .cmfs = models::cmfs_srgb }).eval(); });
          
        // Render selected reflectance and colors
        Spec &gamut_spec = e_gamut_spec[e_gamut_idx];
        Colr &gamut_colr_i = e_gamut_colr[e_gamut_idx];
        Colr &gamut_offs = e_gamut_offs[e_gamut_idx];

        // Show actual resulting color and error
        Colr gamut_actual = e_mappings[e_gamut_mapp_i[e_gamut_idx]].apply_color(gamut_spec);
        Colr gamut_error  = (gamut_actual - gamut_colr_i).abs();

        ImGui::PlotLines("Reflectance", gamut_spec.data(), gamut_spec.rows(), 0, nullptr, 0.f, 1.f, plot_size);

        ImGui::Separator();
        
        ImGui::ColorEdit3("Color, coords", gamut_colr_i.data(), ImGuiColorEditFlags_Float);
        ImGui::ColorEdit3("Color, actual", gamut_actual.data(), ImGuiColorEditFlags_Float);
        ImGui::ColorEdit3("Color, error", gamut_error.data(), ImGuiColorEditFlags_Float);

        ImGui::Separator();

        ImGui::SliderFloat3("Color offset", gamut_offs.data(), -1.f, 1.f);

        ImGui::Separator();

        Colr metam_expected = e_ocs_center + gamut_offs;
        Colr metam_actual   = e_mappings[e_gamut_mapp_j[e_gamut_idx]].apply_color(gamut_spec);
        ImGui::ColorEdit3("Metamer, expctd", metam_expected.data(), ImGuiColorEditFlags_Float);
        ImGui::ColorEdit3("Metamer, actual", metam_actual.data(), ImGuiColorEditFlags_Float);

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
    auto &i_arcball   = info.get_resource<detail::Arcball>("arcball");
    auto &e_gamut_idx  = info.get_resource<int>("viewport", "gamut_selection");
    auto &e_app_data   = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_gamut_colr = e_app_data.project_data.gamut_colr_i;
    auto &e_gamut_offs = e_app_data.project_data.gamut_colr_j;

    // Anchor position is colr + offset, minus center offset 
    eig::Array3f anchor_pos = (e_gamut_colr[e_gamut_idx] + e_gamut_offs[e_gamut_idx]) - 0.5f;
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

    // After transformation update, we transform a second point to obtain
    // translation distance
    auto post_pos = anchor_trf * eig::Vector3f(0, 0, 0);
    auto transl   = (post_pos - pre_pos).eval();

    // Start gizmo drag
    if (ImGuizmo::IsUsing() && !m_is_gizmo_used) {
      m_is_gizmo_used = true;
      m_offs_prev = e_gamut_offs;
    }

    // Halfway gizmo drag
    if (ImGuizmo::IsUsing()) {
      e_gamut_offs[e_gamut_idx] = (e_gamut_offs[e_gamut_idx] + transl.array()).min(1.f).max(0.f);
    }

    // End gizmo drag
    if (!ImGuizmo::IsUsing() && m_is_gizmo_used) {
      m_is_gizmo_used = false;
      
      // Register data edit as drag finishes
      e_app_data.touch({ 
        .name = "Move gamut offsets", 
        .redo = [edit = e_gamut_offs](auto &data) { data.gamut_colr_j = edit; }, 
        .undo = [edit = m_offs_prev](auto &data) { data.gamut_colr_j = edit; }
      });
    }

    auto &io = ImGui::GetIO();
  }
} // namespace met