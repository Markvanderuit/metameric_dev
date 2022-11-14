#pragma once

#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  class ViewportEndTask : public detail::AbstractTask {

  public:
    ViewportEndTask(const std::string &name)
    : detail::AbstractTask(name, true) { }

    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();

      // Declare scoped ImGui style state
      auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 16.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};

      // Note: Main viewport window end is post-pended here, but the window begins in task_viewport_begin.hpp
      ImGui::End();
    }
  };
} // namespace met