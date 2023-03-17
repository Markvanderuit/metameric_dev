#pragma once

#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met {
  class ViewportBeginTask : public detail::TaskBase {
  public:
    void init(detail::SchedulerHandle &info) override {
      met_trace_full();

      // Share resources
      info.emplace_resource<gl::Texture2d4f>("lrgb_target", { .size = 1 });
      info.emplace_resource<gl::Texture2d4f>("srgb_target", { .size = 1 });
    }
    
    void eval(detail::SchedulerHandle &info) override {
      met_trace_full();

      // Get shared resources
      auto &i_lrgb_target = info.get_resource<gl::Texture2d4f>("lrgb_target");
      auto &i_srgb_target = info.get_resource<gl::Texture2d4f>("srgb_target");

      // Declare scoped ImGui style state
      auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 16.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};
      
      // Begin main viewport window
      ImGui::Begin("Viewport", 0, ImGuiWindowFlags_NoBringToFrontOnFocus);

      // Compute viewport size minus ImGui's tab bars etc
      // (Re-)create viewport texture if necessary; attached framebuffers are resized separately
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      if (!i_lrgb_target.is_init() || (i_lrgb_target.size() != viewport_size.cast<uint>()).any()) {
        i_lrgb_target = {{ .size = viewport_size.max(1.f).cast<uint>() }};
        i_srgb_target = {{ .size = viewport_size.max(1.f).cast<uint>() }};
      }

      // Insert image, applying viewport texture to viewport; texture can be safely drawn 
      // to later in the render loop. Flip y-axis UVs to obtain the correct orientation.
      ImGui::Image(ImGui::to_ptr(i_srgb_target.object()), viewport_size, eig::Vector2f(0, 1), eig::Vector2f(1, 0));

      // Note: Main viewport window end is post-pended in task_viewport_end.hpp
    }
  };
} // namespace met