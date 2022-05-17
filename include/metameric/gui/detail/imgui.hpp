#pragma once

#include <metameric/core/utility.hpp>
#include <metameric/core/detail/eigen.hpp>
#include <small_gl/window.hpp>
#include <imgui.h>

// Simple guard statement syntactic sugar
#define guard(expr,...) if (!(expr)) { return __VA_ARGS__ ; }

namespace ImGui {
  template <typename T>
  constexpr ImVec2 to_imvec2(T t) { 
    return { static_cast<float>(t[0]), static_cast<float>(t[1]) }; 
  }

  template <typename T>
  constexpr met::Array2i to_array(T t) { 
    return { static_cast<int>(t[0]), static_cast<int>(t[1]) };
  }
  
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

    ScopedStyleVar(ImGuiStyleVar var, const met::Array2i &v) {
      PushStyleVar(var, to_imvec2(v));
    }

    ~ScopedStyleVar() {
      PopStyleVar();
    }
  };
} // namespace ImGui