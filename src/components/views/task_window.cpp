#include <metameric/core/utility.hpp>
#include <metameric/components/schedule.hpp>
#include <metameric/components/misc/task_lambda.hpp>
#include <metameric/components/views/task_window.hpp>
#include <metameric/components/views/task_create_project.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/window.hpp>

namespace met {
  /* Titles and ImGui IDs used to spawn modals */
  const static std::string create_modal_title = "Create new project";
  const static std::string close_modal_title  = "Close project";
  const static std::string exit_modal_title   = "Exit Metameric";

  /* Task IDs used to spawn modals' tasks */
  const static std::string create_modal_name = "_create_modal";
  const static std::string close_modal_name  = "_close_modal";
  const static std::string exit_modal_name   = "_exit_modal";

  WindowTask::WindowTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void WindowTask::init(detail::TaskInitInfo &info) {
    info.emplace_task_after<CreateProjectTask>(name(), name() + create_modal_name, create_modal_title);

    // Modal subtask to handle safe exiting of program
    info.emplace_task_after<LambdaTask>(name(), name() + close_modal_name, [&](auto &info) {
      if (ImGui::BeginPopupModal(exit_modal_title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Do you wish to exit the program? You may lose unsaved progress.");
        ImGui::SpacedSeparator();
        if (ImGui::Button("Save and exit") && handle_save(info)) { handle_exit(info); } 
        ImGui::SameLine();
        if (ImGui::Button("Exit without saving")) { handle_exit(info); }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
      }
    });

    // Modal subtask to handle safe closing of project
    info.emplace_task_after<LambdaTask>(name(), name() + exit_modal_name, [&](auto &info) {
      if (ImGui::BeginPopupModal(close_modal_title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Do you wish to close the project? You may lose unsaved progress.");
        ImGui::SpacedSeparator();
        if (ImGui::Button("Save and close") && handle_save(info)) { handle_close(info); } 
        ImGui::SameLine();
        if (ImGui::Button("Close without saving")) { handle_close(info); }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
      }
    });
  }

  void WindowTask::dstr(detail::TaskDstrInfo &info) {
    // Remove modal subtasks
    info.remove_task(name() + create_modal_name);
    info.remove_task(name() + close_modal_name);
    info.remove_task(name() + exit_modal_name);
  }
  
  bool WindowTask::handle_open(detail::TaskEvalInfo &info) {
    // Open a file picker
    if (fs::path path; detail::load_dialog(path, "json")) {
      // Initialize existing project
      info.get_resource<ApplicationData>(global_key, "app_data").load(path);

      // Signal schedule re-creation and submit new schedule for main view
      info.signal_flags = detail::TaskSignalFlags::eClearTasks;
      submit_schedule_main(info);

      return true;
    }
    return false;
  }

  bool WindowTask::handle_save(detail::TaskEvalInfo &info) {
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    if (e_app_data.project_state == ProjectState::eNew) {
      return handle_save_as(info);
    } else {
      e_app_data.save(e_app_data.project_path);
      return true;
    }
  }

  bool WindowTask::handle_save_as(detail::TaskEvalInfo &info) {
    if (fs::path path; detail::save_dialog(path, "json")) {
      info.get_resource<ApplicationData>(global_key, "app_data").save(io::path_with_ext(path, ".json"));
      return true;
    }
    return false;
  }
  
  void WindowTask::handle_close_safe(detail::TaskEvalInfo &info) {
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    if (e_app_data.project_state == ProjectState::eUnsaved 
     || e_app_data.project_state == ProjectState::eNew) {
      m_open_close_modal = true;
    } else {
      handle_close(info);
    }
  }

  void WindowTask::handle_close(detail::TaskEvalInfo &info) {
    ImGui::CloseAnyPopupIfOpen();

    // Empty application data as project is closed
    info.get_resource<ApplicationData>(global_key, "app_data").unload();
    
    // Signal schedule re-creation and submit empty schedule for main view
    info.signal_flags = detail::TaskSignalFlags::eClearTasks;
    submit_schedule_empty(info);
  }
  
  void WindowTask::handle_exit_safe(detail::TaskEvalInfo &info) {
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    if (e_app_data.project_state == ProjectState::eUnsaved 
     || e_app_data.project_state == ProjectState::eNew) {
      m_open_exit_modal = true;
    } else {
      handle_exit(info);
    }
  }

  void WindowTask::handle_exit(detail::TaskEvalInfo &info) {
    ImGui::CloseAnyPopupIfOpen();
    
    // Signal to window that it should close itself
    info.get_resource<gl::Window>(global_key, "window").set_should_close();
  }

  void WindowTask::eval(detail::TaskEvalInfo &info) {
    // Modals/popups have to be on the same level of stack as OpenPopup(), so track this state
    // and call OpenPopup() at the end if true
    m_open_close_modal  = false;
    m_open_exit_modal   = false;
    m_open_create_modal = false;

    // Set up the menu bar at the top of the window's viewport
    if (ImGui::BeginMainMenuBar()) {
      /* File menu follows */
      
      if (ImGui::BeginMenu("File")) {
        auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
        const bool is_loaded   = e_app_data.project_state != ProjectState::eUnloaded;
        const bool enable_save = e_app_data.project_state != ProjectState::eSaved 
          && e_app_data.project_state != ProjectState::eNew && is_loaded;
        
        /* Main section follows */

        if (ImGui::MenuItem("New..."))                             { m_open_create_modal = true; }
        if (ImGui::MenuItem("Open..."))                            { handle_open(info);               }
        if (ImGui::MenuItem("Close", nullptr, nullptr, is_loaded)) { handle_close_safe(info);         }

        ImGui::Separator(); 

        /* Save section follows */

        if (ImGui::MenuItem("Save", nullptr, nullptr, enable_save))     { handle_save(info);    }
        if (ImGui::MenuItem("Save as...", nullptr, nullptr, is_loaded)) { handle_save_as(info); }

        ImGui::Separator(); 

        /* Miscellaneous section follows */

        if (ImGui::MenuItem("Exit")) {  handle_exit_safe(info); }

        ImGui::EndMenu();
      }

      /* Edit menu follows */

      if (ImGui::BeginMenu("Edit")) {
        auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
        const bool is_undo = e_app_data.mod_i >= 0;
        const bool is_redo = e_app_data.mod_i < int(e_app_data.mods.size()) - 1;
        if (ImGui::MenuItem("Undo", nullptr, nullptr, is_undo)) { e_app_data.undo(); }
        if (ImGui::MenuItem("Redo", nullptr, nullptr, is_redo)) { e_app_data.redo(); }
        ImGui::EndMenu();
      }

      ImGui::EndMainMenuBar();
    }
    
    // Create an explicit dock space over the entire window's viewport, excluding the menu bar
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    // Spawn requested modal views
    if (m_open_create_modal) { ImGui::OpenPopup(create_modal_title.c_str()); }
    if (m_open_close_modal)  { ImGui::OpenPopup(close_modal_title.c_str()); }
    if (m_open_exit_modal)   { ImGui::OpenPopup(exit_modal_title.c_str()); }
  }
} // namespace met