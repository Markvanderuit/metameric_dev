#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/viewport/task_viewport_overlay.hpp>
#include <metameric/components/views/viewport/task_draw_color_solid.hpp>
#include <metameric/components/views/viewport/task_draw_weights.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/window.hpp>
#include <small_gl/utility.hpp>
#include <ImGuizmo.h>
#include <implot.h>
#include <numeric>
#include <ranges>
#include <utility>

namespace met {
  constexpr auto window_flags 
    = ImGuiWindowFlags_AlwaysAutoResize 
    | ImGuiWindowFlags_NoDocking 
    | ImGuiWindowFlags_NoResize  
    | ImGuiWindowFlags_NoMove
    | ImGuiWindowFlags_NoFocusOnAppearing;
  constexpr auto window_vertices_flags 
    = ImGuiWindowFlags_AlwaysAutoResize
    | ImGuiWindowFlags_NoDocking 
    | ImGuiWindowFlags_NoMove
    | ImGuiWindowFlags_NoFocusOnAppearing;

  // Size constants, independent of window scale
  constexpr float    overlay_width       = 300.f;
  constexpr float    overlay_vert_height = 512.f;
  constexpr float    overlay_plot_height = 128.f;
  constexpr float    overlay_spacing     = 8.f;
  const eig::Array2f overlay_padding     = 8.f;

  ViewportOverlayTask::ViewportOverlayTask(const std::string &name)
  : detail::AbstractTask(name, true) { }

  void ViewportOverlayTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Share resources
    info.emplace_resource<int>("constr_selection", -1);
    info.emplace_resource<gl::Texture2d4f>("lrgb_color_solid_target", { .size = 1 });
    info.emplace_resource<gl::Texture2d4f>("srgb_color_solid_target", { .size = 1 });
    info.emplace_resource<detail::Arcball>("arcball", { .e_eye = 1.0f, .e_center = 0.0f, .dist_delta_mult = -0.075f });

    // Add subtask to handle metamer set draw
    info.emplace_task_after<DrawColorSolidTask>(name(), name() + "_draw_color_solid", name());

    // Start with gizmo inactive
    m_is_gizmo_used = false;
    m_is_vert_edit_used = false;
    m_is_cstr_edit_used = false;
  }

  void ViewportOverlayTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();

    // Remove subtasks
    info.remove_task(name() + "_draw_color_solid");
  }

  void ViewportOverlayTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Adjust tooltip settings based on current selection
    auto &e_view_state = info.get_resource<ViewportState>("state", "viewport_state");
    auto &e_window     = info.get_resource<gl::Window>(global_key, "window");
    auto &e_vert_slct  = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection");
    auto &i_cstr_slct  = info.get_resource<int>("constr_selection");
    auto &e_appl_data  = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data  = e_appl_data.project_data;
    auto &e_verts      = e_proj_data.gamut_verts;

    // Only spawn any tooltips on non-empty gamut selection; only allow constraint selection
    // on a single vertex
    guard(!e_vert_slct.empty());
    if (e_vert_slct.size() > 1) {
      i_cstr_slct = -1;
    }

    // Compute viewport offset and size, minus ImGui's tab bars etc
    eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                               + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

    // Track these positions/sizes so overtays are evenly spaced
    eig::Array2f view_posi = viewport_offs + overlay_padding * e_window.content_scale(), 
                 view_size = { overlay_width * e_window.content_scale(), 0.f },
                 view_minv = { overlay_width * e_window.content_scale(), 0.f},
                 view_maxv = { overlay_width * e_window.content_scale(), overlay_vert_height * e_window.content_scale() };

    // Spawn window with vertex info if one or more vertices are selected
    {
      // Dictate hard size bounds
      ImGui::SetNextWindowPos(view_posi);
      ImGui::SetNextWindowSize(view_size);
      ImGui::SetNextWindowSizeConstraints(view_minv, view_maxv);
      view_size.y() = 0.f;

      if (ImGui::Begin("Vertex settings", nullptr, window_vertices_flags)) {
        if (e_vert_slct.size() == 1) {
          eval_overlay_vertex(info, e_vert_slct[0]);
        } else {
          for (uint i : e_vert_slct) {
            if (ImGui::CollapsingHeader(fmt::format("Vertex {}", i).c_str())) {
              eval_overlay_vertex(info, i);
            }
          }
        }
      }

      // Capture window size to offset next window by this amount
      view_size = static_cast<eig::Array2f>(ImGui::GetWindowSize());  
      view_posi.y() += view_size.y() + overlay_spacing * e_window.content_scale();

      ImGui::End();
    }
    
    // Spawn window for reflectance plot display
    {
      view_size.y() = 0.f;

      // Set window state for next window
      ImGui::SetNextWindowPos(view_posi);
      ImGui::SetNextWindowSize(view_size);

      if (ImGui::Begin("Vertex reflectances", nullptr, window_flags)) {
        eval_overlay_plot(info);
      }
        
      // Capture window size to offset next window by this amount
      view_size = static_cast<eig::Array2f>(ImGui::GetWindowSize());  
      view_posi.y() += view_size.y() + overlay_spacing * e_window.content_scale();

      ImGui::End();
    }

    // Spawn window for metamer set editing if there is one vertex selected
    if (e_vert_slct.size() == 1 && i_cstr_slct >= 0) {
      view_size.y() = 0.f;

      // Set window state for next window
      ImGui::SetNextWindowPos(view_posi);
      ImGui::SetNextWindowSize(view_size);

      // Window open flag, and window title
      bool window_open = true;
      auto window_name = fmt::format("Editing {}: {}",
        i_cstr_slct, 
        e_proj_data.mapping_name(e_verts[e_vert_slct[0]].mapp_j[i_cstr_slct]));
      
      if (ImGui::Begin(window_name.c_str(), &window_open, window_flags)) {
        eval_overlay_color_solid(info, e_vert_slct[0]);
      }

      // Window was closed, deselect constraint
      if (!window_open)
        i_cstr_slct = -1;
        
      // Capture window size to offset next window by this amount
      view_size = static_cast<eig::Array2f>(ImGui::GetWindowSize());  
      view_posi.y() += view_size.y() + overlay_spacing * e_window.content_scale();

      ImGui::End();
    }
  }

  void ViewportOverlayTask::eval_overlay_vertex(detail::TaskEvalInfo &info, uint i) {
    met_trace_full();
    ImGui::PushID(fmt::format("overlay_vertex_{}", i).c_str());

    // Get shared resources
    auto &e_vert_slct  = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection");
    auto &i_cstr_slct  = info.get_resource<int>("constr_selection");
    auto &e_appl_data  = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data  = e_appl_data.project_data;
    auto &e_vert       = e_proj_data.gamut_verts[i];
    auto &e_spec       = info.get_resource<std::vector<Spec>>("gen_spectral_gamut", "gamut_spec")[i];

    // Local state
    float mapp_width, edit3_width;

    // Plot vertex settings for primary color
    {
      Mapp mapp    = e_proj_data.mapping_data(e_vert.mapp_i);
      Colr colr_i  = e_vert.colr_i;
      Colr rtrip_i = mapp.apply_color(e_spec);
      Colr error_i = (rtrip_i - colr_i).abs().eval();

      edit3_width = ImGui::GetContentRegionAvail().x * .485f;
      ImGui::BeginGroup();
      ImGui::AlignTextToFramePadding();
      ImGui::BeginGroup();

      ImGui::Text("Vertex color (lRGB)");

      // Track a copy of the vertex input color for editing
      Colr colr_edit = colr_i;

      // Spawn vertex color editor 
      ImGui::SetNextItemWidth(edit3_width);
      ImGui::ColorEdit3("##Value", colr_edit.data(),  ImGuiColorEditFlags_Float);

      // Handle vertex color editor state
      if (ImGui::IsItemActive() && !m_is_vert_edit_used) {
        m_colr_prev = e_vert.colr_i;
        m_is_vert_edit_used = true;
      }
      if (ImGui::IsItemActive()) {
        e_vert.colr_i = colr_edit;
      }
      if (!ImGui::IsItemActive() && m_is_vert_edit_used) {
        m_is_vert_edit_used = false;
        e_appl_data.touch({
          .name = "Change vertex color",
          .redo = [edit = colr_edit, i = i](auto &data) { data.gamut_verts[i].colr_i = edit; },
          .undo = [edit = m_colr_prev, i = i](auto &data) { data.gamut_verts[i].colr_i = edit; },
        });
      }

      ImGui::EndGroup();
      ImGui::SameLine();
      ImGui::AlignTextToFramePadding();
      ImGui::BeginGroup();
      ImGui::Text("Roundtrip error");
      ImGui::SetNextItemWidth(edit3_width);
      ImGui::ColorEdit3("##Error", linear_srgb_to_gamma_srgb(error_i).data(), ImGuiColorEditFlags_Float);
      ImGui::EndGroup();
      ImGui::EndGroup();
      mapp_width = ImGui::GetItemRectSize().x;
    }

    ImGui::Separator();

    {
      // Selector for primary color mapping index, operating on a local copy
      ImGui::Text("Vertex mapping");
      ImGui::SetNextItemWidth(mapp_width);
      uint l_mapp_i = e_vert.mapp_i;
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

    ImGui::Separator();

    if (!e_vert.colr_j.empty()) {
      ImGui::AlignTextToFramePadding();

      // Coming column widths
      const float constr_total_width = ImGui::GetContentRegionAvail().x;
      const float constr_mapp_width  = .4f * constr_total_width;
      const float constr_colr_width  = .1f * constr_total_width;
      const float constr_edit_width  = .1f * constr_total_width;
      const float constr_arrw_width  = .1f * constr_total_width;
      const float constr_clse_width  = .05f * constr_total_width;

      // Mapping selector
      ImGui::BeginGroup(); ImGui::SetNextItemWidth(constr_mapp_width); ImGui::Text("Constraint mapping");
      for (uint j = 0; j < e_vert.colr_j.size(); ++j) {
        ImGui::PushID(j);

        // Selector for secondary color mapping index, operating on a local copy
        uint l_mapp_j = e_vert.mapp_j[j];
        ImGui::PushItemWidth(constr_mapp_width);
        if (ImGui::BeginCombo("##Mapping", e_proj_data.mapping_name(l_mapp_j).c_str())) {
          for (uint k = 0; k < e_proj_data.mappings.size(); ++k) {
            if (ImGui::Selectable(e_proj_data.mapping_name(k).c_str(), k == l_mapp_j)) {
              l_mapp_j = k;
            }
          }
          ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        // Register potential changes to constraint data
        if (l_mapp_j != e_vert.mapp_j[j]) {
          e_appl_data.touch({
            .name = "Change constraint mapping",
            .redo = [edit = l_mapp_j,         i = i, j = j](auto &data) { data.gamut_verts[i].mapp_j[j] = edit; },
            .undo = [edit = e_vert.mapp_j[j], i = i, j = j](auto &data) { data.gamut_verts[i].mapp_j[j] = edit; }
          });
        }

        ImGui::PopID();
      }
      ImGui::EndGroup();  ImGui::SameLine();

      // Value/error
      ImGui::BeginGroup(); ImGui::SetNextItemWidth(constr_colr_width); ImGui::Text("Value, err");
      for (uint j = 0; j < e_vert.colr_j.size(); ++j) {
        ImGui::PushID(j);

        Mapp mapp    = e_proj_data.mapping_data(e_vert.mapp_j[j]);
        Colr colr_j  = e_vert.colr_j[j];
        Colr rtrip_j = mapp.apply_color(e_spec);
        Colr error_j = (rtrip_j - colr_j).abs().eval();

        ImGui::ColorEdit3(fmt::format("##value{}", j).c_str(), 
          linear_srgb_to_gamma_srgb(colr_j).data(),  
          ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
        ImGui::SameLine();
        ImGui::ColorEdit3(fmt::format("##error{}", j).c_str(), 
          linear_srgb_to_gamma_srgb(error_j).data(), 
          ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);

        ImGui::PopID();
      }
      ImGui::EndGroup();  ImGui::SameLine();

      // Edit buttons
      ImGui::PushItemWidth(constr_edit_width); ImGui::BeginGroup(); ImGui::Text("");
      if (e_vert_slct.size() > 1) ImGui::BeginDisabled();
      for (uint j = 0; j < e_vert.colr_j.size(); ++j) {
        ImGui::PushID(j);
        if (ImGui::Button("Edit")) {
          i_cstr_slct  = j;
        }
        ImGui::PopID();
      }
      if (e_vert_slct.size() > 1) ImGui::EndDisabled();
      ImGui::EndGroup(); ImGui::PopItemWidth(); ImGui::SameLine();
    
      // Up/down buttons
      ImGui::BeginGroup(); ImGui::SetNextItemWidth(constr_arrw_width); ImGui::Text("");
      for (uint j = 0; j < e_vert.colr_j.size(); ++j) {
        ImGui::PushID(j);

        if (j == 0) ImGui::BeginDisabled();
        if (ImGui::ArrowButton("up", ImGuiDir_Up)) {
          e_appl_data.touch({ .name = "Swapped constraint mappings", .redo = [i = i, j = j](auto &data) {  
            std::swap(data.gamut_verts[i].colr_j[j], data.gamut_verts[i].colr_j[j - 1]);
            std::swap(data.gamut_verts[i].mapp_j[j], data.gamut_verts[i].mapp_j[j - 1]);
          },.undo = [i = i, j = j](auto &data) {  
            std::swap(data.gamut_verts[i].colr_j[j], data.gamut_verts[i].colr_j[j - 1]);
            std::swap(data.gamut_verts[i].mapp_j[j], data.gamut_verts[i].mapp_j[j - 1]);
          }});
        }
        if (j == 0) ImGui::EndDisabled();

        ImGui::SameLine();
        
        if (j == e_vert.colr_j.size() - 1) ImGui::BeginDisabled();
        if (ImGui::ArrowButton("down", ImGuiDir_Down)) {
          e_appl_data.touch({ .name = "Swapped constraint mappings", .redo = [i = i, j = j](auto &data) {  
            std::swap(data.gamut_verts[i].colr_j[j], data.gamut_verts[i].colr_j[j + 1]);
            std::swap(data.gamut_verts[i].mapp_j[j], data.gamut_verts[i].mapp_j[j + 1]);
          },.undo = [i = i, j = j](auto &data) {  
            std::swap(data.gamut_verts[i].colr_j[j], data.gamut_verts[i].colr_j[j + 1]);
            std::swap(data.gamut_verts[i].mapp_j[j], data.gamut_verts[i].mapp_j[j + 1]);
          }});
        }
        if (j == e_vert.colr_j.size() - 1) ImGui::EndDisabled();

        ImGui::PopID();
      }
      ImGui::EndGroup();  ImGui::SameLine();

      // Close button
      ImGui::BeginGroup(); ImGui::SetNextItemWidth(constr_clse_width); ImGui::Text("");
      for (uint j = 0; j < e_vert.colr_j.size(); ++j) {
        ImGui::PushID(j);

        if (ImGui::Button("X")) {
          e_appl_data.touch({ .name = "Delete color constraint", .redo = [i = i, j = j](auto &data) { 
            data.gamut_verts[i].colr_j.erase(data.gamut_verts[i].colr_j.begin() + j); 
            data.gamut_verts[i].mapp_j.erase(data.gamut_verts[i].mapp_j.begin() + j); 
          }, .undo = [edit = e_vert,  i = i, j = j](auto &data) { data.gamut_verts[i] = edit; }});

          // Sanitize selected constraint in case this was deleted
          i_cstr_slct = std::min(i_cstr_slct, 
                                  static_cast<int>(e_vert.colr_j.size() - 1));
        }

        ImGui::PopID();
      }
      ImGui::EndGroup();
    }

    // Spawn button to add an extra constraint, if necessary
    if (ImGui::Button("Add constraint")) {
      e_appl_data.touch({ .name = "Add color constraint", .redo = [i = i](auto &data) { 
        data.gamut_verts[i].colr_j.push_back(data.gamut_verts[i].colr_i); 
        data.gamut_verts[i].mapp_j.push_back(0); 
      }, .undo = [edit = e_vert,  i = i](auto &data) { data.gamut_verts[i] = edit; }});

      // Set displayed constraint in viewport to this constraint, iff a constraint was selected
      if (i_cstr_slct != -1)
        i_cstr_slct = e_vert.colr_j.size() - 1;
    }

    ImGui::PopID(); // i
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

    // Spawn constraint color editor
    Colr colr_edit = e_vert.colr_j[i_cstr_slct]; // Track a copy of the constraint color for editing
    ImGui::Separator();
    ImGui::ColorEdit3("##Constr", colr_edit.data(), ImGuiColorEditFlags_Float);

    if (ImGui::IsItemActive() && !m_is_cstr_edit_used) {
      m_colr_prev = e_vert.colr_j[i_cstr_slct];
      m_is_cstr_edit_used = true;
    }
    if (ImGui::IsItemActive()) {
      e_vert.colr_j[i_cstr_slct] = colr_edit;
    }
    if (!ImGui::IsItemActive() && m_is_cstr_edit_used) {
      m_is_cstr_edit_used = false;
      e_appl_data.touch({
        .name = "Change constraint color",
        .redo = [edit = colr_edit,
                 i = i,
                 j = i_cstr_slct](auto &data) { data.gamut_verts[i].colr_j[j] = edit; },
        .undo = [edit = m_colr_prev,
                 i = i,
                 j = i_cstr_slct](auto &data) { data.gamut_verts[i].colr_j[j] = edit; }
      });
    }

    // Add button to move gamut offset back to the metamer boundary's centroid
    ImGui::SameLine();
    if (ImGui::Button("Re-center")) {
      e_appl_data.touch({ 
        .name = "Center cosntraint color", 
        .redo = [edit = e_csol_cntr, 
                 i = i, 
                 j = i_cstr_slct](auto &data) { data.gamut_verts[i].colr_j[j] = edit; }, 
        .undo = [edit = e_vert.colr_j[i_cstr_slct],                 
                 i = i, 
                 j = i_cstr_slct](auto &data) { data.gamut_verts[i].colr_j[j] = edit; }
      });
    }
  }

  void ViewportOverlayTask::eval_overlay_plot(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_window    = info.get_resource<gl::Window>(global_key, "window");
    auto &e_appl_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data = e_appl_data.project_data;
    auto &i_cstr_slct = info.get_resource<int>("constr_selection");
    auto &e_vert_slct = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection");
    auto &e_vert        = e_appl_data.project_data.gamut_verts;
    auto &e_spec      = info.get_resource<std::vector<Spec>>("gen_spectral_gamut", "gamut_spec");

    const ImVec2 refl_size = { -1.f, overlay_plot_height * e_window.content_scale() };
    const auto   refl_flag = ImPlotFlags_NoInputs | ImPlotFlags_NoFrame;

    if (ImPlot::BeginPlot("##Reflectance", refl_size, refl_flag)) {
      // Get wavelength values for x-axis in plot
      Spec x_values;
      for (uint i = 0; i < x_values.size(); ++i)
        x_values[i] = wavelength_at_index(i);

      // Setup minimal format for coming line plots
      ImPlot::SetupLegend(ImPlotLocation_North, ImPlotLegendFlags_Horizontal | ImPlotLegendFlags_Outside);
      ImPlot::SetupAxes("Wavelength", "##Value", ImPlotAxisFlags_NoGridLines, ImPlotAxisFlags_NoDecorations);
      ImPlot::SetupAxesLimits(wavelength_min, wavelength_max, 0.0, 1.0, ImPlotCond_Always);

      // Plot each vertex
      for (uint i : e_vert_slct) {
        auto c = linear_srgb_to_gamma_srgb(e_vert[i].colr_i);
        ImPlot::SetNextLineStyle({ c.x(), c.y(), c.z(), 1.f });
        ImPlot::PlotLine(fmt::format("Vertex {}", i).c_str(), x_values.data(), e_spec[i].data(), wavelength_samples);
      }
      
      ImPlot::EndPlot();
    }
  }
} // namespace met