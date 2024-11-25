#include <metameric/scene/scene.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/editor/schedule.hpp>
#include <metameric/editor/detail/task_lambda.hpp>
#include <metameric/editor/task_window.hpp>
#include <metameric/editor/task_render_export.hpp>
#include <metameric/editor/task_settings_editor.hpp>
#include <metameric/editor/task_measure_tool.hpp>
#include <metameric/editor/detail/file_dialog.hpp>
#include <metameric/editor/detail/imgui.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/window.hpp>
#include <small_gl/detail/program_cache.hpp>
#include <string>

namespace met {
  /* Titles and ImGui IDs used to spawn modals */
  const static std::string close_modal_title  = "Close project";
  const static std::string exit_modal_title   = "Exit Metameric";
  
  namespace detail {
    // TODO handle new safe 
    void handle_new(SchedulerHandle &info) {
      met_trace_full();
      
      // Initialize new project
      info.global("scene").getw<Scene>().create();

      // Clear OpenGL state
      gl::Program::unbind_all();
      
      // Signal schedule re-creation and submit new schedule for main view
      submit_editor_schedule_auto(info);
    }

    bool handle_open(SchedulerHandle &info) {
      met_trace_full();
      
      // Open a file picker
      if (fs::path path; detail::load_dialog(path, { "*.json" })) {
        // Initialize existing project
        info.global("scene").getw<Scene>().load(path);

        // Clear OpenGL state
        gl::Program::unbind_all();

        // Signal schedule re-creation and submit new schedule for main view
        submit_editor_schedule_auto(info);
        
        return true;
      }
      return false;
    }

    bool handle_save_as(SchedulerHandle &info) {
      met_trace_full();
      if (fs::path path; detail::save_dialog(path, { "*.json" })) {
        info.global("scene").getw<Scene>().save(path);
        return true;
      }
      return false;
    }

    bool handle_save(SchedulerHandle &info) {
      met_trace_full();
      auto &e_scene = info.global("scene").getw<Scene>();
      if (e_scene.save_state == Scene::SaveState::eNew) {
        return handle_save_as(info);
      } else {
        e_scene.save(e_scene.save_path);
        return true;
      }
    }

    void handle_reload_schedule(SchedulerHandle &info) {
      met_trace_full();

      // Clear OpenGL state
      ImGui::CloseAnyPopupIfOpen();
      gl::Program::unbind_all();
      
      // Signal schedule re-creation and submit new schedule for main view
      submit_editor_schedule_auto(info);
    }

    void handle_close(SchedulerHandle &info) {
      met_trace_full();
      
      // Clear OpenGL state
      ImGui::CloseAnyPopupIfOpen();
      gl::Program::unbind_all();

      // Empty application data as project is closed
      info.global("scene").getw<Scene>().unload();
      
      // Signal schedule re-creation and submit empty schedule for main view
      submit_editor_schedule_auto(info);
    }

    void handle_exit(SchedulerHandle &info) {
      met_trace_full();
      
      ImGui::CloseAnyPopupIfOpen();

      info.global("scene").getw<Scene>().unload(); // Empty application data as project is closed
      info.global("window").getw<gl::Window>().set_should_close(); // Signal to window that it should close itself
      info.clear(); // Signal to scheduler that it should empty out
    }
  } // namespace detail

  void WindowTask::handle_close_safe(SchedulerHandle &info) {
    met_trace_full();
    
    // Get external resources
    const auto &e_scene = info.global("scene").getr<Scene>();

    // Continue to close function if scene state is ok; otherwise, present modal on next frame
    if (e_scene.save_state == Scene::SaveState::eUnsaved 
     || e_scene.save_state == Scene::SaveState::eNew) {
      m_open_close_modal = true;
    } else {
      handle_close(info);
    }
  }
  
  void WindowTask::handle_exit_safe(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_scene = info.global("scene").getr<Scene>();

    // Continue to exit function if scene state is ok; otherwise, present modal on next frame
    if (e_scene.save_state == Scene::SaveState::eUnsaved 
     || e_scene.save_state == Scene::SaveState::eNew) {
      m_open_exit_modal = true;
    } else {
      handle_exit(info);
    }
  }

  void WindowTask::eval(SchedulerHandle &info) {
    met_trace_full();
    
    // Get external resources
    const auto &e_scene = info.global("scene").getr<Scene>();

    // Query handler state that is used in several places
    bool is_loaded  = e_scene.save_state != Scene::SaveState::eUnloaded;
    bool is_savable = e_scene.save_state != Scene::SaveState::eSaved 
                   && e_scene.save_state != Scene::SaveState::eNew && is_loaded;
    
    // Modals/popups have to be on the same level of stack as OpenPopup(), so track this state
    // and call OpenPopup() at the end if true
    m_open_close_modal  = false;
    m_open_exit_modal   = false;

    // Set up the menu bar at the top of the window's viewport
    if (ImGui::BeginMainMenuBar()) {
      // File menu follows; new/open/close/import/exit
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New..."))                              { detail::handle_new(info);    }
        if (ImGui::MenuItem("Open..."))                             { detail::handle_open(info);   }
        if (ImGui::MenuItem("Close", nullptr, nullptr, is_loaded))  { handle_close_safe(info);     }

        // Debug method to force reload editor
        // if (ImGui::MenuItem("Reload schedule", nullptr, nullptr, is_loaded)) { detail::handle_reload_schedule(info); }

        ImGui::Separator(); 

        if (ImGui::MenuItem("Save", nullptr, nullptr, is_savable))      { detail::handle_save(info);    }
        if (ImGui::MenuItem("Save as...", nullptr, nullptr, is_loaded)) { detail::handle_save_as(info); }

        ImGui::Separator(); 

        if (ImGui::BeginMenu("Import", is_loaded)) {
          if (ImGui::MenuItem("Wavefront (.obj)")) {
            if (fs::path path; detail::load_dialog(path, { "*.obj" })) {
              auto &e_scene = info.global("scene").getw<Scene>();
              e_scene.import_obj(path);
            }
          }

          if (ImGui::MenuItem("Image (.exr, .png, .jpg, ...)")) {
            if (fs::path path; detail::load_dialog(path, { "*.exr", "*.png", "*.jpg", "*.jpeg", "*.bmp" })) {
              auto &e_scene = info.global("scene").getw<Scene>();
              e_scene.resources.images.emplace(path.filename().string(), {{ .path = path  }});
            }
          }
          
          if (ImGui::MenuItem("Illuminant spectrum")) {
            if (fs::path path; detail::load_dialog(path, { })) {
              auto &e_scene = info.global("scene").getw<Scene>();
              e_scene.resources.illuminants.emplace("New illuminant", io::load_spec(path));
            }
          }

          if (ImGui::MenuItem("Observer functions")) {
            if (fs::path path; detail::load_dialog(path, { })) {
              auto &e_scene = info.global("scene").getw<Scene>();
              e_scene.resources.observers.emplace("New observer", io::load_cmfs(path));
            }
          }

          if (ImGui::MenuItem("Basis functions")) {
            if (fs::path path; detail::load_dialog(path, { })) {
              auto &e_scene = info.global("scene").getw<Scene>();
              e_scene.resources.bases.emplace("New basis", io::load_basis(path));
            }
          }

          ImGui::EndMenu();
        }

        ImGui::Separator(); 

        if (ImGui::MenuItem("Exit")) {  handle_exit_safe(info); }

        ImGui::EndMenu();
      }

      // Edit menu follows; undo/redo
      if (ImGui::BeginMenu("Edit", is_loaded)) {
        const bool is_undo = e_scene.mod_i >= 0;
        const bool is_redo = e_scene.mod_i < (int(e_scene.mods.size()) - 1);
        
        std::string undo_msg = is_undo
          ? fmt::format("Undo ({})", e_scene.mods[e_scene.mod_i - 1].name) : "Undo";
        std::string redo_msg = is_redo
          ? fmt::format("Redo ({})", e_scene.mods[e_scene.mod_i + 1].name) : "Redo";
        
        if (ImGui::MenuItem(undo_msg.c_str(), nullptr, nullptr, is_undo)) { 
          info.global("scene").getw<Scene>().undo_mod(); 
        }
        if (ImGui::MenuItem(redo_msg.c_str(), nullptr, nullptr, is_redo)) { 
          info.global("scene").getw<Scene>().redo_mod(); 
        }

        ImGui::EndMenu();
      }
      
      // Add menu follows; create objects/emitters/upliftings/other components
      if (ImGui::BeginMenu("Add", is_loaded)) {
        if (ImGui::MenuItem("Emitter", nullptr, nullptr)) {
          using comp_type = typename detail::Component<Emitter>;
          info.global("scene").getw<Scene>().touch({
            .name = "Add emitter",
            .redo = [](auto &scene) {
              scene_data_by_type<comp_type>(scene).push("New emitter", { }); },
            .undo = [](auto &scene) {
              scene_data_by_type<comp_type>(scene).erase(scene_data_by_type<comp_type>(scene).size() - 1); }
          });
        }
        if (ImGui::MenuItem("Object", nullptr, nullptr)) {
          using comp_type = typename detail::Component<Object>;
          info.global("scene").getw<Scene>().touch({
            .name = "Add object",
            .redo = [](auto &scene) {
              scene_data_by_type<comp_type>(scene).push("New object", { }); },
            .undo = [](auto &scene) {
              scene_data_by_type<comp_type>(scene).erase(scene_data_by_type<comp_type>(scene).size() - 1); }
          });
        }
        if (ImGui::MenuItem("Uplifting", nullptr, nullptr)) {
          using comp_type = typename detail::Component<Uplifting>;
          info.global("scene").getw<Scene>().touch({
            .name = "Add uplifting",
            .redo = [](auto &scene) {
              scene_data_by_type<comp_type>(scene).push("New uplifting", { }); },
            .undo = [](auto &scene) {
              scene_data_by_type<comp_type>(scene).erase(scene_data_by_type<comp_type>(scene).size() - 1); }
          });
        }
        if (ImGui::MenuItem("View", nullptr, nullptr)) {
          using comp_type = typename detail::Component<View>;
          info.global("scene").getw<Scene>().touch({
            .name = "Add view",
            .redo = [](auto &scene) {
              scene_data_by_type<comp_type>(scene).push("New view", { }); },
            .undo = [](auto &scene) {
              scene_data_by_type<comp_type>(scene).erase(scene_data_by_type<comp_type>(scene).size() - 1); }
          });
        }

        ImGui::EndMenu();
      }

      // View menu follows; show export/settings/measure
      if (ImGui::BeginMenu("View", is_loaded)) {
        if (ImGui::MenuItem("Path measure tool", nullptr, nullptr)) {
          auto parent = info.task("viewport.viewport_input_editor").mask(info);
          if (auto handle = parent.child_task("path_measure_tool"); !handle.is_init()) {
            handle.init<PathMeasureToolTask>();
          }
        }

        if (ImGui::MenuItem("Render to file", nullptr, nullptr)) {
          if (auto handle = info.child_task("render_export"); !handle.is_init()) {
            handle.init<RenderExportTask>();
          }
        }
        
        ImGui::Separator();

        if (ImGui::MenuItem("Settings", nullptr, nullptr)) {
          if (auto handle = info.child_task("settings_editor"); !handle.is_init()) {
            handle.init<SettingsEditorTask>();
          }
        }

        ImGui::EndMenu();
      }

      ImGui::EndMainMenuBar();
    }
    
    // Create an explicit dock space over the entire window's viewport, excluding the menu bar
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    // Spawn close modal
    if (m_open_close_modal)  { 
      info.child_task("close_modal").init<detail::LambdaTask>([&](auto &info) {
        if (ImGui::BeginPopupModal(close_modal_title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
          ImGui::Text("Do you wish to close the project? You may lose unsaved progress.");
          ImGui::SpacedSeparator();
          if (ImGui::Button("Save and close") && handle_save(info)) { handle_close(info); } 
          ImGui::SameLine();
          if (ImGui::Button("Close without saving")) { handle_close(info); }
          ImGui::SameLine();
          if (ImGui::Button("Cancel")) { 
            info.task().dstr();
            ImGui::CloseCurrentPopup(); 
          }
          ImGui::EndPopup();
        }
      });
      ImGui::OpenPopup(close_modal_title.c_str()); 
    }

    // Spawm exit modal
    if (m_open_exit_modal)   { 
      info.child_task("exit_modal").init<detail::LambdaTask>([&](auto &info) {
        if (ImGui::BeginPopupModal(exit_modal_title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
          ImGui::Text("Do you wish to exit the program? You may lose unsaved progress.");
          ImGui::SpacedSeparator();
          if (ImGui::Button("Save and exit") && handle_save(info)) { handle_exit(info); } 
          ImGui::SameLine();
          if (ImGui::Button("Exit without saving")) { handle_exit(info); }
          ImGui::SameLine();
          if (ImGui::Button("Cancel")) { 
            info.task().dstr();
            ImGui::CloseCurrentPopup(); 
          }
          ImGui::EndPopup();
        }
      });
      ImGui::OpenPopup(exit_modal_title.c_str()); 
    }
  }
} // namespace met