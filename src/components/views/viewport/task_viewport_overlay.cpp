#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/viewport/task_viewport_overlay.hpp>
#include <metameric/components/views/viewport/task_draw_color_solid.hpp>
#include <metameric/components/views/viewport/task_draw_weights.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <ImGuizmo.h>
#include <numeric>
#include <ranges>
#include <utility>

namespace met {
  constexpr auto window_flags = ImGuiWindowFlags_AlwaysAutoResize 
    | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar
    | ImGuiWindowFlags_NoResize  | ImGuiWindowFlags_NoMove
    | ImGuiWindowFlags_NoFocusOnAppearing;

  constexpr float    overlay_width   = 256.f;
  constexpr float    overlay_spacing = 16.f;
  const eig::Array2f overlay_padding = 16.f;

  ViewportOverlayTask::ViewportOverlayTask(const std::string &name)
  : detail::AbstractTask(name, true) { }

  void ViewportOverlayTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Share resources
    info.emplace_resource<int>("constr_selection", -1);
    info.emplace_resource<uint>("weight_mapping", 0);
    info.emplace_resource<gl::Texture2d4f>("srgb_weights_target",     { .size = 1 });
    info.emplace_resource<gl::Texture2d4f>("lrgb_color_solid_target", { .size = 1 });
    info.emplace_resource<gl::Texture2d4f>("srgb_color_solid_target", { .size = 1 });
    info.emplace_resource<detail::Arcball>("arcball", { .e_eye = 1.0f, .e_center = 0.0f, .dist_delta_mult = -0.075f });

    // Add subtask to handle metamer set draw
    info.emplace_task_after<DrawColorSolidTask>(name(), name() + "_draw_color_solid", name());
    info.emplace_task_after<DrawWeightsTask>(name(),    name() + "_draw_weights",     name());

    // Start with gizmo inactive
    m_is_gizmo_used = false;
  }

  void ViewportOverlayTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();

    // Remove subtasks
    info.remove_task(name() + "_draw_weights");
    info.remove_task(name() + "_draw_color_solid");
  }

  void ViewportOverlayTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Adjust tooltip settings based on current selection
    auto &e_selection = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection");
    auto &i_cstr_slct = info.get_resource<int>("constr_selection");
    auto &e_appl_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data = e_appl_data.project_data;
    auto &e_verts     = e_proj_data.gamut_verts;
    
    if (e_selection.size() != 1) {
      i_cstr_slct = -1;
    }

    // Only spawn any tooltips on non-empty gamut selection
    guard(!e_selection.empty());

    // Compute viewport offset and size, minus ImGui's tab bars etc
    eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                               + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

    // Track these positions/sizes so overtays are evenly spaced
    eig::Array2f view_posi = viewport_offs + overlay_padding, 
                 view_size = { 0.f, 0.f };
                //  view_size = { overlay_width * ImGui::GetIO().DisplayFramebufferScale.x, 0.f };
    
    // Spawn window with selection info if one or more vertices are selected
    for (const uint &i : e_selection) {
      view_size.y() = 0.f;
      
      // Set window state for next window
      ImGui::SetNextWindowPos(view_posi);
      ImGui::SetNextWindowSize(view_size);
      
      if (ImGui::Begin(fmt::format("Vertex {}", i).c_str(), nullptr, window_flags)) {
        eval_overlay_vertex(info, i);
      }

      // Capture window size to offset next window by this amount
      view_size = static_cast<eig::Array2f>(ImGui::GetWindowSize());  
      view_posi.y() += view_size.y() + overlay_spacing;

      ImGui::End();
    }

    // Spawn window for metamer set editing if there is one vertex selected
    if (e_selection.size() == 1 && i_cstr_slct >= 0) {
      view_size.y() = 0.f;

      // Set window state for next window
      // auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};
      ImGui::SetNextWindowPos(view_posi);
      ImGui::SetNextWindowSize(view_size);

      // Window open flag, and window title
      bool window_open = true;
      auto window_name = fmt::format("Editing {} ({} -> {})",
        i_cstr_slct, 
        e_proj_data.mapping_name(e_verts[e_selection[0]].mapp_i),
        e_proj_data.mapping_name(e_verts[e_selection[0]].mapp_j[i_cstr_slct]));
      
      if (ImGui::Begin(window_name.c_str(), &window_open, window_flags)) {
        eval_overlay_color_solid(info, e_selection[0]);
      }

      // Window was closed, deselect constraint
      if (!window_open)
        i_cstr_slct = -1;
        
      // Capture window size to offset next window by this amount
      view_size = static_cast<eig::Array2f>(ImGui::GetWindowSize());  
      view_posi.y() += view_size.y() + overlay_spacing;

      ImGui::End();
    }

    // Spawn window for selection display
    {
      view_size.y() = 0.f;
      
      // Set window state for next window
      ImGui::SetNextWindowPos(view_posi);
      ImGui::SetNextWindowSize(view_size);
      
      if (ImGui::Begin("Affected region", NULL, window_flags | ImGuiWindowFlags_NoTitleBar)) {
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
    auto &i_cstr_slct  = info.get_resource<int>("constr_selection");
    auto &e_appl_data  = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data  = e_appl_data.project_data;
    auto &e_vert       = e_proj_data.gamut_verts[i];
    auto &e_spec       = info.get_resource<std::vector<Spec>>("gen_spectral_gamut", "gamut_spec")[i];
    
    // Plot reflectances
    ImGui::PlotLines("Reflectance", e_spec.data(), wavelength_samples, 0, nullptr, 0.f, 1.f, { 0.f, 64.f });

    // Plot vertex settings for primary color
    if (ImGui::CollapsingHeader("Primary Color", ImGuiTreeNodeFlags_DefaultOpen)) {
      Mapp mapp    = e_proj_data.mapping_data(e_vert.mapp_i);
      Colr colr_i  = e_vert.colr_i;
      Colr rtrip_i = mapp.apply_color(e_spec);
      Colr error_i = (rtrip_i - colr_i).abs().eval();
      
      ImGui::ColorEdit3("Value",     linear_srgb_to_gamma_srgb(colr_i).data(),  ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
      ImGui::SameLine();
      ImGui::ColorEdit3("Roundtrip", linear_srgb_to_gamma_srgb(rtrip_i).data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
      ImGui::SameLine();
      ImGui::ColorEdit3("Error",     linear_srgb_to_gamma_srgb(error_i).data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);

      // Selector for primary color mapping index, operating on a local copy
      uint l_mapp_i = e_vert.mapp_i;
      ImGui::SetNextItemWidth(ImGui::GetWindowSize().x * 0.9f);
      if (ImGui::BeginCombo("##Mapping", e_proj_data.mapping_name(l_mapp_i).c_str())) {
        for (uint j = 0; j < e_proj_data.mappings.size(); ++j) {
          if (ImGui::Selectable(e_proj_data.mapping_name(j).c_str(), j == l_mapp_i)) {
            l_mapp_i = j;
          }
        }
        ImGui::EndCombo();
      }
      
      // Register potential changes to mapping data
      if (l_mapp_i != e_vert.mapp_i) {
        e_appl_data.touch({
          .name = "Change mapping index",
          .redo = [edit = l_mapp_i,      i = i](auto &data) { data.gamut_verts[i].mapp_i = edit; },
          .undo = [edit = e_vert.mapp_i, i = i](auto &data) { data.gamut_verts[i].mapp_i = edit; }
        });
      }
    }

    // Plot vertex settings for secondary colors
    for (uint j = 0; j < e_vert.colr_j.size(); ++j) {
      auto constraint_name = fmt::format("Secondary {} ({} -> {})",
        j, 
        e_proj_data.mapping_name(e_vert.mapp_i),
        e_proj_data.mapping_name(e_vert.mapp_j[j]));
      auto constraint_id = fmt::format("constraint{}", j);
      
      bool color_visible = true;
      if (ImGui::CollapsingHeader(constraint_name.c_str(), &color_visible, ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushID(constraint_id.c_str());

        Mapp mapp    = e_proj_data.mapping_data(e_vert.mapp_j[j]);
        Colr colr_j  = e_vert.colr_j[j];
        Colr rtrip_j = mapp.apply_color(e_spec);
        Colr error_j = (rtrip_j - colr_j).abs().eval();

        ImGui::ColorEdit3("Value",     linear_srgb_to_gamma_srgb(colr_j).data(),  ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
        ImGui::SameLine();
        ImGui::ColorEdit3("Roundtrip", linear_srgb_to_gamma_srgb(rtrip_j).data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
        ImGui::SameLine();
        ImGui::ColorEdit3("Error",     linear_srgb_to_gamma_srgb(error_j).data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);

        // Selector for secondary color mapping index, operating on a local copy
        uint l_mapp_j = e_vert.mapp_j[j];
        if (ImGui::BeginCombo("##Mapping", e_proj_data.mapping_name(l_mapp_j).c_str())) {
          for (uint k = 0; k < e_proj_data.mappings.size(); ++k) {
            if (ImGui::Selectable(e_proj_data.mapping_name(k).c_str(), k == l_mapp_j)) {
              l_mapp_j = k;
            }
          }
          ImGui::EndCombo();
        }

        // Register potential changes to constraint data
        if (l_mapp_j != e_vert.mapp_j[j]) {
          e_appl_data.touch({
            .name = "Change constraint mapping",
            .redo = [edit = l_mapp_j,         i = i, j = j](auto &data) { data.gamut_verts[i].mapp_j[j] = edit; },
            .undo = [edit = e_vert.mapp_j[j], i = i, j = j](auto &data) { data.gamut_verts[i].mapp_j[j] = edit; }
          });
        }

        ImGui::SameLine();
        if (ImGui::Button("Edit"))
          i_cstr_slct  = j;

        // Button to move constraint up
        if (j > 0) ImGui::SameLine();
        if (j > 0 && ImGui::ArrowButton("up", ImGuiDir_Up)) {
          e_appl_data.touch({
            .name = "Swapped constraint mappings",
            .redo = [i = i, j = j](auto &data) {  
              std::swap(data.gamut_verts[i].colr_j[j], data.gamut_verts[i].colr_j[j - 1]);
              std::swap(data.gamut_verts[i].mapp_j[j], data.gamut_verts[i].mapp_j[j - 1]);
            },
            .undo = [i = i, j = j](auto &data) {  
              std::swap(data.gamut_verts[i].colr_j[j], data.gamut_verts[i].colr_j[j - 1]);
              std::swap(data.gamut_verts[i].mapp_j[j], data.gamut_verts[i].mapp_j[j - 1]);
            }
          });
        }
        if (j < e_vert.colr_j.size() - 1) ImGui::SameLine();
        if (j < e_vert.colr_j.size() - 1 && ImGui::ArrowButton("down", ImGuiDir_Down)) {
          e_appl_data.touch({
            .name = "Swapped constraint mappings",
            .redo = [i = i, j = j](auto &data) {  
              std::swap(data.gamut_verts[i].colr_j[j], data.gamut_verts[i].colr_j[j + 1]);
              std::swap(data.gamut_verts[i].mapp_j[j], data.gamut_verts[i].mapp_j[j + 1]);
            },
            .undo = [i = i, j = j](auto &data) {  
              std::swap(data.gamut_verts[i].colr_j[j], data.gamut_verts[i].colr_j[j + 1]);
              std::swap(data.gamut_verts[i].mapp_j[j], data.gamut_verts[i].mapp_j[j + 1]);
            }
          });
        }

        ImGui::PopID();
      }

      // Secondary color was deleted, register changes to constraint data
      if (!color_visible) {
        e_appl_data.touch({
            .name = "Delete color constraint",
            .redo = [i = i, j = j](auto &data) { 
              data.gamut_verts[i].colr_j.erase(data.gamut_verts[i].colr_j.begin() + j); 
              data.gamut_verts[i].mapp_j.erase(data.gamut_verts[i].mapp_j.begin() + j); 
            }, .undo = [edit = e_vert,  i = i, j = j](auto &data) { data.gamut_verts[i] = edit; }
        });
      }
    }

    // Spawn button to add an extra constraint, if necessary
    ImGui::SpacedSeparator();
    if (ImGui::Button("Add color constraint")) {
      // Submit data to add color constraint
      e_appl_data.touch({
          .name = "Add color constraint",
          .redo = [i = i](auto &data) { 
            data.gamut_verts[i].colr_j.push_back(data.gamut_verts[i].colr_i); 
            data.gamut_verts[i].mapp_j.push_back(0); 
          }, .undo = [edit = e_vert,  i = i](auto &data) { data.gamut_verts[i] = edit; }
      });

      // Set displayed constraint in viewport to this constraint, iff a constraint was selected
      if (i_cstr_slct != -1)
        i_cstr_slct = e_vert.colr_j.size() - 1;
    }
  }

  void ViewportOverlayTask::eval_overlay_color_solid(detail::TaskEvalInfo &info, uint i) {
    met_trace_full();
        
    // Get shared resources
    auto &i_cstr_slct   = info.get_resource<int>("constr_selection");
    auto &i_arcball     = info.get_resource<detail::Arcball>("arcball");
    auto &i_lrgb_target = info.get_resource<gl::Texture2d4f>("lrgb_color_solid_target");
    auto &i_srgb_target = info.get_resource<gl::Texture2d4f>("srgb_color_solid_target");
    auto &e_csol_cntr   = info.get_resource<Colr>("gen_color_solids", "csol_cntr");
    auto &e_appl_data   = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_vert        = e_appl_data.project_data.gamut_verts[i];

    // Only continue if at least one secondary color mapping is present
    guard(!e_vert.colr_j.empty());
    
    // Ensure constraint selection is viable, in case a constraint was deleted
    if (i_cstr_slct >= e_vert.colr_j.size())
      i_cstr_slct = e_vert.colr_j.size() - 1;

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
    if (ImGui::IsItemHovered()) {
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
      eig::Vector3f trf_trnsl = e_vert.colr_j[i_cstr_slct] - e_csol_cntr;
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
        m_colr_prev = e_vert.colr_j[i_cstr_slct];
        m_is_gizmo_used = true;
      }

      // Register gizmo use
      if (ImGuizmo::IsUsing())
        e_vert.colr_j[i_cstr_slct] = (trf_delta * e_vert.colr_j[i_cstr_slct].matrix()).array();

      // Register gizmo use end, update positions
      if (!ImGuizmo::IsUsing() && m_is_gizmo_used) {
        m_is_gizmo_used = false;
        
        // Register data edit as drag finishes
        e_appl_data.touch({ 
          .name = "Move gamut offsets", 
          .redo = [edit = e_vert.colr_j[i_cstr_slct], 
                  i = i, 
                  j = i_cstr_slct](auto &data) { data.gamut_verts[i].colr_j[j] = edit; }, 
          .undo = [edit = m_colr_prev,                 
                  i = i, 
                  j = i_cstr_slct](auto &data) { data.gamut_verts[i].colr_j[j] = edit; }
        });
      }
    }

    // Add button to move gamut offset back to the metamer boundary's centroid
    ImGui::SpacedSeparator();
    if (ImGui::Button("Center value")) {
      e_appl_data.touch({ 
        .name = "Center gamut offsets", 
        .redo = [edit = e_csol_cntr, 
                 i = i, 
                 j = i_cstr_slct](auto &data) { data.gamut_verts[i].colr_j[j] = edit; }, 
        .undo = [edit = e_vert.colr_j[i_cstr_slct],                 
                 i = i, 
                 j = i_cstr_slct](auto &data) { data.gamut_verts[i].colr_j[j] = edit; }
      });
    }
  }

  void ViewportOverlayTask::eval_overlay_weights(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &i_srgb_target    = info.get_resource<gl::Texture2d4f>("srgb_weights_target");
    auto &i_weight_mapping = info.get_resource<uint>("weight_mapping");
    auto &e_appl_data      = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data      = e_appl_data.project_data;
    auto &e_mappings       = e_appl_data.project_data.mappings;
    
    // Compute viewport size minus ImGui's tab bars etc
    eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                               - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
    eig::Array2f texture_size  = viewport_size.x();

    // (Re-)create viewport texture if necessary; attached framebuffers are resized in subtask
    if (!i_srgb_target.is_init() || (i_srgb_target.size() != texture_size.cast<uint>()).all()) {
      i_srgb_target = {{ .size = texture_size.cast<uint>() }};
    }
    
    // Insert image, applying viewport texture to viewport; texture can be safely drawn 
    // to later in the render loop. Flip y-axis UVs to obtain the correct orientation.
    ImGui::Image(ImGui::to_ptr(i_srgb_target.object()), texture_size, eig::Vector2f(0, 1), eig::Vector2f(1, 0));

    ImGui::SpacedSeparator();

    // Insert mapping selector for which color mapping is used in the weights overlay
    i_weight_mapping = std::min(i_weight_mapping, static_cast<uint>(e_mappings.size() - 1));
    if (ImGui::BeginCombo("Mapping", e_proj_data.mapping_name(i_weight_mapping).c_str())) {
      for (uint i = 0; i < e_mappings.size(); ++i) {
        if (ImGui::Selectable(e_proj_data.mapping_name(i).c_str(), i == i_weight_mapping)) {
          i_weight_mapping = i;
        }
      }
      ImGui::EndCombo();
    }
  }
} // namespace met