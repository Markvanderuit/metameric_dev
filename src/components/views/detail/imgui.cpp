#include <metameric/core/utility.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuizmo.h>
#include <fmt/core.h>

namespace ImGui {
  static bool appl_imgui_init = false;
  static std::string appl_imgui_ini_path;
  static std::string appl_imgui_font_path;
  
  void Init(const gl::Window &window, bool dark_mode) {
    guard(!appl_imgui_init);
    appl_imgui_init = true;
    appl_imgui_ini_path = "resources/misc/imgui.ini";
    appl_imgui_font_path = "resources/misc/atkinson_hyperlegible.ttf";

    // Initialize ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Apply requested application color scheme
    if (dark_mode) {
        ImGui::StyleColorsDark();
    } else {
        ImGui::StyleColorsLight();
    }

    // Pass context to ImGuizmo as they are separate libraries
    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());

    auto &io = ImGui::GetIO();
    auto &style = ImGui::GetStyle();
    auto content_scale = window.content_scale();

    // Set ini location
    io.IniFilename = appl_imgui_ini_path.c_str();

    // Enable docking mode
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    // Handle font loading/dpi scaling
    io.Fonts->AddFontFromFileTTF(appl_imgui_font_path.c_str(), 12 * content_scale , 0, 0);
    style.ScaleAllSizes(content_scale);

    // Initialize ImGui platform specific bindings
    ImGui_ImplOpenGL3_Init();
    ImGui_ImplGlfw_InitForOpenGL((GLFWwindow *) window.object(), true);
  }

  void Destr() {
    guard(appl_imgui_init);
    appl_imgui_init = false;
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
  }

  void BeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
  }

  void DrawFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  }
} // namespace ImGui