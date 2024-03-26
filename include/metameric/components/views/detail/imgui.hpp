#pragma once

#include <metameric/components/views/detail/arcball.hpp>

#define IM_VEC2_CLASS_EXTRA                                    \
  ImVec2(const met::eig::Vector2f &v) : x(v.x()), y(v.y()) { } \
  ImVec2(const met::eig::Vector2i &v) : x(v.x()), y(v.y()) { } \
  ImVec2(const met::eig::Array2f &v)  : x(v.x()), y(v.y()) { } \
  ImVec2(const met::eig::Array2i &v)  : x(v.x()), y(v.y()) { } \
  operator met::eig::Vector2f() const { return { x, y }; }     \
  operator met::eig::Vector2i() const { return {               \
    static_cast<int>(x), static_cast<int>(y) }; }              \
  operator met::eig::Array2f() const { return { x, y }; }      \
  operator met::eig::Array2i() const { return {                \
    static_cast<int>(x), static_cast<int>(y) }; }

#define IM_VEC4_CLASS_EXTRA                                                        \
  ImVec4(const met::eig::Vector4f &v) : x(v.x()), y(v.y()), z(v.z()), w(v.w()) { } \
  ImVec4(const met::eig::Vector4i &v) : x(v.x()), y(v.y()), z(v.z()), w(v.w()) { } \
  ImVec4(const met::eig::Array4f &v)  : x(v.x()), y(v.y()), z(v.z()), w(v.w()) { } \
  ImVec4(const met::eig::Array4i &v)  : x(v.x()), y(v.y()), z(v.z()), w(v.w()) { } \
  operator met::eig::Vector4f() const { return { x, y, z, w }; }                   \
  operator met::eig::Vector4i() const { return {                                   \
    static_cast<int>(x), static_cast<int>(y),                                      \
    static_cast<int>(z), static_cast<int>(w) }; }                                  \
  operator met::eig::Array4f() const { return { x, y, z, w }; }                    \
  operator met::eig::Array4i() const { return {                                    \
    static_cast<int>(x), static_cast<int>(y),                                      \
    static_cast<int>(z), static_cast<int>(w) }; }

#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <small_gl/fwd.hpp>
#include <imgui.h>
#include <string>

namespace ImGui {
  template <typename T>
  constexpr void * to_ptr(T t) { 
    return (void *) static_cast<size_t>(t);
  }

  void Init(const gl::Window &window, bool dark_mode = true);
  void Destr();
  void BeginFrame();
  void DrawFrame();

  /* Useful objects */

  // Helper for RAII ImGui PushStyleVar/PopStyleVar wrapper
  struct ScopedStyleVar {
    ScopedStyleVar() = delete;

    ScopedStyleVar(ImGuiStyleVar var, float f) {
      PushStyleVar(var, f);
    }

    ScopedStyleVar(ImGuiStyleVar var, const met::eig::Array2f &v) {
      PushStyleVar(var, v);
    }

    ~ScopedStyleVar() {
      PopStyleVar();
    }

  public:
    inline void swap(ScopedStyleVar &o) { /* ... */ }
    met_declare_noncopyable(ScopedStyleVar);
  };

  // Helper for RAII ImGui PushID/PopID wrapper
  struct ScopedID {
    ScopedID() = delete;

    ScopedID(const std::string &s) {
      PushID(s.c_str());
    }

    ~ScopedID() {
      PopID();
    }

  public:
    inline void swap(ScopedID &o) { /* ... */ }
    met_declare_noncopyable(ScopedID);
  };

  // ImGuizmo wrapper object to make handling gizmos slightly easier
  class Gizmo {
    using trf = Eigen::Affine3f;

    bool m_is_active = false;
    trf  m_delta;
  public:
    // 
    enum class Operation : met::uint {
      eTranslate = 7u,
      eRotate    = 120u,
      eScale     = 896u,
      eAll       = eTranslate | eRotate | eScale
    };
    
    // Begin/eval/end functions, s.t. eval() returns a delta transform applied to the current
    // transform over every frame, and the user can detect changes
    bool begin_delta(const met::detail::Arcball &arcball, const trf &current_trf, Operation op = Operation::eTranslate);
    std::pair<bool, trf> 
         eval_delta();
    bool end_delta();

    // eval function, s.t. the current_trf variable is modified over every frame
    void eval(const met::detail::Arcball &arcball, trf &current_trf, Operation op = Operation::eAll);
  };

  /* Useful shorthands */

  void SpacedSeparator();
  void CloseAnyPopupIfOpen();
  void CloseAllPopupsIfOpen();
  void PlotSpectrum(const char* label, const met::Spec &reflectance, float min_bounds = -0.05f, float max_bounds = 1.05f, const ImVec2 &size = { -1, 0 });

  /* Wrappers for std::string STL types.
     Src: https://github.com/ocornut/imgui/blob/master/misc/cpp/imgui_stdlib.h
     and  https://github.com/ocornut/imgui/blob/master/misc/cpp/imgui_stdlib.cpp 
  */

  IMGUI_API bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
  IMGUI_API bool InputTextMultiline(const char* label, std::string* str, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
  IMGUI_API bool InputTextWithHint(const char* label, const char* hint, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
} // namespace ImGui