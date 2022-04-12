#pragma once

#include <metameric/core/math.h>

namespace metameric::gl {
  // Special texture components
  struct DepthComponent;
  struct StencilComponent;

  // OpenGL objects
  class Buffer;
  class Fence;
  class Framebuffer;
  class Program;
  class Query;
  class Sampler;
  class Shader;
  class Vertexarray;

  #define MET_NONCOPYABLE_CONSTR(T)\
    T(const T &) = delete;\
    T & operator= (const T &) = delete;\
    T(T &&o) noexcept { swap(o); }\
    inline T & operator= (T &&o) noexcept { swap(o); return *this; }
} /* namespace metameric::gl */