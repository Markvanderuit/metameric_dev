#pragma once

#include <metameric/core/utility.hpp>
#include <small_gl/window.hpp>

#define IM_VEC2_CLASS_EXTRA                                   \
  ImVec2(const glm::ivec2 &v) : x(v[0]), y(v[1]) { }          \
  ImVec2(const glm::vec2 &v) : x(v[0]), y(v[1]) { }           \
  operator glm::vec2() const { return { x, y }; }             \
  operator glm::ivec2() const { return {                      \
    static_cast<int>(x), static_cast<int>(y) }; }

#define IM_VEC4_CLASS_EXTRA                                   \
  ImVec4(const glm::ivec4 &v) :                               \
  x(v[0]), y(v[1]), z(v[2]), w(v[3]) { }                      \
  ImVec4(const glm::vec4 &v)                                  \
  : x(v[0]), y(v[1]), z(v[2]), w(v[3]) { }                    \
  operator glm::vec4() const { return { x, y, z, w }; }       \
  operator glm::ivec4() const { return {                      \
    static_cast<int>(x), static_cast<int>(y),                 \
    static_cast<int>(z), static_cast<int>(w)}; }

#include <imgui.h>

namespace ImGui {
  template <typename T>
  constexpr void * to_ptr(T t) { 
    return (void *) static_cast<size_t>(t);
  }

  void Init(const gl::Window &window);
  void Destr();
  void BeginFrame();
  void DrawFrame();

  struct ScopedStyleVar {
    ScopedStyleVar() = delete;

    ScopedStyleVar(ImGuiStyleVar var, float f) {
      PushStyleVar(var, f);
    }

    ScopedStyleVar(ImGuiStyleVar var, const glm::ivec2 &v) {
      PushStyleVar(var, v);
    }

    ~ScopedStyleVar() {
      PopStyleVar();
    }
  };
} // namespace ImGui