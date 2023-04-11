#pragma once

#include <metameric/core/data.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/window.hpp>

namespace met {
  struct FrameBeginTask : public detail::TaskNode {
    void init(SchedulerHandle &info) override {
      met_trace();

      // Get external resources
      const auto &e_window    = info.global("window").read_only<gl::Window>();
      const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();

      // Initialize ImGui for all following tasks
      ImGui::Init(e_window, e_appl_data.color_mode == ApplicationData::ColorMode::eDark);
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