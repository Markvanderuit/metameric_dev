#pragma once

#include <metameric/core/utility.hpp>
#include <metameric/core/detail/eigen.hpp>
#include <small_gl/window.hpp>
#include <glm/glm.hpp>

// Interoperability with eigen's types
// #define IM_VEC2_CLASS_EXTRA                                     \
//   ImVec2(const met::Array2f &v) : x(v[0]), y(v[1]) { }          \
//   ImVec2(const met::Array2i &v) : x(v[0]), y(v[1]) { }          \
//   operator met::Array2f() const { return { x, y }; }            \
//   operator met::Array2i() const { return {                      \
//     static_cast<int>(x), static_cast<int>(y) }; }

#define IM_VEC2_CLASS_EXTRA                                   \
  ImVec2(const glm::ivec2 &v) : x(v[0]), y(v[1]) { }          \
  ImVec2(const glm::vec2 &v) : x(v[0]), y(v[1]) { }           \
  operator glm::vec2() const { return { x, y }; }             \
  operator glm::ivec2() const { return {                      \
    static_cast<int>(x), static_cast<int>(y) }; }

// #define IM_VEC2_CLASS_EXTRA                                                     \
//         constexpr ImVec2(const MyVec2& f) : x(f.x), y(f.y) {}                   \
//         operator MyVec2() const { return MyVec2(x,y); }

#include <imgui.h>

// Simple guard statement syntactic sugar
#define guard(expr,...) if (!(expr)) { return __VA_ARGS__ ; }

namespace ImGui {
  // template <typename T>
  // constexpr met::Array2i to_array(T t) { 
  //   return { static_cast<int>(t[0]), static_cast<int>(t[1]) };
  // }
  
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
      // PushStyleVar(var, to_imvec2(v));
      PushStyleVar(var, ImVec2(v));
    }

    ~ScopedStyleVar() {
      PopStyleVar();
    }
  };
} // namespace ImGui