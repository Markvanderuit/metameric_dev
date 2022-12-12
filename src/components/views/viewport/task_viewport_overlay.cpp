#include <metameric/components/views/viewport/task_viewport_overlay.hpp>

namespace met {
  constexpr auto window_flags = ImGuiWindowFlags_AlwaysAutoResize 
    | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar
    | ImGuiWindowFlags_NoResize  | ImGuiWindowFlags_NoMove
    | ImGuiWindowFlags_NoFocusOnAppearing;

  constexpr float    overlay_spacing = 16.f;
  const eig::Array2f overlay_padding = 16.f;

  ViewportOverlayTask::ViewportOverlayTask(const std::string &name)
  : detail::AbstractTask(name, true) { }

  void ViewportOverlayTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Share resources
    info.emplace_resource<gl::Texture2d4f>("lrgb_target", { .size = 1 });
    info.emplace_resource<gl::Texture2d4f>("srgb_target", { .size = 1 });
    info.emplace_resource<detail::Arcball>("arcball",     { .e_eye = 1.0f, .e_center = 0.0f, .dist_delta_mult = -0.075f });

    // Add subtask to handle metamer set draw
    info.emplace_task_after<DrawColorSolidTask>(name(), name() + "_draw_color_solid", name());

    // Start with gizmo inactive
    m_is_gizmo_used = false;
  }

  void ViewportOverlayTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();

    // Remove subtasks
    info.remove_task(name() + "_draw_color_solid");
  }

  void ViewportOverlayTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Only spawn any of these tooltips on non-empty gamut selection
    auto &e_gamut_index = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection");
    guard(!e_gamut_index.empty());

    // Compute viewport offset and size, minus ImGui's tab bars etc
    eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                               + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

    // Track these positions/sizes so overtays are evenly spaced
    eig::Array2f view_posi = viewport_offs + overlay_padding, view_size = 0.f;

    // Spawn window with selection info if one or more vertices are selected
    for (const uint &i : e_gamut_index) {
      // Set window state for next window
      ImGui::SetNextWindowPos(view_posi);
      
      if (ImGui::Begin(fmt::format("Vertex {}", i).c_str(), nullptr, window_flags)) {
        eval_overlay_vertex(info, i);
      }

      // Capture window size to offset next window by this amount
      view_size = static_cast<eig::Array2f>(ImGui::GetWindowSize());  
      view_posi.y() += view_size.y() + overlay_spacing;

      ImGui::End();
    }

    // Spawn window for metamer set editing if there is one vertex selected
    if (e_gamut_index.size() == 1) {
      view_size.y() = 0.f;

      // Set window state for next window
      auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};
      ImGui::SetNextWindowPos(view_posi);
      ImGui::SetNextWindowSize(view_size);
      
      if (ImGui::Begin("Metamer set", NULL, window_flags | ImGuiWindowFlags_NoTitleBar)) {
        eval_overlay_color_solid(info);
      }
        
      // Capture window size to offset next window by this amount
      view_size = static_cast<eig::Array2f>(ImGui::GetWindowSize());  
      view_posi.y() += view_size.y() + overlay_spacing;

      ImGui::End();
    }

    // Spawn window for selection display
    {
      view_size.y() = 0.f;
      
      // Set window state for next window
      auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};
      ImGui::SetNextWindowPos(view_posi);
      ImGui::SetNextWindowSize(view_size);
      
      if (ImGui::Begin("Vertex weights", NULL, window_flags | ImGuiWindowFlags_NoTitleBar)) {
        eval_overlay_weights(info);
      }
        
      // Capture window size to offset next window by this amount
      view_size = static_cast<eig::Array2f>(ImGui::GetWindowSize());  
      view_posi.y() += view_size.y() + overlay_spacing;

      ImGui::End();
    }
  }

  void ViewportOverlayTask::eval_overlay_vertex(detail::TaskEvalInfo &info, uint i) {
    met_trace_full();

    // Get shared resources
    auto &e_gamut_spec   = info.get_resource<std::vector<Spec>>("gen_spectral_gamut", "gamut_spec");
    auto &e_app_data     = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_prj_data     = e_app_data.project_data;
    auto &e_gamut_mapp_i = e_prj_data.gamut_mapp_i;
    auto &e_gamut_mapp_j = e_prj_data.gamut_mapp_j;
    auto &e_mapping_data = e_prj_data.mappings;

    // Compute expected offset color, roundtrip color, roundtrip error
    Colr gamut_colr_i = e_prj_data.gamut_colr_i[i], gamut_colr_j = gamut_colr_i + e_prj_data.gamut_offs_j[i];
    Colr gamut_actual_i = e_app_data.loaded_mappings[e_gamut_mapp_i[i]].apply_color(e_gamut_spec[i]);
    Colr gamut_actual_j = e_app_data.loaded_mappings[e_gamut_mapp_j[i]].apply_color(e_gamut_spec[i]);
    Colr gamut_error_i  = (gamut_actual_i - gamut_colr_i).abs();
    Colr gamut_error_j  = (gamut_actual_j - gamut_colr_j).abs();

    // Plot reflectances
    ImGui::PlotLines("Reflectance", e_gamut_spec[i].data(), wavelength_samples, 0, nullptr, 0.f, 1.f, { 0.f, 64.f });
    ImGui::Separator();

    // Plot expected color values
    ImGui::Text("Color values");
    ImGui::ColorEdit3("Value 0", linear_srgb_to_gamma_srgb(gamut_colr_i).data(), ImGuiColorEditFlags_Float);
    ImGui::ColorEdit3("Value 1", linear_srgb_to_gamma_srgb(gamut_colr_j).data(), ImGuiColorEditFlags_Float);
    ImGui::Separator();

    // Plot roundtrip color values
    ImGui::Text("Color roundtrip");
    ImGui::ColorEdit3("Value, 0", linear_srgb_to_gamma_srgb(gamut_actual_i).data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
    ImGui::SameLine();
    ImGui::ColorEdit3("Value, 1", linear_srgb_to_gamma_srgb(gamut_actual_j).data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit3("Error, 0", gamut_error_i.data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
    ImGui::SameLine();
    ImGui::ColorEdit3("Error, 1", gamut_error_j.data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
    ImGui::Separator();

    // Selector for first mapping index, operating on a local copy
    uint l_gamut_mapp_i = e_gamut_mapp_i[i];
    if (ImGui::BeginCombo("Mapping 0", e_mapping_data[l_gamut_mapp_i].first.c_str())) {
      for (uint j = 0; j < e_mapping_data.size(); ++j) {
        if (ImGui::Selectable(e_mapping_data[j].first.c_str(), j == l_gamut_mapp_i)) {
          l_gamut_mapp_i = j;
        }
      }
      ImGui::EndCombo();
    }

    // Selector for second mapping index, operating on a local copy
    uint l_gamut_mapp_j = e_gamut_mapp_j[i];
    if (ImGui::BeginCombo("Mapping 1", e_mapping_data[l_gamut_mapp_j].first.c_str())) {
      for (uint j = 0; j < e_mapping_data.size(); ++j) {
        if (ImGui::Selectable(e_mapping_data[j].first.c_str(), j == l_gamut_mapp_j)) {
          l_gamut_mapp_j = j;
        }
      }
      ImGui::EndCombo();
    }

    // If changes to local copies were made, register a data edit
    if (l_gamut_mapp_i != e_gamut_mapp_i[i]) {
      e_app_data.touch({
        .name = "Change gamut mapping 0",
        .redo = [edit = l_gamut_mapp_i,    j = i](auto &data) { data.gamut_mapp_i[j] = edit; },
        .undo = [edit = e_gamut_mapp_i[i], j = i](auto &data) { data.gamut_mapp_i[j] = edit; }
      });
    }
    if (l_gamut_mapp_j != e_gamut_mapp_j[i]) {
      e_app_data.touch({
        .name = "Change gamut mapping 1",
        .redo = [edit = l_gamut_mapp_j,    j = i](auto &data) { data.gamut_mapp_j[j] = edit; },
        .undo = [edit = e_gamut_mapp_j[i], j = i](auto &data) { data.gamut_mapp_j[j] = edit; }
      });
    }
  }

  void ViewportOverlayTask::eval_overlay_color_solid(detail::TaskEvalInfo &info) {
    met_trace_full();
        
    // Get shared resources
    auto &i_arcball     = info.get_resource<detail::Arcball>("arcball");
    auto &i_lrgb_target = info.get_resource<gl::Texture2d4f>("lrgb_target");
    auto &i_srgb_target = info.get_resource<gl::Texture2d4f>("srgb_target");
    auto &e_selection   = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection")[0];
    auto &e_ocs_center  = info.get_resource<std::vector<Colr>>("gen_color_solids", "ocs_centers")[e_selection];
    auto &e_app_data    = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data   = e_app_data.project_data;
    auto &e_vert        = e_proj_data.gamut_colr_i;
    auto &e_offs        = e_proj_data.gamut_offs_j;

    // Compute viewport size minus ImGui's tab bars etc
    eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                               - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
    eig::Array2f texture_size  = viewport_size.x();

    // (Re-)create viewport texture if necessary; attached framebuffers are resized in subtask
    if (!i_lrgb_target.is_init() || (i_lrgb_target.size() != viewport_size.cast<uint>()).all()) {
      i_lrgb_target = {{ .size = texture_size.cast<uint>() }};
      i_srgb_target = {{ .size = texture_size.cast<uint>() }};
    }

    // Insert image, applying viewport texture to viewport; texture can be safely drawn 
    // to later in the render loop. Flip y-axis UVs to obtain the correct orientation.
    ImGui::Image(ImGui::to_ptr(i_srgb_target.object()), texture_size, eig::Vector2f(0, 1), eig::Vector2f(1, 0));
      
    // Ensure mouse is over window for the following section
    guard(ImGui::IsItemHovered());

    // If gizmo is not in use, process input to update camera
    if (!ImGuizmo::IsUsing()) {
      auto &io = ImGui::GetIO();
      i_arcball.m_aspect = i_lrgb_target.size().x() / i_lrgb_target.size().y();
      i_arcball.set_dist_delta(io.MouseWheel);
      if (io.MouseDown[2] || (io.MouseDown[0] && io.KeyCtrl))
        i_arcball.set_pos_delta(eig::Array2f(io.MouseDelta) / i_lrgb_target.size().cast<float>());
      i_arcball.update_matrices();
    }

    // Anchor position for ocs gizmo is colr + offset, minus center offset 
    eig::Vector3f trf_trnsl = e_vert[e_selection] + e_offs[e_selection] - e_ocs_center;
    eig::Affine3f trf_basic = eig::Affine3f(eig::Translation3f(trf_trnsl));
    eig::Affine3f trf_delta = eig::Affine3f::Identity();

    // Insert ImGuizmo manipulator at anchor position
    eig::Vector2f rmin = ImGui::GetItemRectMin(), rmax = ImGui::GetItemRectSize();
    ImGuizmo::SetRect(rmin.x(), rmin.y(), rmax.x(), rmax.y());
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::Manipulate(i_arcball.view().data(), i_arcball.proj().data(), 
      ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::MODE::LOCAL, trf_basic.data(), trf_delta.data());
    
    // Register gizmo use start, cache current offset position
    if (ImGuizmo::IsUsing() && !m_is_gizmo_used) {
      m_offs_prev = e_offs;
      m_is_gizmo_used = true;
    }

    // Register gizmo use
    if (ImGuizmo::IsUsing())
    e_offs[e_selection] = (trf_delta * e_offs[e_selection].matrix()).array();

    // Register gizmo use end, update positions
    if (!ImGuizmo::IsUsing() && m_is_gizmo_used) {
      m_is_gizmo_used = false;
      
      // Register data edit as drag finishes
      e_app_data.touch({ 
        .name = "Move gamut offsets", 
        .redo = [edit = e_offs](auto &data) { data.gamut_offs_j = edit; }, 
        .undo = [edit = m_offs_prev](auto &data) { data.gamut_offs_j = edit; }
      });
    }
  }

  void ViewportOverlayTask::eval_overlay_weights(detail::TaskEvalInfo &info) {
    
  }
} // namespace met