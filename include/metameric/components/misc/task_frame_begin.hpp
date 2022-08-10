#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  struct FrameBeginTask : public detail::AbstractTask {
    FrameBeginTask(const std::string &name)
    : detail::AbstractTask(name) { }

    void eval(detail::TaskEvalInfo &info) override {
      met_declare_trace_zone();

      ImGui::BeginFrame();
    }
  };
} // namespace met