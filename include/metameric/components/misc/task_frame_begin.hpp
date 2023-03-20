#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  struct FrameBeginTask : public detail::TaskBase {
    void eval(SchedulerHandle &info) override {
      met_trace_full();
      ImGui::BeginFrame();
    }
  };
} // namespace met