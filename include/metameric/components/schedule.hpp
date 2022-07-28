#pragma once

// Metameric includes
#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>

// Miscellaneous
#include <metameric/components/misc/task_lambda.hpp>
#include <metameric/components/misc/task_frame_begin.hpp>
#include <metameric/components/misc/task_frame_end.hpp>

// Pipeline tasks
#include <metameric/components/tasks/task_generate_spectral_gamut.hpp>
#include <metameric/components/tasks/task_generate_spectral_texture.hpp>
#include <metameric/components/tasks/task_compute_color_mapping.hpp>

// View tasks
#include <metameric/components/views/task_gamut_viewer.hpp>
#include <metameric/components/views/task_mappings_editor.hpp>
#include <metameric/components/views/task_viewport.hpp>
#include <metameric/components/views/task_window.hpp>
#include <metameric/components/views/detail/imgui.hpp>

// TODO remove
#include <metameric/components/tasks/mapping_cpu_task.hpp>
#include <metameric/components/views/image_viewer.hpp>

namespace met {
  template <typename Scheduler>
  void submit_schedule_debug(Scheduler &scheduler) {
    scheduler.emplace_task<LambdaTask>("imgui_demo", [](auto &) {  ImGui::ShowDemoWindow(); });
    scheduler.emplace_task<LambdaTask>("imgui_metrics", [](auto &) { ImGui::ShowMetricsWindow(); });

    // Temporary window to plot some timings
    scheduler.emplace_task<LambdaTask>("imgui_delta", [](auto &info) {
      if (ImGui::Begin("Imgui timings")) {
        auto &e_window = info.get_resource<gl::Window>(global_key, "window");
        auto &io = ImGui::GetIO();

        // Report frame times
        ImGui::LabelText("Frame delta, last", "%.3f ms (%.1f fps)", 1000.f * io.DeltaTime, 1.f / io.DeltaTime);
        ImGui::LabelText("Frame delta, average", "%.3f ms (%.1f fps)", 1000.f / io.Framerate, io.Framerate);
        
        // Report mouse pos
        const auto &input = e_window.input_info();
        
        glm::vec2 mouse_pos_2 = input.mouse_position;
        ImGui::LabelText("Mouse delta", "%.1f, %.1f", io.MouseDelta.x, io.MouseDelta.y);
        ImGui::LabelText("Mouse position", "%.1f, %.1f", io.MousePos.x, io.MousePos.y);
        ImGui::LabelText("Mouse position (glfw)", "%.1f, %.1f", mouse_pos_2.x, mouse_pos_2.y);
      }
      ImGui::End();
    });
    
    // Temporary window to plot some distributions
    scheduler.emplace_task<LambdaTask>("plot_models", [](auto &) {
      if (ImGui::Begin("Model plots")) {
        auto plot_size = (static_cast<glm::vec2>(ImGui::GetWindowContentRegionMax())
                        - static_cast<glm::vec2>(ImGui::GetWindowContentRegionMin())) * glm::vec2(.67f, 0.3f);
        ImGui::PlotLines("Emitter, d65", models::emitter_cie_d65.data(), 
          wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
        ImGui::PlotLines("Emitter, fl11", models::emitter_cie_fl11.data(), 
          wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
        ImGui::PlotLines("Emitter, ledb1", models::emitter_cie_ledb1.data(), 
          wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
        ImGui::PlotLines("Emitter, ledrgb1", models::emitter_cie_ledrgb1.data(), 
          wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
        ImGui::PlotLines("CIE XYZ, x()", models::cmfs_cie_xyz.col(0).data(), 
          wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
        ImGui::PlotLines("CIE XYZ, y()", models::cmfs_cie_xyz.col(1).data(), 
          wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
        ImGui::PlotLines("CIE XYZ, z()", models::cmfs_cie_xyz.col(2).data(), 
          wavelength_samples, 0, nullptr, FLT_MAX, FLT_MAX, plot_size);
      }
      ImGui::End();
    });
  }

  template <typename Scheduler>
  void submit_schedule_main(Scheduler &scheduler) {
    scheduler.emplace_task<FrameBeginTask>("frame_begin");
    scheduler.emplace_task<WindowTask>("window");

    // The following tasks define the uplifting pipeline
    scheduler.emplace_task<GenerateSpectralGamutTask>("generate_gamut");
    scheduler.emplace_task<GenerateSpectralTextureTask>("generate_spectral");
    scheduler.emplace_task<ComputeColorMappingTask>("mapping");

    // TODO remove
    scheduler.emplace_task<MappingCPUTask>("mapping_cpu");

    // The following tasks define UI components and windows
    scheduler.emplace_task<ViewportTask>("viewport");
    scheduler.emplace_task<GamutViewerTask>("gamut_viewer");
    scheduler.emplace_task<MappingsEditorTask>("mapping_viewer");

    // TODO remove
    scheduler.emplace_task<ImageViewerTask>("image_viewer");

    // Insert temporary unimportant tasks
    submit_schedule_debug(scheduler);
    
    scheduler.emplace_task<FrameEndTask>("frame_end");
  }
  
  template <typename Scheduler>
  void submit_schedule_empty(Scheduler &scheduler) {
    scheduler.emplace_task<FrameBeginTask>("frame_begin");
    scheduler.emplace_task<WindowTask>("window");
    scheduler.emplace_task<FrameEndTask>("frame_end");
  }
} // namespace met