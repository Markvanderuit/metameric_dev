#include <metameric/core/utility.hpp>
#include <metameric/components/schedule.hpp>
#include <metameric/components/misc/lambda_task.hpp>
#include <metameric/components/views/window_task.hpp>
#include <metameric/components/views/new_project_view.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/window.hpp>

namespace met {
  const static std::string new_project_modal_title = "Create new project";
  const static std::string save_close_modal_title  = "Close project";
  const static std::string save_exit_modal_title   = "Exit Metameric";

  WindowTask::WindowTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void WindowTask::init(detail::TaskInitInfo &info) {
    info.emplace_task_after<NewProjectView>(name(), name() + "_new_project_view", new_project_modal_title);

    // Modal subtask to handle safe exiting of program
    info.emplace_task_after<LambdaTask>(name(), name() + "_exit_view", [&](auto &info) {
      if (ImGui::BeginPopupModal(save_exit_modal_title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Do you wish to exit the program? You may lose unsaved progress.");
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        if (ImGui::Button("Save and exit")) {
          handle_save(info);
          handle_exit(info);
          ImGui::CloseCurrentPopup();
        } 
        ImGui::SameLine();
        if (ImGui::Button("Exit without saving")) {
          handle_exit(info);
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
      }
    });

    // Modal subtask to handle safe closing of project
    info.emplace_task_after<LambdaTask>(name(), name() + "_close_view", [&](auto &info) {
      if (ImGui::BeginPopupModal(save_close_modal_title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Do you wish to close the project? You may lose unsaved progress.");
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        if (ImGui::Button("Save and close")) {
          handle_save(info);
          handle_close(info);
          ImGui::CloseCurrentPopup();
        } 
        ImGui::SameLine();
        if (ImGui::Button("Close without saving")) {
          handle_close(info);
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
      }
    });
  }

  void WindowTask::dstr(detail::TaskDstrInfo &info) {
    // Remove modal subtasks
    info.remove_task(name() + "_new_project_view");
    info.remove_task(name() + "_close_view");
    info.remove_task(name() + "_exit_view");
  }
  
  void WindowTask::handle_open(detail::TaskEvalInfo &info) {
    // Open a file picker
    if (fs::path path; detail::load_dialog(path, "json")) {
      // Initialize existing project
      info.get_resource<ApplicationData>(global_key, "app_data").load(path);

      // Signal schedule re-creation and submit new schedule for main view
      info.signal_flags = detail::TaskSignalFlags::eClearTasks;
      submit_schedule_main(info);
    }
  }

  void WindowTask::handle_save(detail::TaskEvalInfo &info) {
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    if (!e_app_data.project_path.empty()) {
      e_app_data.save(e_app_data.project_path);
    } else {
      handle_save_as(info);
    }
  }

  void WindowTask::handle_save_as(detail::TaskEvalInfo &info) {
    if (fs::path path; detail::save_dialog(path, "json")) {
      info.get_resource<ApplicationData>(global_key, "app_data").save(io::path_with_ext(path, ".json"));
    }
  }
  
  void WindowTask::handle_close_safe(detail::TaskEvalInfo &info) {
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    if (e_app_data.project_state == ProjectState::eModified || e_app_data.project_state == ProjectState::eUnsaved) {
      m_open_save_close_modal = true;
    } else {
      handle_close(info);
    }
  }

  void WindowTask::handle_close(detail::TaskEvalInfo &info) {
    // Empty application data as project is closed
    info.get_resource<ApplicationData>(global_key, "app_data").clear();
    
    // Signal schedule re-creation and submit empty schedule for main view
    info.signal_flags = detail::TaskSignalFlags::eClearTasks;
    submit_schedule_empty(info);
  }
  
  void WindowTask::handle_exit_safe(detail::TaskEvalInfo &info) {
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    if (e_app_data.project_state == ProjectState::eModified || e_app_data.project_state == ProjectState::eUnsaved) {
      m_open_save_exit_modal = true;
    } else {
      handle_exit(info);
    }
  }

  void WindowTask::handle_exit(detail::TaskEvalInfo &info) {
    // Signal to window that it should close itself
    info.get_resource<gl::Window>(global_key, "window").set_should_close();
  }

  void WindowTask::eval(detail::TaskEvalInfo &info) {
    // Modals/popups have to be on the same level of stack as OpenPopup(), so track this state
    // and call OpenPopup() at the end if true
    m_open_save_close_modal  = false;
    m_open_save_exit_modal   = false;
    m_open_new_project_modal = false;

    // Set up the menu bar at the top of the window's viewport
    if (ImGui::BeginMainMenuBar()) {
      /* File menu follows */
      
      if (ImGui::BeginMenu("File")) {
        auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
        const bool is_loaded = e_app_data.project_state != ProjectState::eUnloaded;
        const bool is_saveable = is_loaded && !e_app_data.project_path.empty();
        
        /* Main section follows */

        if (ImGui::MenuItem("New..."))                             { m_open_new_project_modal = true; }
        if (ImGui::MenuItem("Open..."))                            { handle_open(info);               }
        if (ImGui::MenuItem("Close", nullptr, nullptr, is_loaded)) { handle_close_safe(info);         }

        /* Save section follows */ ImGui::Separator(); 

        if (ImGui::MenuItem("Save", nullptr, nullptr, is_saveable))     { handle_save(info);    }
        if (ImGui::MenuItem("Save as...", nullptr, nullptr, is_loaded)) { handle_save_as(info); }

        /* Miscellaneous section follows */ ImGui::Separator(); 

        if (ImGui::MenuItem("Exit")) {  handle_exit_safe(info); }

        ImGui::EndMenu();
      }

      /* Edit menu follows */

      if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", nullptr, nullptr, false)) {
          // ...
        }

        if (ImGui::MenuItem("Redo", nullptr, nullptr, false)) {
          // ...
        }
        ImGui::EndMenu();
      }

      ImGui::EndMainMenuBar();
    }
    
    // Create an explicit dock space over the entire window's viewport, excluding the menu bar
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    // Spawn requested modal views
    if (m_open_new_project_modal) { ImGui::OpenPopup(new_project_modal_title.c_str()); }
    if (m_open_save_close_modal)  { ImGui::OpenPopup(save_close_modal_title.c_str()); }
    if (m_open_save_exit_modal)   { ImGui::OpenPopup(save_exit_modal_title.c_str()); }
  }
} // namespace met