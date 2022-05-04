#pragma once

#include <imgui.h>
#include <small_gl/window.hpp>

namespace ImGui {
  void Init(const gl::Window &window);
  void Destroy();
  void BeginFrame();
  void DrawFrame();
} // namespace ImGui