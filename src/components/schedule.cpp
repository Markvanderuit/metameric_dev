#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/schedule.hpp>
#include <metameric/components/misc/task_lambda.hpp>
#include <metameric/components/misc/task_frame_begin.hpp>
#include <metameric/components/misc/task_frame_end.hpp>
#include <metameric/components/misc/task_state.hpp>
#include <metameric/components/pipeline/task_gen_delaunay_weights.hpp>
#include <metameric/components/pipeline/task_gen_spectral_data.hpp>
#include <metameric/components/pipeline/task_gen_color_mappings.hpp>
#include <metameric/components/pipeline/task_gen_color_solids.hpp>
#include <metameric/components/views/task_error_viewer.hpp>
#include <metameric/components/views/task_weight_viewer.hpp>
#include <metameric/components/views/task_mappings_viewer.hpp>
#include <metameric/components/views/task_spectra_editor.hpp>
#include <metameric/components/views/task_viewport.hpp>
#include <metameric/components/views/task_window.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <ranges>

namespace met {
  void submit_schedule_debug(detail::SchedulerBase &scheduler) {
    scheduler.emplace_task<LambdaTask>("schedule_view", [&](auto &info) {
      // Temporary window to show runtime schedule
      if (ImGui::Begin("Schedule debug")) {
        const auto &resource_map = info.resources();
        for (const auto &task_key : info.schedule()) {
          // Split string to get task name without prepend
          auto count = std::count(range_iter(task_key), '.');
          auto split = std::views::split(task_key, '.')
                     | std::views::transform([](const auto &r) { return std::string(range_iter(r)); });
          auto name = (split | std::views::drop(count)).front();

          // Indent dependent on how much of a subtask something is
          for (uint i = 0; i < count; ++i) ImGui::Indent(16.f);

          if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
            if (ImGui::IsItemHovered() && resource_map.contains(task_key)) {
              ImGui::BeginTooltip();
              for (const auto &[key, _] : resource_map.at(task_key)) {
                ImGui::Text(key.c_str());
              }
              ImGui::EndTooltip();
            }
            ImGui::TreePop();
          }

          /* if (!resource_map.contains(task_key)) {
            if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_Bullet))
              ImGui::TreePop();
          } else {
            if (ImGui::TreeNode(name.c_str())) {
              for (const auto &[key, _] : resource_map.at(task_key)) {
                if (ImGui::TreeNodeEx(fmt::format("Resource: {}", key).c_str(), ImGuiTreeNodeFlags_Leaf))
                  ImGui::TreePop();
              }
              ImGui::TreePop();
            }
          } */

          // Unindent dependent on how much of a subtask something is
          for (uint i = 0; i < count; ++i) ImGui::Unindent(16.f);
        }
      }
      ImGui::End();
    });

    /* // Temporary window to plot pca components
    scheduler.emplace_task<LambdaTask>("plot_models", [](auto &info) {
      if (ImGui::Begin("PCA plots")) {
        eig::Array2f plot_size = (static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                               - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin())) 
                               * eig::Array2f(.67f, 0.3f);

        // Do some stuff with the PCA bases
        auto &pca = info.use_resource<ApplicationData>(global_key, "app_data").loaded_basis;
        for (uint i = 0; i < pca.cols(); ++i) {
          ImGui::PlotLines(fmt::format("Component {}", i).c_str(), pca.col(i).data(), 
            wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
        }
      }
      ImGui::End();
    }); */
  }

  void submit_schedule_main(detail::SchedulerBase &scheduler) {
    scheduler.clear();

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

    scheduler.build();
  }
  
  void submit_schedule_empty(detail::SchedulerBase &scheduler) {
    scheduler.clear();
    
    scheduler.emplace_task<FrameBeginTask>("frame_begin");
    scheduler.emplace_task<WindowTask>("window");
    scheduler.emplace_task<FrameEndTask>("frame_end", false);
    
    scheduler.build();
  }
} // namespace met