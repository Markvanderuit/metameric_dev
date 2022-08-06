#pragma once

#define IM_VEC2_CLASS_EXTRA                                        \
  ImVec2(const met::eig::Vector2f &v) : x(v.x()), y(v.y()) { }     \
  ImVec2(const met::eig::Vector2i &v) : x(v.x()), y(v.y()) { }     \
  ImVec2(const met::eig::Array2f &v)  : x(v.x()), y(v.y()) { }     \
  ImVec2(const met::eig::Array2i &v)  : x(v.x()), y(v.y()) { }     \
  operator met::eig::Vector2f() const { return { x, y }; }         \
  operator met::eig::Vector2i() const { return {                   \
    static_cast<int>(x), static_cast<int>(y) }; }                  \
  operator met::eig::Array2f() const { return { x, y }; }          \
  operator met::eig::Array2i() const { return {                    \
    static_cast<int>(x), static_cast<int>(y) }; }

/* #define IM_VEC4_CLASS_EXTRA                                   \
  ImVec4(const glm::ivec4 &v) :                               \
  x(v[0]), y(v[1]), z(v[2]), w(v[3]) { }                      \
  ImVec4(const glm::vec4 &v)                                  \
  : x(v[0]), y(v[1]), z(v[2]), w(v[3]) { }                    \
  operator glm::vec4() const { return { x, y, z, w }; }       \
  operator glm::ivec4() const { return {                      \
    static_cast<int>(x), static_cast<int>(y),                 \
    static_cast<int>(z), static_cast<int>(w)}; } */

#include <metameric/core/math.hpp>
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
  };

  /* Useful shorthands */

  void SpacedSeparator();

void CloseAnyPopupIfOpen();

  /* Wrappers for std::string STL types.
     Src: https://github.com/ocornut/imgui/blob/master/misc/cpp/imgui_stdlib.h
     and  https://github.com/ocornut/imgui/blob/master/misc/cpp/imgui_stdlib.cpp 
  */

  IMGUI_API bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
  IMGUI_API bool InputTextMultiline(const char* label, std::string* str, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
  IMGUI_API bool InputTextWithHint(const char* label, const char* hint, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
} // namespace ImGui