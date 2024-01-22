#include <metameric/core/scene.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/schedule.hpp>
#include <metameric/components/misc/task_lambda.hpp>
#include <metameric/components/views/task_window.hpp>
#include <metameric/components/views/task_create_project.hpp>
#include <metameric/components/views/task_settings_editor.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/window.hpp>
#include <string>

namespace met {
  /* Titles and ImGui IDs used to spawn modals */
  const static std::string create_modal_title = "New project";
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
      submit_metameric_editor_schedule_loaded(info);
    }

    bool handle_open(SchedulerHandle &info) {
      met_trace_full();
      
      // Open a file picker
      if (fs::path path; detail::load_dialog(path, "json")) {
        // Initialize existing project
        info.global("scene").getw<Scene>().load(path);

        // Clear OpenGL state
        gl::Program::unbind_all();

        // Signal schedule re-creation and submit new schedule for main view
        submit_metameric_editor_schedule_loaded(info);
        
        return true;
      }
      return false;
    }

    bool handle_save_as(SchedulerHandle &info) {
      met_trace_full();
      if (fs::path path; detail::save_dialog(path, "json")) {
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

    bool handle_export(SchedulerHandle &info) {
      met_trace_full();
      
      // TODO implement

      return true;
    }

    void handle_reload(SchedulerHandle &info) {
      met_trace_full();

      // Clear OpenGL state
      ImGui::CloseAnyPopupIfOpen();
      gl::Program::unbind_all();
      
      // Signal schedule re-creation and submit new schedule for main view
      submit_metameric_editor_schedule_loaded(info);
    }

    void handle_close(SchedulerHandle &info) {
      met_trace_full();
      
      // Clear OpenGL state
      ImGui::CloseAnyPopupIfOpen();
      gl::Program::unbind_all();

      // Empty application data as project is closed
      info.global("scene").getw<Scene>().unload();
      
      // Signal schedule re-creation and submit empty schedule for main view
      submit_metameric_editor_schedule_unloaded(info);
    }

    void handle_exit(SchedulerHandle &info) {
      met_trace_full();
      
      ImGui::CloseAnyPopupIfOpen();

      info.global("scene").getw<Scene>().unload();        // Empty application data as project is closed
      info.global("window").getw<gl::Window>().set_should_close(); // Signal to window that it should close itself
      info.clear();                                                     // Signal to scheduler that it should empty out
    }
  } // namespace detail

  /* bool WindowTask::handle_export(SchedulerHandle &info) {
    met_trace_full();

    if (fs::path path; detail::save_dialog(path, "met")) {
      // Get shared resources
      const auto &e_appl_data = info.global("appl_data").getr<ApplicationData>();
      const auto &e_proj_data = e_appl_data.project_data;
      const auto &e_spectra   = info("gen_spectral_data", "spectra").getr<std::vector<Spec>>();
      const auto &e_weights   = info("gen_convex_weights", "bary_buffer").getr<gl::Buffer>();

      // Insert barriers for the following operations
      gl::sync::memory_barrier(gl::BarrierFlags::eBufferUpdate | gl::BarrierFlags::eStorageBuffer | gl::BarrierFlags::eClientMappedBuffer);

      if (e_proj_data.meshing_type == ProjectMeshingType::eConvexHull) {
        // Obtain barycentric data from buffer
        std::vector<Bary> bary_data(e_weights.size() / sizeof(Bary));
        e_weights.get_as<Bary>(bary_data);

        // Save data to specified filepath
        io::save_spectral_data({
          .bary_xres = e_appl_data.loaded_texture.size()[0],
          .bary_yres = e_appl_data.loaded_texture.size()[1],
          .bary_zres = static_cast<uint>(e_spectra.size()),
          .functions = cnt_span<const float>(e_spectra),
          .weights   = cnt_span<const float>(bary_data)
        }, io::path_with_ext(path, ".met"));
      } else if (e_proj_data.meshing_type == ProjectMeshingType::eDelaunay) {
        const auto &e_delaunay = info("gen_convex_weights", "delaunay").getr<AlDelaunay>();

        // Obtain barycentric data from buffer
        std::vector<eig::Array4f> bary_data(e_weights.size() / sizeof(eig::Array4f));
        e_weights.get_as<eig::Array4f>(bary_data);

        // Pack interleaved spectral data 
        std::vector<eig::Array4f> spec_data(wavelength_samples * e_delaunay.elems.size());
        for (uint i = 0; i < e_delaunay.elems.size(); ++i) {
          const auto &el = e_delaunay.elems[i];

          // Gather the four relevant spectra for this element
          std::array<Spec, 4> el_spectra;
          std::ranges::transform(el, el_spectra.begin(), [&](uint i) { return e_spectra[i]; });

          // Interleave values and scatter into data so four values are accessed in one query
          for (uint j = 0; j < wavelength_samples; ++j) {
            spec_data[i * wavelength_samples + j] = eig::Array4f {
              el_spectra[0][j], el_spectra[1][j], el_spectra[2][j], el_spectra[3][j], 
            };
          }
        }
        
        // Save data to specified filepath
        io::save_spectral_data({
          .bary_xres = e_appl_data.loaded_texture.size()[0],
          .bary_yres = e_appl_data.loaded_texture.size()[1],
          .bary_zres = static_cast<uint>(e_delaunay.elems.size()),
          .functions = cnt_span<float>(spec_data),
          .weights   = cnt_span<float>(bary_data)
        }, io::path_with_ext(path, ".met"));
      }

      return true;
    }

    return false;
  } */
  
  /* void WindowTask::handle_new_safe(SchedulerHandle &info) {
    met_trace_full();


  } */


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
    m_open_create_modal = false;

    // Set up the menu bar at the top of the window's viewport
    if (ImGui::BeginMainMenuBar()) {
      /* File menu follows */
      
      if (ImGui::BeginMenu("File")) {
        /* Main section follows */

        if (ImGui::MenuItem("New..."))                              { detail::handle_new(info);    }
        // if (ImGui::MenuItem("New..."))                              { m_open_create_modal = true; }
        if (ImGui::MenuItem("Open..."))                             { detail::handle_open(info);   }
        if (ImGui::MenuItem("Reload", nullptr, nullptr, is_loaded)) { detail::handle_reload(info); }
        if (ImGui::MenuItem("Close", nullptr, nullptr, is_loaded))  { handle_close_safe(info);     }

        ImGui::Separator(); 

        /* Save section follows */

        if (ImGui::MenuItem("Save", nullptr, nullptr, is_savable))      { detail::handle_save(info);    }
        if (ImGui::MenuItem("Save as...", nullptr, nullptr, is_loaded)) { detail::handle_save_as(info); }

        ImGui::Separator(); 

        if (ImGui::BeginMenu("Import", is_loaded)) {
          if (ImGui::MenuItem("Wavefront (.obj)")) {
            if (fs::path path; detail::load_dialog(path, "obj")) {
              auto &e_scene = info.global("scene").getw<Scene>();
              e_scene.import_wavefront_obj(path);
            }
          }

          if (ImGui::MenuItem("Image (.exr, .png, .jpg, ...)")) {
            if (fs::path path; detail::load_dialog(path, "exr,png,jpg,jpeg,bmp")) {
              auto &e_scene = info.global("scene").getw<Scene>();
              e_scene.resources.images.emplace(path.filename().string(), {{ .path = path  }});
            }
          }
          
          if (ImGui::MenuItem("Spectral functions")) {
            
          }

          if (ImGui::MenuItem("Observer functions")) {
            
          }

          if (ImGui::MenuItem("Basis functions")) {
            
          }

          ImGui::EndMenu();
        }


        if (ImGui::MenuItem("Export", nullptr, nullptr, is_loaded)) { detail::handle_export(info); }

        ImGui::Separator(); 

        /* Miscellaneous section follows */

        if (ImGui::MenuItem("Exit")) {  handle_exit_safe(info); }

        ImGui::EndMenu();
      }

      /* Edit menu follows */

      if (ImGui::BeginMenu("Edit", is_loaded)) {
        const bool is_undo = e_scene.mod_i >= 0;
        const bool is_redo = e_scene.mod_i < int(e_scene.mods.size()) - 1;

        if (ImGui::MenuItem("Undo", nullptr, nullptr, is_undo)) { 
          info.global("scene").getw<Scene>().undo_mod(); 
        }
        if (ImGui::MenuItem("Redo", nullptr, nullptr, is_redo)) { 
          info.global("scene").getw<Scene>().redo_mod(); 
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
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    // Spawn create modal
    if (m_open_create_modal) { 
      info.child_task("create_modal").init<CreateProjectTask>(create_modal_title);
      ImGui::OpenPopup(create_modal_title.c_str()); 
    }

    // Spawn close modal
    if (m_open_close_modal)  { 
      info.child_task("close_modal").init<LambdaTask>([&](auto &info) {
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
      info.child_task("exit_modal").init<LambdaTask>([&](auto &info) {
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