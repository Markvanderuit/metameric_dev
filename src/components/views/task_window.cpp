#include <metameric/core/io.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/schedule.hpp>
#include <metameric/components/misc/task_lambda.hpp>
#include <metameric/components/views/task_window.hpp>
#include <metameric/components/views/task_create_project.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/window.hpp>
#include <ranges>

namespace met {
  /* Titles and ImGui IDs used to spawn modals */
  const static std::string create_modal_title = "Create new project";
  const static std::string close_modal_title  = "Close project";
  const static std::string exit_modal_title   = "Exit Metameric";

  /* Task IDs used to spawn modals' tasks */
  const static std::string create_modal_name = "create_modal";
  const static std::string close_modal_name  = "close_modal";
  const static std::string exit_modal_name   = "exit_modal";
  
  bool WindowTask::handle_open(SchedulerHandle &info) {
    met_trace_full();
    
    // Open a file picker
    if (fs::path path; detail::load_dialog(path, "json")) {
      // Initialize existing project
      info.global("appl_data").writeable<ApplicationData>().load(path);

      // Clear OpenGL state
      gl::Program::unbind_all();

      // Signal schedule re-creation and submit new schedule for main view
      submit_schedule_main(info);
      
      return true;
    }
    return false;
  }

  bool WindowTask::handle_save(SchedulerHandle &info) {
    met_trace_full();
    
    auto &e_appl_data = info.global("appl_data").writeable<ApplicationData>();
    if (e_appl_data.project_save == ProjectSaveState::eNew) {
      return handle_save_as(info);
    } else {
      e_appl_data.save(e_appl_data.project_path);
      return true;
    }
  }

  bool WindowTask::handle_save_as(SchedulerHandle &info) {
    met_trace_full();
    
    if (fs::path path; detail::save_dialog(path, "json")) {
      info.global("appl_data").writeable<ApplicationData>().save(io::path_with_ext(path, ".json"));
      return true;
    }
    return false;
  }

  bool WindowTask::handle_export(SchedulerHandle &info) {
    met_trace_full();

    if (fs::path path; detail::save_dialog(path, "met")) {
      // Get shared resources
      const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
      const auto &e_proj_data = e_appl_data.project_data;
      const auto &e_spectra   = info("gen_spectral_data", "spectra").read_only<std::vector<Spec>>();
      const auto &e_weights   = info("gen_convex_weights", "bary_buffer").read_only<gl::Buffer>();

      // Insert barriers for the following operations
      gl::sync::memory_barrier(gl::BarrierFlags::eBufferUpdate | gl::BarrierFlags::eShaderStorageBuffer | gl::BarrierFlags::eClientMappedBuffer);

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
        const auto &e_delaunay = info("gen_convex_weights", "delaunay").read_only<AlignedDelaunayData>();

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
  }
  
  void WindowTask::handle_close_safe(SchedulerHandle &info) {
    met_trace_full();
    
    const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
    if (e_appl_data.project_save == ProjectSaveState::eUnsaved 
     || e_appl_data.project_save == ProjectSaveState::eNew) {
      m_open_close_modal = true;
    } else {
      handle_close(info);
    }
  }

  void WindowTask::handle_close(SchedulerHandle &info) {
    met_trace_full();
    
    // Clear OpenGL state
    ImGui::CloseAnyPopupIfOpen();
    gl::Program::unbind_all();

    // Empty application data as project is closed
    info.global("appl_data").writeable<ApplicationData>().clear();
    
    // Signal schedule re-creation and submit empty schedule for main view
    submit_schedule_empty(info);
  }
  
  void WindowTask::handle_exit_safe(SchedulerHandle &info) {
    met_trace_full();
    
    const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
    if (e_appl_data.project_save == ProjectSaveState::eUnsaved 
     || e_appl_data.project_save == ProjectSaveState::eNew) {
      m_open_exit_modal = true;
    } else {
      handle_exit(info);
    }
  }

  void WindowTask::handle_exit(SchedulerHandle &info) {
    met_trace_full();
    
    ImGui::CloseAnyPopupIfOpen();

    // Empty application data as project is closed
    info.global("appl_data").writeable<ApplicationData>().clear();

    // Signal to window that it should close itself
    info.global("window").writeable<gl::Window>().set_should_close();
    
    // Signal scheduler end
    info.clear();
  }

  void WindowTask::eval(SchedulerHandle &info) {
    met_trace_full();
    
    // Modals/popups have to be on the same level of stack as OpenPopup(), so track this state
    // and call OpenPopup() at the end if true
    m_open_close_modal  = false;
    m_open_exit_modal   = false;
    m_open_create_modal = false;

    // Set up the menu bar at the top of the window's viewport
    if (ImGui::BeginMainMenuBar()) {
      /* File menu follows */
      
      if (ImGui::BeginMenu("File")) {
        const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
        const bool is_loaded   = e_appl_data.project_save != ProjectSaveState::eUnloaded;
        const bool enable_save = e_appl_data.project_save != ProjectSaveState::eSaved 
          && e_appl_data.project_save != ProjectSaveState::eNew && is_loaded;
        
        /* Main section follows */

        if (ImGui::MenuItem("New..."))                             { m_open_create_modal = true; }
        if (ImGui::MenuItem("Open..."))                            { handle_open(info);               }
        if (ImGui::MenuItem("Close", nullptr, nullptr, is_loaded)) { handle_close_safe(info);         }

        ImGui::Separator(); 

        /* Save section follows */

        if (ImGui::MenuItem("Save", nullptr, nullptr, enable_save))     { handle_save(info);    }
        if (ImGui::MenuItem("Save as...", nullptr, nullptr, is_loaded)) { handle_save_as(info); }

        ImGui::Separator(); 

        if (ImGui::MenuItem("Export", nullptr, nullptr, is_loaded)) { handle_export(info); }

        ImGui::Separator(); 

        /* Miscellaneous section follows */

        if (ImGui::MenuItem("Exit")) {  handle_exit_safe(info); }

        ImGui::EndMenu();
      }

      /* Edit menu follows */

      if (ImGui::BeginMenu("Edit")) {
        auto &e_appl_data = info.global("appl_data").writeable<ApplicationData>();
        const bool is_undo = e_appl_data.mod_i >= 0;
        const bool is_redo = e_appl_data.mod_i < int(e_appl_data.mods.size()) - 1;
        if (ImGui::MenuItem("Undo", nullptr, nullptr, is_undo)) { e_appl_data.undo_mod(); }
        if (ImGui::MenuItem("Redo", nullptr, nullptr, is_redo)) { e_appl_data.redo_mod(); }
        ImGui::EndMenu();
      }

      ImGui::EndMainMenuBar();
    }
    
    // Create an explicit dock space over the entire window's viewport, excluding the menu bar
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    // Spawn create modal
    if (m_open_create_modal) { 
      info.subtask(create_modal_name).init<CreateProjectTask>(create_modal_title);
      ImGui::OpenPopup(create_modal_title.c_str()); 
    }

    // Spawn close modal
    if (m_open_close_modal)  { 
      info.subtask(close_modal_name).init<LambdaTask>([&](auto &info) {
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
      info.subtask(exit_modal_name).init<LambdaTask>([&](auto &info) {
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