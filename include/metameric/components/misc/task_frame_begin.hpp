#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/window.hpp>

namespace met {
  struct FrameBeginTask : public detail::TaskNode {
    void init(SchedulerHandle &info) override {
      met_trace();
      ImGui::Init(info.global("window").getr<gl::Window>());
    }

    void dstr(SchedulerHandle &info) override {
      met_trace();
      ImGui::Destr();
    }
    
    void eval(SchedulerHandle &info) override {
      met_trace();
      ImGui::BeginFrame();
    }
  };
} // namespace met