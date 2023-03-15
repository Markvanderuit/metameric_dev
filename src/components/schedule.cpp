// Metameric includes
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
// #include <metameric/components/pipeline/task_gen_barycentric_weights.hpp>
#include <metameric/components/pipeline/task_gen_delaunay_weights.hpp>
#include <metameric/components/pipeline/task_gen_spectral_data.hpp>
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


    // Temporary window to plot pca components
    scheduler.emplace_task<LambdaTask>("plot_models", [](auto &info) {
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

    // The following tasks define the color->spectrum uplifting pipeline and view data
    scheduler.emplace_task<GenSpectralDataTask>("gen_spectral_data");            
    scheduler.emplace_task<GenDelaunayWeightsTask>("gen_delaunay_weights");
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
    scheduler.emplace_task<FrameBeginTask>("frame_begin");
    scheduler.emplace_task<WindowTask>("window");
    scheduler.emplace_task<FrameEndTask>("frame_end", false);
  }

  /* Explicit template instantiations of submit_schedule_*<...> */

  template void submit_schedule_main<LinearScheduler>(LinearScheduler &scheduler);
  template void submit_schedule_main<detail::TaskInfo>(detail::TaskInfo &scheduler);
  template void submit_schedule_empty<LinearScheduler>(LinearScheduler &scheduler);
  template void submit_schedule_empty<detail::TaskInfo>(detail::TaskInfo &scheduler);
} // namespace met