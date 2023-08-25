#include <metameric/core/scene_handler.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/schedule.hpp>
#include <metameric/components/misc/task_lambda.hpp>
#include <metameric/components/views/task_window.hpp>
#include <metameric/components/views/task_create_project.hpp>
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
      info.global("scene_handler").writeable<SceneHandler>().create();

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
        info.global("scene_handler").writeable<SceneHandler>().load(path);

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
        info.global("scene_handler").writeable<SceneHandler>().save(path);
        return true;
      }
      return false;
    }

    bool handle_save(SchedulerHandle &info) {
      met_trace_full();
      auto &e_handler = info.global("scene_handler").writeable<SceneHandler>();
      if (e_handler.save_state == SceneHandler::SaveState::eNew) {
        return handle_save_as(info);
      } else {
        e_handler.save(e_handler.save_path);
        return true;
      }
    }

    bool handle_export(SchedulerHandle &info) {
      met_trace_full();
      
      // TODO implement

      return true;
    }

    void handle_close(SchedulerHandle &info) {
      met_trace_full();
      
      // Clear OpenGL state
      ImGui::CloseAnyPopupIfOpen();
      gl::Program::unbind_all();

      // Empty application data as project is closed
      info.global("scene_handler").writeable<SceneHandler>().unload();
      
      // Signal schedule re-creation and submit empty schedule for main view
      submit_metameric_editor_schedule_unloaded(info);
    }

    void handle_exit(SchedulerHandle &info) {
      met_trace_full();
      
      ImGui::CloseAnyPopupIfOpen();

      info.global("scene_handler").writeable<SceneHandler>().unload();        // Empty application data as project is closed
      info.global("window").writeable<gl::Window>().set_should_close(); // Signal to window that it should close itself
      info.clear();                                                     // Signal to scheduler that it should empty out
    }
  } // namespace detail

  /* bool WindowTask::handle_export(SchedulerHandle &info) {
    met_trace_full();

    if (fs::path path; detail::save_dialog(path, "met")) {
      // Get shared resources
      const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
      const auto &e_proj_data = e_appl_data.project_data;
      const auto &e_spectra   = info("gen_spectral_data", "spectra").read_only<std::vector<Spec>>();
      const auto &e_weights   = info("gen_convex_weights", "bary_buffer").read_only<gl::Buffer>();

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
        const auto &e_delaunay = info("gen_convex_weights", "delaunay").read_only<AlDelaunayData>();

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
    const auto &e_handler = info.global("scene_handler").read_only<SceneHandler>();

    // Continue to close function if scene state is ok; otherwise, present modal on next frame
    if (e_handler.save_state == SceneHandler::SaveState::eUnsaved 
     || e_handler.save_state == SceneHandler::SaveState::eNew) {
      m_open_close_modal = true;
    } else {
      handle_close(info);
    }
  }
  
  void WindowTask::handle_exit_safe(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_handler = info.global("scene_handler").read_only<SceneHandler>();

    // Continue to exit function if scene state is ok; otherwise, present modal on next frame
    if (e_handler.save_state == SceneHandler::SaveState::eUnsaved 
     || e_handler.save_state == SceneHandler::SaveState::eNew) {
      m_open_exit_modal = true;
    } else {
      handle_exit(info);
    }
  }

  void WindowTask::eval(SchedulerHandle &info) {
    met_trace_full();
    
    // Get external resources
    const auto &e_handler = info.global("scene_handler").read_only<SceneHandler>();

    // Modals/popups have to be on the same level of stack as OpenPopup(), so track this state
    // and call OpenPopup() at the end if true
    m_open_close_modal  = false;
    m_open_exit_modal   = false;
    m_open_create_modal = false;

    // Set up the menu bar at the top of the window's viewport
    if (ImGui::BeginMainMenuBar()) {
      /* File menu follows */
      
      if (ImGui::BeginMenu("File")) {
        const bool is_loaded  = e_handler.save_state != SceneHandler::SaveState::eUnloaded;
        const bool is_savable = e_handler.save_state != SceneHandler::SaveState::eSaved 
                             && e_handler.save_state != SceneHandler::SaveState::eNew && is_loaded;
        
        /* Main section follows */

        if (ImGui::MenuItem("New..."))                             { detail::handle_new(info); }
        // if (ImGui::MenuItem("New..."))                             { m_open_create_modal = true; }
        if (ImGui::MenuItem("Open..."))                            { detail::handle_open(info);  }
        if (ImGui::MenuItem("Close", nullptr, nullptr, is_loaded)) { handle_close_safe(info);    }

        ImGui::Separator(); 

        /* Save section follows */

        if (ImGui::MenuItem("Save", nullptr, nullptr, is_savable))      { detail::handle_save(info);    }
        if (ImGui::MenuItem("Save as...", nullptr, nullptr, is_loaded)) { detail::handle_save_as(info); }

        ImGui::Separator(); 

        if (ImGui::MenuItem("Export", nullptr, nullptr, is_loaded)) { detail::handle_export(info); }

        ImGui::Separator(); 

        /* Miscellaneous section follows */

        if (ImGui::MenuItem("Exit")) {  handle_exit_safe(info); }

        ImGui::EndMenu();
      }

      /* Edit menu follows */

      if (ImGui::BeginMenu("Edit")) {
        auto &e_handler = info.global("scene_handler").writeable<SceneHandler>();
        const bool is_undo = e_handler.mod_i >= 0;
        const bool is_redo = e_handler.mod_i < int(e_handler.mods.size()) - 1;
        if (ImGui::MenuItem("Undo", nullptr, nullptr, is_undo)) { e_handler.undo_mod(); }
        if (ImGui::MenuItem("Redo", nullptr, nullptr, is_redo)) { e_handler.redo_mod(); }
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