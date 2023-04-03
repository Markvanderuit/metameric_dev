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
#include <metameric/components/pipeline/task_gen_generalized_weights.hpp>
#include <metameric/components/pipeline/task_gen_spectral_data.hpp>
#include <metameric/components/pipeline/task_gen_color_mappings.hpp>
#include <metameric/components/pipeline/task_gen_color_system_solid.hpp>
#include <metameric/components/pipeline/task_gen_mismatch_solid.hpp>
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
    scheduler.task("schedule_view").init<LambdaTask>([&](SchedulerHandle &info) {
      // Check if the scheduler handle implements a MapBasedSchedule
      auto *info_data = dynamic_cast<MapBasedSchedule *>(&info);
      guard(info_data);

      // Temporary window to show runtime schedule
      if (ImGui::Begin("Scheduler info")) {
        // Query MapBasedSchedule information
        const auto &rsrc_map = info_data->resources();
        const auto &task_map = info_data->tasks();
        const auto  schedule = info_data->schedule();

        for (const auto &task_key : schedule) {
          // Split string to get task name without prepend
          auto count = std::count(range_iter(task_key), '.');
          auto split = std::views::split(task_key, '.')
                     | std::views::transform([](const auto &r) { return std::string(range_iter(r)); });
          auto name = (split | std::views::drop(count)).front();
          
          // Indent dependent on how much of a subtask something is
          for (uint i = 0; i < count; ++i) ImGui::Indent(16.f);

          if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
            if (ImGui::IsItemHovered() && rsrc_map.contains(task_key)) {
              ImGui::BeginTooltip();
              for (const auto &[key, _] : rsrc_map.at(task_key)) {
                ImGui::Text(key.c_str());
              }
              ImGui::EndTooltip();
            }
            ImGui::TreePop();
          }

          // Unindent dependent on how much of a subtask something is
          for (uint i = 0; i < count; ++i) ImGui::Unindent(16.f);
        }
      }
      ImGui::End();
    });

    // Temporary window to plot pca components
    scheduler.task("plot_models").init<LambdaTask>([](auto &info) {
      if (ImGui::Begin("PCA plots")) {
        eig::Array2f plot_size = (static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                               - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin())) 
                               * eig::Array2f(.67f, 0.3f);

        // Do some stuff with the PCA bases
        const auto &pca = info.global("appl_data").read_only<ApplicationData>().loaded_basis;
        for (uint i = 0; i < pca.cols(); ++i) {
          ImGui::PlotLines(fmt::format("Component {}", i).c_str(), pca.col(i).data(), 
            wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
        }
      }
      ImGui::End();
    });
  }

  void submit_schedule_main(detail::SchedulerBase &scheduler) {
    debug::check_expr(scheduler.global("appl_data").is_init() && scheduler.global("window").is_init());

    const auto &e_appl_data = scheduler.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data = e_appl_data.project_data;
    
    scheduler.clear();

    scheduler.task("frame_begin").init<FrameBeginTask>();
    scheduler.task("state").init<StateTask>();

    // The following tasks define the color->spectrum uplifting pipeline and dependent data
    scheduler.task("gen_spectral_data").init<GenSpectralDataTask>();
    if (e_proj_data.meshing_type == ProjectMeshingType::eConvexHull) {
      scheduler.task("gen_convex_weights").init<GenGeneralizedWeightsTask>();
    } else {
      scheduler.task("gen_convex_weights").init<GenDelaunayWeightsTask>();
    }
    scheduler.task("gen_color_mappings").init<GenColorMappingsTask>();
    
    // The following tasks define dependent data for the view components
    scheduler.task("gen_color_system_solid").init<GenColorSystemSolidTask>();
    scheduler.task("gen_mismatch_solid").init<GenMismatchSolidTask>();

    // The following tasks define view components and windows
    scheduler.task("window").init<WindowTask>();
    scheduler.task("viewport").init<ViewportTask>();
    scheduler.task("spectra_editor").init<SpectraEditorTask>();
    scheduler.task("mappings_viewer").init<MappingsViewerTask>();
    scheduler.task("error_viewer").init<ErrorViewerTask>();
    scheduler.task("weight_viewer").init<WeightViewerTask>();

    // Insert temporary unimportant tasks
    // submit_schedule_debug(scheduler);

    scheduler.task("frame_end").init<FrameEndTask>(true);
  }
  
  void submit_schedule_empty(detail::SchedulerBase &scheduler) {
    debug::check_expr(scheduler.global("window").is_init());
    
    scheduler.clear();
    
    scheduler.task("frame_begin").init<FrameBeginTask>();
    scheduler.task("window").init<WindowTask>();
    scheduler.task("frame_end").init<FrameEndTask>(false);
  }

  void submit_schedule(detail::SchedulerBase &scheduler) {
    debug::check_expr(scheduler.global("window").is_init());

    if (auto rsrc = scheduler.global("appl_data"); rsrc.is_init()) {
      const auto &e_appl_data = rsrc.read_only<ApplicationData>();
      if (e_appl_data.project_save != ProjectSaveState::eUnloaded) {
        submit_schedule_main(scheduler);
      } else {
        submit_schedule_empty(scheduler);
      }
    } else {
      submit_schedule_empty(scheduler);
    }
  }
} // namespace met