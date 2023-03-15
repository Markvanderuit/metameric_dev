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
  const static std::string create_modal_name = "_create_modal";
  const static std::string close_modal_name  = "_close_modal";
  const static std::string exit_modal_name   = "_exit_modal";

  WindowTask::WindowTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void WindowTask::init(detail::TaskInfo &info) {
    met_trace_full();
    // ...
  }

  void WindowTask::dstr(detail::TaskInfo &info) {
    met_trace_full();
    
    // Remove straggling modal subtasks if they exist
    info.remove_task(name() + create_modal_name);
    info.remove_task(name() + close_modal_name);
    info.remove_task(name() + exit_modal_name);
  }
  
  bool WindowTask::handle_open(detail::TaskInfo &info) {
    met_trace_full();
    
    // Open a file picker
    if (fs::path path; detail::load_dialog(path, "json")) {
      // Initialize existing project
      info.get_resource<ApplicationData>(global_key, "app_data").load(path);

      // Clear OpenGL state
      gl::Program::unbind_all();

      // Signal schedule re-creation and submit new schedule for main view
      info.signal_flags = detail::TaskSignalFlags::eClearTasks;
      submit_schedule_main(info);
      return true;
    }
    return false;
  }

  bool WindowTask::handle_save(detail::TaskInfo &info) {
    met_trace_full();
    
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    if (e_app_data.project_save == SaveFlag::eNew) {
      return handle_save_as(info);
    } else {
      e_app_data.save(e_app_data.project_path);
      return true;
    }
  }

  bool WindowTask::handle_save_as(detail::TaskInfo &info) {
    met_trace_full();
    
    if (fs::path path; detail::save_dialog(path, "json")) {
      info.get_resource<ApplicationData>(global_key, "app_data").save(io::path_with_ext(path, ".json"));
      return true;
    }
    return false;
  }

  bool WindowTask::handle_export(detail::TaskInfo &info) {
    met_trace_full();

    if (fs::path path; detail::save_dialog(path, "met")) {
      // Get shared resources
      auto &e_app_data    = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_prj_data    = e_app_data.project_data;
      auto &e_bary_buffer = info.get_resource<gl::Buffer>("gen_delaunay_weights", "bary_buffer");
      auto &e_spectra     = info.get_resource<std::vector<Spec>>("gen_spectral_data", "vert_spec");
      auto &e_delaunay    = info.get_resource<AlignedDelaunayData>("gen_spectral_data", "delaunay");

      // Insert barriers for the following operations
      gl::sync::memory_barrier( gl::BarrierFlags::eBufferUpdate        | 
                                gl::BarrierFlags::eShaderStorageBuffer | 
                                gl::BarrierFlags::eClientMappedBuffer  );

      // Obtain packed barycentric data from buffer
      std::vector<eig::Array4f> bary_data(e_bary_buffer.size() / sizeof(float));
      e_bary_buffer.get_as<eig::Array4f>(bary_data);

      // Pack interleaved spectral data 
      std::vector<eig::Array4f> spec_data(wavelength_samples * e_delaunay.elems.size());
      for (uint i = 0; i < e_delaunay.elems.size(); ++i) {
        const auto &el = e_delaunay.elems[i];

        // Gather the four relevant spectra for this element
        std::array<Spec, 4> el_spectra;
        std::ranges::transform(el, el_spectra.begin(), [&](uint i) { return e_spectra[i]; });

        // Interleave values and scatter into data so four values are accessed in one query
        for (uint j = 0; j < wavelength_samples; ++j)
          spec_data[i * wavelength_samples + j] = eig::Array4f {
            el_spectra[0][j], el_spectra[1][j], el_spectra[2][j], el_spectra[3][j], 
          };
      }

      // Save data to specified filepath
      io::save_spectral_data({
        .bary_xres = e_app_data.loaded_texture_f32.size()[0],
        .bary_yres = e_app_data.loaded_texture_f32.size()[1],
        .bary_zres = static_cast<uint>(e_delaunay.elems.size()),
        .functions = cnt_span<float>(spec_data),
        .weights   = cnt_span<float>(bary_data)
      }, io::path_with_ext(path, ".met"));

/* 
      // Used sizes
      const uint func_count  = static_cast<uint>(e_prj_data.vertices.size());
      const auto weights_res = e_app_data.loaded_texture_f32.size();


      // Obtain padded weight data from buffers
      std::vector<AlWeight> bary_data(e_bary_buffer.size() / sizeof(AlWeight));
      e_bary_buffer.get(cnt_span<std::byte>(bary_data));

      // Copy weights without padding to wght_data_out
      std::vector<float> wght_data_out(bary_data.size() * func_count);
      #pragma omp parallel for
      for (int i = 0; i < bary_data.size(); ++i) {
        std::span<float> in(bary_data[i].data(), func_count);
        std::span<float> out(wght_data_out.data() + i * func_count, func_count);
        std::copy(range_iter(in), out.begin());
      }

      // Obtain (already unpadded) function data from buffers
      std::vector<Spec> spec_data_out(e_spec_buffer.size() / sizeof(Spec));
      e_spec_buffer.get(cnt_span<std::byte>(spec_data_out));

      // Save data to specified filepath
      io::save_spectral_data({
        .header = {
          .func_count = func_count, 
          .wght_xres = weights_res.x(), 
          .wght_yres = weights_res.y()
        },
        .functions = cnt_span<float>(spec_data_out), 
        .weights   = cnt_span<float>(wght_data_out)
      }, io::path_with_ext(path, ".met")); */

      return true;
    }

    return false;
  }
  
  void WindowTask::handle_close_safe(detail::TaskInfo &info) {
    met_trace_full();
    
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    if (e_app_data.project_save == SaveFlag::eUnsaved 
     || e_app_data.project_save == SaveFlag::eNew) {
      m_open_close_modal = true;
    } else {
      handle_close(info);
    }
  }

  void WindowTask::handle_close(detail::TaskInfo &info) {
    met_trace_full();
    
    // Clear OpenGL state
    ImGui::CloseAnyPopupIfOpen();
    gl::Program::unbind_all();

    // Empty application data as project is closed
    info.get_resource<ApplicationData>(global_key, "app_data").unload();
    
    // Signal schedule re-creation and submit empty schedule for main view
    info.signal_flags = detail::TaskSignalFlags::eClearTasks;
    submit_schedule_empty(info);
  }
  
  void WindowTask::handle_exit_safe(detail::TaskInfo &info) {
    met_trace_full();
    
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    if (e_app_data.project_save == SaveFlag::eUnsaved 
     || e_app_data.project_save == SaveFlag::eNew) {
      m_open_exit_modal = true;
    } else {
      handle_exit(info);
    }
  }

  void WindowTask::handle_exit(detail::TaskInfo &info) {
    met_trace_full();
    
    ImGui::CloseAnyPopupIfOpen();

    // Empty application data as project is closed
    info.get_resource<ApplicationData>(global_key, "app_data").unload();

    // Signal to window that it should close itself
    info.get_resource<gl::Window>(global_key, "window").set_should_close();
    
    // Signal schedule dump
    info.signal_flags = detail::TaskSignalFlags::eClearTasks;
  }

  void WindowTask::eval(detail::TaskInfo &info) {
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
        auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
        const bool is_loaded   = e_app_data.project_save != SaveFlag::eUnloaded;
        const bool enable_save = e_app_data.project_save != SaveFlag::eSaved 
          && e_app_data.project_save != SaveFlag::eNew && is_loaded;
        
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

    // Spawn create modal
    if (m_open_create_modal) { 
      info.emplace_task_after<CreateProjectTask>(name(), name() + create_modal_name, create_modal_title);
      ImGui::OpenPopup(create_modal_title.c_str()); 
    }

    // Spawn close modal
    if (m_open_close_modal)  { 
      info.emplace_task_after<LambdaTask>(name(), name() + close_modal_name, [&](auto &info) {
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
      ImGui::OpenPopup(close_modal_title.c_str()); 
    }

    // Spawm exit modal
    if (m_open_exit_modal)   { 
      info.emplace_task_after<LambdaTask>(name(), name() + exit_modal_name, [&](auto &info) {
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
      ImGui::OpenPopup(exit_modal_title.c_str()); 
    }

    if ((uint) info.signal_flags) {
      ImGui::DrawFrame();
    }
  }
} // namespace met