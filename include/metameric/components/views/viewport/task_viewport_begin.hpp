#pragma once

#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met {
  class ViewportBeginTask : public detail::AbstractTask {

  public:
    ViewportBeginTask(const std::string &name)
    : detail::AbstractTask(name, true) { }

    void init(detail::TaskInitInfo &info) override {
      met_trace_full();

      // Share resources
      info.emplace_resource<gl::Texture2d3f>("draw_texture", { .size = 1 });
    }
    
    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();

      // Get shared resources
      auto &i_draw_texture = info.get_resource<gl::Texture2d3f>("draw_texture");

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
      if (!i_draw_texture.is_init() || (i_draw_texture.size() != viewport_size.cast<uint>()).all()) {
        i_draw_texture = {{ .size = viewport_size.cast<uint>() }};
      }

      // Insert image, applying viewport texture to viewport; texture can be safely drawn 
      // to later in the render loop. Flip y-axis UVs to obtain the correct orientation.
      ImGui::Image(ImGui::to_ptr(i_draw_texture.object()), viewport_size, eig::Vector2f(0, 1), eig::Vector2f(1, 0));

      // Note: Main viewport window end is post-pended in task_viewport_end.hpp
    }
  };
} // namespace met