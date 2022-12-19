#define IMGUI_DEFINE_MATH_OPERATORS
#include <metameric/core/utility.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/window.hpp>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <implot.h>
#include <fmt/core.h>

namespace ImGui {
  static bool appl_imgui_init = false;
  static std::string appl_imgui_ini_path  = "resources/misc/imgui.ini";
  static std::string appl_imgui_font_path = "resources/misc/atkinson_hyperlegible.ttf";
  static ImVector<ImRect> s_GroupPanelLabelStack;

  void Init(const gl::Window &window, bool dark_mode) {
    guard(!appl_imgui_init);
    appl_imgui_init = true;

    // Initialize ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    // Apply requested application color scheme
    if (dark_mode) {
      ImGui::StyleColorsDark();
    } else {
      ImGui::StyleColorsLight();
    }

    // Pass context to ImGuizmo as they are separate libraries
    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());

    auto &io = ImGui::GetIO();

    // Set ini location
    io.IniFilename = appl_imgui_ini_path.c_str();

    // Enable docking mode
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    // Handle font loading/dpi scaling
    auto content_scale = window.content_scale();
    io.Fonts->AddFontFromFileTTF(appl_imgui_font_path.c_str(), 12 * content_scale , 0, 0);
    ImGui::GetStyle().ScaleAllSizes(content_scale);

    // Initialize ImGui platform specific bindings
    ImGui_ImplOpenGL3_Init();
    ImGui_ImplGlfw_InitForOpenGL((GLFWwindow *) window.object(), true);
  }

  void Destr() {
    guard(appl_imgui_init);
    appl_imgui_init = false;
    
    ImPlot::DestroyContext();
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

  void SpacedSeparator() {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
  }

  void CloseAnyPopupIfOpen() {
    guard(ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel));
    ImGui::CloseCurrentPopup();
  }

  void CloseAllPopupsIfOpen() {
    while (true) {
      guard(ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel));
      ImGui::CloseCurrentPopup();
    }
  }

  struct InputTextCallback_UserData{
    std::string*            Str;
    ImGuiInputTextCallback  ChainCallback;
    void*                   ChainCallbackUserData;
  };

  static int InputTextCallback(ImGuiInputTextCallbackData* data)
  {
      InputTextCallback_UserData* user_data = (InputTextCallback_UserData*)data->UserData;
      if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
      {
          // Resize string callback
          // If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
          std::string* str = user_data->Str;
          IM_ASSERT(data->Buf == str->c_str());
          str->resize(data->BufTextLen);
          data->Buf = (char*)str->c_str();
      }
      else if (user_data->ChainCallback)
      {
          // Forward to user callback, if any
          data->UserData = user_data->ChainCallbackUserData;
          return user_data->ChainCallback(data);
      }
      return 0;
  }

  bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data) {
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCallback_UserData cb_user_data;
    cb_user_data.Str = str;
    cb_user_data.ChainCallback = callback;
    cb_user_data.ChainCallbackUserData = user_data;
    return InputText(label, (char*)str->c_str(), str->capacity() + 1, flags, InputTextCallback, &cb_user_data);
  }

  bool InputTextMultiline(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data) {
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCallback_UserData cb_user_data;
    cb_user_data.Str = str;
    cb_user_data.ChainCallback = callback;
    cb_user_data.ChainCallbackUserData = user_data;
    return InputTextMultiline(label, (char*)str->c_str(), str->capacity() + 1, size, flags, InputTextCallback, &cb_user_data);
  }

  bool InputTextWithHint(const char* label, const char* hint, std::string* str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data) {
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCallback_UserData cb_user_data;
    cb_user_data.Str = str;
    cb_user_data.ChainCallback = callback;
    cb_user_data.ChainCallbackUserData = user_data;
    return InputTextWithHint(label, hint, (char*)str->c_str(), str->capacity() + 1, flags, InputTextCallback, &cb_user_data);
  }
} // namespace ImGui