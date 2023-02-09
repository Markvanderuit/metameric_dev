// Metameric includes
#include <metameric/core/linprog.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/schedule.hpp>

// Miscellaneous tasks
#include <metameric/components/misc/task_lambda.hpp>
#include <metameric/components/misc/task_frame_begin.hpp>
#include <metameric/components/misc/task_frame_end.hpp>
#include <metameric/components/misc/task_state.hpp>

// Pipeline tasks
#include <metameric/components/pipeline/task_gen_color_systems.hpp>
#include <metameric/components/pipeline/task_gen_barycentric_weights.hpp>
#include <metameric/components/pipeline/task_gen_spectral_gamut.hpp>
// #include <metameric/components/pipeline/task_gen_spectral_texture.hpp>

// View data tasks
#include <metameric/components/pipeline/task_gen_color_mappings.hpp>
#include <metameric/components/pipeline/task_gen_color_solids.hpp>

// View tasks
#include <metameric/components/views/task_error_viewer.hpp>
#include <metameric/components/views/task_weight_viewer.hpp>
#include <metameric/components/views/task_mappings_viewer.hpp>
#include <metameric/components/views/task_spectra_editor.hpp>
#include <metameric/components/views/task_viewport.hpp>
#include <metameric/components/views/task_window.hpp>
#include <metameric/components/views/detail/imgui.hpp>

// State changes
#include <small_gl/framebuffer.hpp>

// TODO remove
#include <metameric/core/metamer.hpp>

namespace met {
  template <typename Scheduler>
  void submit_schedule_debug(Scheduler &scheduler) {
    /* scheduler.emplace_task<LambdaTask>("imgui_demo", [](auto &) {  ImGui::ShowDemoWindow(); });
    scheduler.emplace_task<LambdaTask>("imgui_metrics", [](auto &) { ImGui::ShowMetricsWindow(); }); */

    // Temporary window to show runtime schedule
    scheduler.emplace_task<LambdaTask>("schedule_view", [&](auto &info) {
      if (ImGui::Begin("Schedule")) {
        const auto &tasks = scheduler.tasks();
        const auto &resources = scheduler.resources();

        for (const auto &task : tasks) {
          if (task->is_subtask()) ImGui::Indent();
          std::string name = task->name();
          if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
            ImGui::TreePop();
          }
          if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            if (resources.contains(name)) {
              ImGui::Value("Resources", static_cast<int>(resources.at(name).size()));
            }
            ImGui::EndTooltip();
          }
          if (task->is_subtask()) ImGui::Unindent();
        }
      }
      ImGui::End();
    });

    /* // Temporary window to plot some timings
    scheduler.emplace_task<LambdaTask>("imgui_delta", [](auto &info) {
      if (ImGui::Begin("Imgui timings")) {
        auto &e_window = info.get_resource<gl::Window>(global_key, "window");
        auto &io = ImGui::GetIO();

        // Report frame times
        ImGui::LabelText("Frame delta, last", "%.3f ms (%.1f fps)", 1000.f * io.DeltaTime, 1.f / io.DeltaTime);
        ImGui::LabelText("Frame delta, average", "%.3f ms (%.1f fps)", 1000.f / io.Framerate, io.Framerate);
        
        // Report mouse pos
        const auto &input = e_window.input_info();
        ImGui::LabelText("Mouse delta", "%.1f, %.1f", io.MouseDelta.x, io.MouseDelta.y);
        ImGui::LabelText("Mouse position", "%.1f, %.1f", io.MousePos.x, io.MousePos.y);
        ImGui::LabelText("Mouse position (glfw)", "%.1f, %.1f", input.mouse_position.x(), input.mouse_position.y());
      }
      ImGui::End();
    }); */

    /* // Temporary window to plot some distributions
    scheduler.emplace_task<LambdaTask>("plot_models", [](auto &) {
      if (ImGui::Begin("Model plots")) {
        eig::Array2f plot_size = (static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                               - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin())) 
                               * eig::Array2f(.67f, 0.3f);
        ImGui::PlotLines("Emitter, d65", models::emitter_cie_d65.data(), 
          wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
        ImGui::PlotLines("Emitter, fl11", models::emitter_cie_fl11.data(), 
          wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
        ImGui::PlotLines("Emitter, ledb1", models::emitter_cie_ledb1.data(), 
          wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
        ImGui::PlotLines("Emitter, ledrgb1", models::emitter_cie_ledrgb1.data(), 
          wavelength_samples, 03, nullptr, FLT_MAX, FLT_MAX, plot_size);
        ImGui::PlotLines("CIE XYZ, x()", models::cmfs_cie_xyz.col(0).data(), 
          wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
        ImGui::PlotLines("CIE XYZ, y()", models::cmfs_cie_xyz.col(1).data(), 
          wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
        ImGui::PlotLines("CIE XYZ, z()", models::cmfs_cie_xyz.col(2).data(), 
          wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
      }
      ImGui::End();
    }); */

    // Temporary window to plot pca components
    scheduler.emplace_task<LambdaTask>("plot_models", [](auto &info) {
     /*  if (ImGui::Begin("PCA inputs")) {
        eig::Array2f plot_size = (static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                               - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin())) 
                               * eig::Array2f(.67f, 0.3f);

        // Do some stuff with the PCA bases
        auto &spectra = info.get_resource<std::vector<Spec>>(global_key, "pca_input");
        for (uint i = 0; i < spectra.size(); ++i) {
          ImGui::PlotLines(fmt::format("Input {}", i).c_str(), spectra[i].data(), 
            wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
        }
      }
      ImGui::End(); */
      
      if (ImGui::Begin("PCA plots")) {
        eig::Array2f plot_size = (static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                               - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin())) 
                               * eig::Array2f(.67f, 0.3f);

        // Do some stuff with the PCA bases
        auto &pca = info.get_resource<ApplicationData>(global_key, "app_data").loaded_basis;
        for (uint i = 0; i < pca.cols(); ++i) {
          ImGui::PlotLines(fmt::format("Component {}", i).c_str(), pca.col(i).data(), 
            wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
        }
      }
      ImGui::End();
    });
  }

  template <typename Scheduler>
  void submit_schedule_main<Scheduler>(Scheduler &scheduler) {
    scheduler.emplace_task<FrameBeginTask>("frame_begin");
    scheduler.emplace_task<StateTask>("state");

    // The following tasks define the color->spectrum uplifting pipeline
    scheduler.emplace_task<GenColorSystemsTask>("gen_color_systems");
    scheduler.emplace_task<GenSpectralGamutTask>("gen_spectral_gamut");            
    scheduler.emplace_task<GenBarycentricWeightsTask>("gen_barycentric_weights");
    // scheduler.emplace_task<GenSpectralTextureTask>("gen_spectral_texture");

    // The following tasks define view pipeline necessities
    scheduler.emplace_task<GenColorSolidsTask>("gen_color_solids");
    scheduler.emplace_task<GenColorMappingsTask>("gen_color_mappings");

    // The following tasks define view components and windows
    scheduler.emplace_task<WindowTask>("window");
    scheduler.emplace_task<ViewportTask>("viewport");
    scheduler.emplace_task<SpectraEditorTask>("spectra_editor");
    scheduler.emplace_task<MappingsViewerTask>("mappings_viewer");
    scheduler.emplace_task<ErrorViewerTask>("error_viewer");
    scheduler.emplace_task<WeightViewerTask>("weight_viewer");

    // Insert temporary unimportant tasks
    submit_schedule_debug(scheduler);

    scheduler.emplace_task<FrameEndTask>("frame_end", true);
  }
  
  template <typename Scheduler>
  void submit_schedule_empty<Scheduler>(Scheduler &scheduler) {
    gl::Framebuffer::make_default().bind();
    scheduler.emplace_task<FrameBeginTask>("frame_begin");
    scheduler.emplace_task<WindowTask>("window");
    scheduler.emplace_task<FrameEndTask>("frame_end", false);
  }

  /* Explicit template instantiations of submit_schedule_*<...> */

  template void submit_schedule_main<LinearScheduler>(LinearScheduler &scheduler);
  template void submit_schedule_main<detail::TaskInitInfo>(detail::TaskInitInfo &scheduler);
  template void submit_schedule_main<detail::TaskEvalInfo>(detail::TaskEvalInfo &scheduler);
  template void submit_schedule_main<detail::TaskDstrInfo>(detail::TaskDstrInfo &scheduler);
  template void submit_schedule_empty<LinearScheduler>(LinearScheduler &scheduler);
  template void submit_schedule_empty<detail::TaskInitInfo>(detail::TaskInitInfo &scheduler);
  template void submit_schedule_empty<detail::TaskEvalInfo>(detail::TaskEvalInfo &scheduler);
  template void submit_schedule_empty<detail::TaskDstrInfo>(detail::TaskDstrInfo &scheduler);
} // namespace met