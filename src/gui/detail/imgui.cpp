#include <metameric/gui/detail/imgui.hpp>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace ImGui {
  void Init(const gl::Window &window) {
    // Initialize ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsLight();

    // Specify optional config flags
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Initialize ImGui platform specific bindings
    ImGui_ImplGlfw_InitForOpenGL((GLFWwindow *) window.object(), true);
    ImGui_ImplOpenGL3_Init();
  }

  void Destroy() {
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();
  }

  void BeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
  }

  void DrawFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  }
} // namespace ImGui