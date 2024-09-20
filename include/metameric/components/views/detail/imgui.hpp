#pragma once

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
#define IMGUI_DEFINE_MATH_OPERATORS
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

  /* Useful shorthands */

  void SpacedSeparator();
  void CloseAnyPopupIfOpen();
  void CloseAllPopupsIfOpen();
  void PlotSpectrum(const char* label, const met::Spec &reflectance, float min_bounds = -0.05f, float max_bounds = 1.05f, const ImVec2 &size = { -1, 0 });
  void PlotSpectra(const char* label, std::span<const std::string> legend, std::span<const met::Spec> reflectances, float min_bounds = -0.05f, float max_bounds = 1.05f, const ImVec2 &size = { -1, 0 });

  /* Wrappers for std::string STL types.
     Src: https://github.com/ocornut/imgui/blob/master/misc/cpp/imgui_stdlib.h
     and  https://github.com/ocornut/imgui/blob/master/misc/cpp/imgui_stdlib.cpp 
  */

  IMGUI_API bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
  IMGUI_API bool InputTextMultiline(const char* label, std::string* str, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
  IMGUI_API bool InputTextWithHint(const char* label, const char* hint, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
} // namespace ImGui