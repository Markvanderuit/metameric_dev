#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  struct FrameBeginTask : public detail::AbstractTask {
    FrameBeginTask(const std::string &name)
    : detail::AbstractTask(name) { }

    void eval(detail::TaskEvalInfo &info) override {
      ImGui::BeginFrame();
    }
  };
} // namespace met