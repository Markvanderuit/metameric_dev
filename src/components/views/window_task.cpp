#include <metameric/core/utility.hpp>
#include <metameric/components/schedule.hpp>
#include <metameric/components/views/window_task.hpp>
#include <metameric/components/views/new_project_view.hpp>
#include <metameric/components/views/save_exit_view.hpp>
#include <metameric/components/views/save_close_view.hpp>
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
    info.emplace_task_after<NewProjectView>(name(), "new_project_view", new_project_modal_title);
    info.emplace_task_after<SaveExitView>(name(), "save_exit_view", save_exit_modal_title);
    info.emplace_task_after<SaveCloseView>(name(), "save_close_view", save_close_modal_title);
  }

  void WindowTask::dstr(detail::TaskDstrInfo &info) {
    info.remove_task("new_project_view");
  }
  
  void WindowTask::handle_open(detail::TaskEvalInfo &info) {
    // Open a file picker
    std::filesystem::path path;
    if (detail::open_file_dialog(path, "json") == detail::FileDialogResultType::eOkay) {
      // Initialize existing project
      auto &app_data = info.get_resource<ApplicationData>("global", "application_data");
      app_data.load(path);

      // Signal schedule re-creation and submit new schedule for main view
      info.signal_flags = detail::TaskSignalFlags::eClearTasks;
      submit_schedule_main(info);
    }
  }

  void WindowTask::handle_save(detail::TaskEvalInfo &info) {
    auto &app_data = info.get_resource<ApplicationData>("global", "application_data");
    if (!app_data.project_path.empty()) {
      app_data.save(app_data.project_path);
    } else {
      handle_save_as(info);
    }
  }

  void WindowTask::handle_save_as(detail::TaskEvalInfo &info) {
    // Open a file picker
    std::filesystem::path path;
    if (detail::save_file_dialog(path, "json") == detail::FileDialogResultType::eOkay) {
      auto &app_data = info.get_resource<ApplicationData>("global", "application_data");
      app_data.save(path.replace_extension(".json"));
    }
  }

  void WindowTask::handle_close(detail::TaskEvalInfo &info) {
    // Empty application data as project is closed
    auto &app_data = info.get_resource<ApplicationData>("global", "application_data");
    app_data.clear();
    
    // Signal schedule re-creation and submit new schedule for main view
    info.signal_flags = detail::TaskSignalFlags::eClearTasks;
    submit_schedule_empty(info);
  }

  void WindowTask::eval(detail::TaskEvalInfo &info) {
    auto &app_data = info.get_resource<ApplicationData>("global", "application_data");
    
    // Modals/popups have to be on the same level of stack as OpenPopup(), so track state
    // and call OpenPopup() outside the window
    bool show_new_project_modal = false; 
    bool show_save_close_modal  = false; 
    bool show_save_exit_modal   = false; 
    
    // Set up the menu bar at the top of the window's viewport
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        const bool is_loaded = app_data.project_state != ProjectState::eUnloaded;
        
        /* Main section follows */

        if (ImGui::MenuItem("New..."))                             { show_new_project_modal = true; }
        if (ImGui::MenuItem("Open..."))                            { handle_open(info);             }
        if (ImGui::MenuItem("Close", nullptr, nullptr, is_loaded)) { show_save_close_modal = true;  }

        /* Save section follows */ ImGui::Separator(); 

        const bool enable_save    = is_loaded && !app_data.project_path.empty();
        if (ImGui::MenuItem("Save", nullptr, nullptr, enable_save))     { handle_save(info);    }
        if (ImGui::MenuItem("Save as...", nullptr, nullptr, is_loaded)) { handle_save_as(info); }

        /* Miscellaneous section follows */ ImGui::Separator(); 

        if (ImGui::MenuItem("Exit")) { 
          show_save_exit_modal = app_data.project_state == ProjectState::eUnsaved
                              || app_data.project_state == ProjectState::eModified;
          if (!show_save_exit_modal) {
            auto &window = info.get_resource<gl::Window>("global", "window");
            window.set_should_close();
          }
        }

        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", nullptr, nullptr, false)) {
          // ...
        }

        if (ImGui::MenuItem("Redo", nullptr, nullptr, false)) {
          // ...
        }

        ImGui::Separator();

        // ...
        
        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("View")) {
        // ...
        
        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Help")) {
        // ...
        
        ImGui::EndMenu();
      }

      ImGui::EndMainMenuBar();
    }
    
    // Create an explicit dock space over the entire window's viewport, excluding the menu bar
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    // Spawn requested modal views
    if (show_new_project_modal) { ImGui::OpenPopup(new_project_modal_title.c_str()); }
    if (show_save_close_modal)  { ImGui::OpenPopup(save_close_modal_title.c_str()); }
    if (show_save_exit_modal)   { ImGui::OpenPopup(save_exit_modal_title.c_str()); }
  }
} // namespace met