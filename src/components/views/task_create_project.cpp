#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/task_create_project.hpp>
#include <small_gl/window.hpp>

namespace met {
  constexpr float img_rel_width = 128.f;
  
  CreateProjectTask::CreateProjectTask(const std::string &name, const std::string &view_title)
  : detail::AbstractTask(name),
    m_view_title(view_title) { }

  void CreateProjectTask::init(detail::TaskInitInfo &info) {
    met_trace_full();
    m_proj_data = { };
  }

  void CreateProjectTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();
    m_proj_data = { };
  }

  void CreateProjectTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_window = info.get_resource<gl::Window>(global_key, "window");
    
    if (ImGui::BeginPopupModal(m_view_title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      // Image selector
      if (std::vector<fs::path> paths; ImGui::Button("Load images...") && detail::load_dialog_mult(paths)) {
        for (const fs::path &path : paths) {
          // Load image without gamma correction applied. Copy this image to gpu for direct display,
          auto host_image   = io::load_texture2d<Colr>(path);
          auto device_image = gl::Texture2d3f {{ .size = host_image.size(),
                                                 .data = cast_span<const float>(host_image.data()) }};

          // Then apply gamma correction after for rest of program pipeline
          io::to_lrgb(host_image);

          // Push on list of input 
          m_proj_data.images.push_back({ .image = std::move(host_image),
                                         .cmfs = 0,
                                         .illuminant = 0 });
          m_imag_data.push_back({ path.filename().replace_extension().string(), std::move(device_image) });
        }
      }
      
      ImGui::SameLine();
      if (std::vector<fs::path> paths; ImGui::Button("Load illuminants...") && detail::load_dialog_mult(paths)) {
        // ...
      }
      
      ImGui::SameLine();
      if (std::vector<fs::path> paths; ImGui::Button("Load cmfs...") && detail::load_dialog_mult(paths)) {
        // ...
      }

      ImGui::SpacedSeparator();
      
      const float child_height = (img_rel_width + 80.f) * e_window.content_scale();
      if (ImGui::BeginChild("Added images", { 2 * child_height, child_height }, false, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::AlignTextToFramePadding();

        // Track id of image to erase after full draw
        int erased_image = -1;

        for (uint i = 0; i < m_proj_data.images.size(); ++i) {
          const auto &[name, img] = m_imag_data[i];
          auto &img_data = m_proj_data.images[i];

          const float img_width  = img_rel_width * e_window.content_scale();
          const float img_height = img_width * (img.size().x() / img.size().y());

          // Begin wrapper group around image and related content
          if (ImGui::BeginChild(fmt::format("image_{}", i).c_str(), { img_width, 0 }, false)) {
            // Image title
            ImGui::Text(name.c_str());

            // Image delete button at end of line
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16.f * e_window.content_scale());
            if (ImGui::SmallButton("X")) {
              erased_image = i;
            }

            // Plot image in full pre-calculated width/height
            ImGui::Image(ImGui::to_ptr(img.object()), { img_width, img_height });
            
            // Selector for color matching functions
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::BeginCombo("##cmfs_selector", m_proj_data.cmfs[img_data.cmfs].first.c_str())) {
              for (uint j = 0; j < m_proj_data.cmfs.size(); ++j) {
                if (ImGui::Selectable(m_proj_data.cmfs[j].first.c_str(), j == img_data.cmfs)) {
                  img_data.cmfs = j;
                }
              }
              ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            
            // Selector for illuminant SPD
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::BeginCombo("##illuminant_selector", m_proj_data.illuminants[img_data.illuminant].first.c_str())) {
              for (uint j = 0; j < m_proj_data.illuminants.size(); ++j) {
                if (ImGui::Selectable(m_proj_data.illuminants[j].first.c_str(), j == img_data.illuminant)) {
                  img_data.illuminant = j;
                }
              }
              ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

          } // End wrapper group around image
          ImGui::EndChild();

          // ImGui::PopID();
          // ImGui::EndGroup();

          if (i < m_imag_data.size() - 1)
            ImGui::SameLine();
        }

        if (erased_image != -1) {
          m_imag_data.erase(m_imag_data.begin() + erased_image);
          m_proj_data.images.erase(m_proj_data.images.begin() + erased_image);
        }
      }
      ImGui::EndChild();

      ImGui::SpacedSeparator();

      // Define create/cancel buttons to handle results 
      if (m_proj_data.images.empty()) ImGui::BeginDisabled();
      if (ImGui::Button("Create") && create_project_safe(info)) { 
        ImGui::CloseAnyPopupIfOpen();
        info.remove_task(name());
      }
      if (m_proj_data.images.empty()) ImGui::EndDisabled();
      ImGui::SameLine();      
      if (ImGui::Button("Cancel")) { 
        ImGui::CloseCurrentPopup();
        info.remove_task(name());
      }

      // Insert modals
      insert_progress_warning(info);

      ImGui::EndPopup();
    }
  }
  
  void CreateProjectTask::insert_progress_warning(detail::TaskEvalInfo &info) {
    if (ImGui::BeginPopupModal("Warning: unsaved progress")) {
      ImGui::Text("If you continue, you may lose unsaved progress.");
      ImGui::SpacedSeparator();
      if (ImGui::Button("Continue")) {
        create_project(info);
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }
    }
  }

  bool CreateProjectTask::create_project_safe(detail::TaskEvalInfo &info) {
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    if (e_app_data.project_save == SaveFlag::eUnsaved || e_app_data.project_save == SaveFlag::eNew) {
      ImGui::OpenPopup("Warning: unsaved progress", 0);
      return false;
    /* } else if (!fs::exists(m_input_path)) {
      ImGui::OpenPopup("Warning: file not found", 0);
      return false; */
    } else {
      return create_project(info);
    }
  }

  bool CreateProjectTask::create_project(detail::TaskEvalInfo &info) {
    // Create a new project
    info.get_resource<ApplicationData>(global_key, "app_data").create(std::move(m_proj_data));

    // Signal schedule re-creation and submit new task schedule
    info.signal_flags = detail::TaskSignalFlags::eClearTasks;
    submit_schedule_main(info);

    return true;
  }
} // namespace met