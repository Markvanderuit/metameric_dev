#define IMGUI_DEFINE_MATH_OPERATORS
#include <metameric/core/ranges.hpp>
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
    met_trace_full();

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

    // Handle dpi scaling
    ImGui::GetStyle().ScaleAllSizes(window.content_scale());

    // Initialize ImGui platform specific bindings
    ImGui_ImplOpenGL3_Init();
    ImGui_ImplGlfw_InitForOpenGL((GLFWwindow *) window.object(), true);
  }

  void Destr() {
    met_trace_full();

    guard(appl_imgui_init);
    appl_imgui_init = false;
    
    ImPlot::DestroyContext();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
  }

  void BeginFrame() {
    met_trace_full();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
  }

  void DrawFrame() {
    met_trace_full();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  }

  void SpacedSeparator() {
    met_trace();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
  }

  void CloseAnyPopupIfOpen() {
    met_trace();
    guard(ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel));
    ImGui::CloseCurrentPopup();
  }

  void CloseAllPopupsIfOpen() {
    met_trace();
  
    while (true) {
      guard(ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel));
      ImGui::CloseCurrentPopup();
    }
  }

  void PlotSpectra(const char* label, std::span<const std::string> legend, std::span<const met::Spec> reflectances, float min_bounds, float max_bounds, const ImVec2 &size) {
    using namespace met;

    if (ImPlot::BeginPlot(label, size, ImPlotFlags_Crosshairs | ImPlotFlags_NoFrame)) {
      // Get wavelength values for x-axis in plot
      Spec x_values;
      rng::copy(vws::iota(0u, wavelength_samples) | vws::transform(wavelength_at_index), x_values.begin());
      
      // Setup minimal format for coming line plots
      ImPlot::SetupAxes("Wavelength", "Value", ImPlotAxisFlags_NoGridLines, ImPlotAxisFlags_NoDecorations);

      // More restrained 400-700nm to ignore funky edges
      ImPlot::SetupAxesLimits(400.f, 700.f, min_bounds, max_bounds, ImPlotCond_Always);

      // Plot data lines
      if (legend.empty()) {
        for (const auto &[i, sd] : met::enumerate_view(reflectances))
          ImPlot::PlotLine(std::format("{}", i).c_str(), x_values.data(), sd.data(), wavelength_samples);
      } else {
        for (const auto &[text, sd] : met::vws::zip(legend, reflectances))
          ImPlot::PlotLine(text.c_str(), x_values.data(), sd.data(), wavelength_samples);
      }
    
      ImPlot::EndPlot();
    }
  }

  void PlotSpectrum(const char* label, const met::Spec &sd, float min_bounds, float max_bounds, const ImVec2 &size) {
    using namespace met;

    if (ImPlot::BeginPlot(label, size, ImPlotFlags_NoInputs | ImPlotFlags_NoFrame)) {
      // Get wavelength values for x-axis in plot
      Spec x_values;
      rng::copy(vws::iota(0u, wavelength_samples) | vws::transform(wavelength_at_index), x_values.begin());
      
      // Setup minimal format for coming line plots
      ImPlot::SetupAxes("Wavelength", "Value", ImPlotAxisFlags_NoGridLines, ImPlotAxisFlags_NoDecorations);

      // More restrained 400-700nm to ignore funky edges
      ImPlot::SetupAxesLimits(400.f, 700.f, min_bounds, max_bounds, ImPlotCond_Always);

      ImPlot::PlotLine("", x_values.data(), sd.data(), wavelength_samples);
      ImPlot::EndPlot();
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

  bool Gizmo::begin_delta(const met::detail::Arcball &arcball, trf init_trf, Operation op) {
    met_trace();

    using namespace met;
    
    // Reset internal state
    if (!m_is_active) {
      m_init_trf  = init_trf;
      m_delta_trf = trf::Identity();
    }

    // Compute viewport offset and size, minus ImGui's tab bars etc
    eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                               + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
    eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                               - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
    
    // Specify ImGuizmo settings for current viewport and insert the gizmo
    ImGuizmo::SetRect(viewport_offs[0], viewport_offs[1], viewport_size[0], viewport_size[1]);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::Manipulate(arcball.view().data(), arcball.proj().data(), 
      static_cast<ImGuizmo::OPERATION>(op), ImGuizmo::MODE::LOCAL, 
      m_init_trf.data(), m_delta_trf.data());

    guard(!m_is_active && ImGuizmo::IsUsing(), false);
    m_is_active = true;
    return true;
  }

  std::pair<bool, Gizmo::trf> Gizmo::eval_delta() {
    met_trace();
    guard(m_is_active && ImGuizmo::IsUsing(), {false, trf::Identity()});
    return { true, m_delta_trf };
  }

  bool Gizmo::end_delta() {
    met_trace();
    guard(m_is_active && !ImGuizmo::IsUsing(), false);
    m_is_active = false;
    return true;
  }

  void Gizmo::eval(const met::detail::Arcball &arcball, trf &current_trf, Operation op) {
    met_trace();

    using namespace met;
    
    // Reset internal state
    m_delta_trf = trf::Identity();

    // Compute viewport offset and size, minus ImGui's tab bars etc
    eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                               + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
    eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                               - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
    
    // Specify ImGuizmo settings for current viewport and insert the gizmo
    ImGuizmo::SetRect(viewport_offs[0], viewport_offs[1], viewport_size[0], viewport_size[1]);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::Manipulate(arcball.view().data(), arcball.proj().data(), 
      static_cast<ImGuizmo::OPERATION>(op), ImGuizmo::MODE::LOCAL, current_trf.data(), m_delta_trf.data());

    // Setup phase
    if (!m_is_active && ImGuizmo::IsUsing()) {
      m_is_active = true;
    }

    // Move phase
    if (ImGuizmo::IsUsing()) {

    }

    // Teardown phase
    if (m_is_active && !ImGuizmo::IsUsing()) {
      m_is_active = false;

    }
  }

  bool Gizmo::is_over() const {
    return ImGuizmo::IsOver();
  }
} // namespace ImGui