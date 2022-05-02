#pragma once

#include <metameric/core/math.h>
#include <metameric/gl/enum.h>

namespace metameric::gl {
  /* Non-templated OpenGL object wrappers */
  class Buffer;
  class Fence;
  class Framebuffer;
  class Program;
  class Sampler;
  class Shader;
  class Vertexarray;
  class Window;
  class Query;

  /* Templated texture and subobjects */
  struct DepthComponent;
  struct StencilComponent;
  template <typename T, uint D, uint Components, TextureType Ty = TextureType::eBase>
  class Texture;

  /* 
    Shorthand texture size specializations...
  */

  template <TextureType Ty = TextureType::eBase> using TextureObject1d1f = Texture<float, 1, 1, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject2d1f = Texture<float, 2, 1, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject3d1f = Texture<float, 3, 1, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject1d2f = Texture<float, 1, 2, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject2d2f = Texture<float, 2, 2, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject3d2f = Texture<float, 3, 2, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject1d3f = Texture<float, 1, 3, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject2d3f = Texture<float, 2, 3, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject3d3f = Texture<float, 3, 3, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject1d4f = Texture<float, 1, 4, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject2d4f = Texture<float, 2, 4, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject3d4f = Texture<float, 3, 4, Ty>;

  template <TextureType Ty = TextureType::eBase> using TextureObject1d1us = Texture<ushort, 1, 1, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject2d1us = Texture<ushort, 2, 1, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject3d1us = Texture<ushort, 3, 1, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject1d2us = Texture<ushort, 1, 2, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject2d2us = Texture<ushort, 2, 2, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject3d2us = Texture<ushort, 3, 2, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject1d3us = Texture<ushort, 1, 3, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject2d3us = Texture<ushort, 2, 3, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject3d3us = Texture<ushort, 3, 3, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject1d4us = Texture<ushort, 1, 4, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject2d4us = Texture<ushort, 2, 4, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject3d4us = Texture<ushort, 3, 4, Ty>;
  
  template <TextureType Ty = TextureType::eBase> using TextureObject1d1s = Texture<short, 1, 1, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject2d1s = Texture<short, 2, 1, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject3d1s = Texture<short, 3, 1, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject1d2s = Texture<short, 1, 2, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject2d2s = Texture<short, 2, 2, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject3d2s = Texture<short, 3, 2, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject1d3s = Texture<short, 1, 3, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject2d3s = Texture<short, 2, 3, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject3d3s = Texture<short, 3, 3, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject1d4s = Texture<short, 1, 4, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject2d4s = Texture<short, 2, 4, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject3d4s = Texture<short, 3, 4, Ty>;
  
  template <TextureType Ty = TextureType::eBase> using TextureObject1d1ui = Texture<uint, 1, 1, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject2d1ui = Texture<uint, 2, 1, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject3d1ui = Texture<uint, 3, 1, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject1d2ui = Texture<uint, 1, 2, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject2d2ui = Texture<uint, 2, 2, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject3d2ui = Texture<uint, 3, 2, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject1d3ui = Texture<uint, 1, 3, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject2d3ui = Texture<uint, 2, 3, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject3d3ui = Texture<uint, 3, 3, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject1d4ui = Texture<uint, 1, 4, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject2d4ui = Texture<uint, 2, 4, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject3d4ui = Texture<uint, 3, 4, Ty>;
  
  template <TextureType Ty = TextureType::eBase> using TextureObject1d1i = Texture<int, 1, 1, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject2d1i = Texture<int, 2, 1, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject3d1i = Texture<int, 3, 1, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject1d2i = Texture<int, 1, 2, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject2d2i = Texture<int, 2, 2, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject3d2i = Texture<int, 3, 2, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject1d3i = Texture<int, 1, 3, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject2d3i = Texture<int, 2, 3, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject3d3i = Texture<int, 3, 3, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject1d4i = Texture<int, 1, 4, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject2d4i = Texture<int, 2, 4, Ty>;
  template <TextureType Ty = TextureType::eBase> using TextureObject3d4i = Texture<int, 3, 4, Ty>;
  
  template <TextureType Ty = TextureType::eBase> using DepthTextureObject1d = Texture<DepthComponent, 1, 1, Ty>;
  template <TextureType Ty = TextureType::eBase> using DepthTextureObject2d = Texture<DepthComponent, 2, 1, Ty>;
  template <TextureType Ty = TextureType::eBase> using DepthTextureObject3d = Texture<DepthComponent, 3, 1, Ty>;
  template <TextureType Ty = TextureType::eBase> using StencilTextureObject1d = Texture<StencilComponent, 1, 1, Ty>;
  template <TextureType Ty = TextureType::eBase> using StencilTextureObject2d = Texture<StencilComponent, 2, 1, Ty>;
  template <TextureType Ty = TextureType::eBase> using StencilTextureObject3d = Texture<StencilComponent, 3, 1, Ty>;

  /*
    Shorthand default size + type texture specializations
  */
  
  using Texture1d1f = TextureObject1d1f<>;
  using Texture2d1f = TextureObject2d1f<>;
  using Texture3d1f = TextureObject3d1f<>;
  using Texture1d2f = TextureObject1d2f<>;
  using Texture2d2f = TextureObject2d2f<>;
  using Texture3d2f = TextureObject3d2f<>;
  using Texture1d3f = TextureObject1d3f<>;
  using Texture2d3f = TextureObject2d3f<>;
  using Texture3d3f = TextureObject3d3f<>;
  using Texture1d4f = TextureObject1d4f<>;
  using Texture2d4f = TextureObject2d4f<>;
  using Texture3d4f = TextureObject3d4f<>;

  using Texture1d1us = TextureObject1d1us<>;
  using Texture2d1us = TextureObject2d1us<>;
  using Texture3d1us = TextureObject3d1us<>;
  using Texture1d2us = TextureObject1d2us<>;
  using Texture2d2us = TextureObject2d2us<>;
  using Texture3d2us = TextureObject3d2us<>;
  using Texture1d3us = TextureObject1d3us<>;
  using Texture2d3us = TextureObject2d3us<>;
  using Texture3d3us = TextureObject3d3us<>;
  using Texture1d4us = TextureObject1d4us<>;
  using Texture2d4us = TextureObject2d4us<>;
  using Texture3d4us = TextureObject3d4us<>;
  
  using Texture1d1s = TextureObject1d1s<>;
  using Texture2d1s = TextureObject2d1s<>;
  using Texture3d1s = TextureObject3d1s<>;
  using Texture1d2s = TextureObject1d2s<>;
  using Texture2d2s = TextureObject2d2s<>;
  using Texture3d2s = TextureObject3d2s<>;
  using Texture1d3s = TextureObject1d3s<>;
  using Texture2d3s = TextureObject2d3s<>;
  using Texture3d3s = TextureObject3d3s<>;
  using Texture1d4s = TextureObject1d4s<>;
  using Texture2d4s = TextureObject2d4s<>;
  using Texture3d4s = TextureObject3d4s<>;
  
  using Texture1d1ui = TextureObject1d1ui<>;
  using Texture2d1ui = TextureObject2d1ui<>;
  using Texture3d1ui = TextureObject3d1ui<>;
  using Texture1d2ui = TextureObject1d2ui<>;
  using Texture2d2ui = TextureObject2d2ui<>;
  using Texture3d2ui = TextureObject3d2ui<>;
  using Texture1d3ui = TextureObject1d3ui<>;
  using Texture2d3ui = TextureObject2d3ui<>;
  using Texture3d3ui = TextureObject3d3ui<>;
  using Texture1d4ui = TextureObject1d4ui<>;
  using Texture2d4ui = TextureObject2d4ui<>;
  using Texture3d4ui = TextureObject3d4ui<>;
  
  using Texture1d1i = TextureObject1d1i<>;
  using Texture2d1i = TextureObject2d1i<>;
  using Texture3d1i = TextureObject3d1i<>;
  using Texture1d2i = TextureObject1d2i<>;
  using Texture2d2i = TextureObject2d2i<>;
  using Texture3d2i = TextureObject3d2i<>;
  using Texture1d3i = TextureObject1d3i<>;
  using Texture2d3i = TextureObject2d3i<>;
  using Texture3d3i = TextureObject3d3i<>;
  using Texture1d4i = TextureObject1d4i<>;
  using Texture2d4i = TextureObject2d4i<>;
  using Texture3d4i = TextureObject3d4i<>;
  
  using DepthTexture1d = DepthTextureObject1d<>;
  using DepthTexture2d = DepthTextureObject2d<>;
  using DepthTexture3d = DepthTextureObject3d<>;

  using StencilTexture1d = StencilTextureObject1d<>;
  using StencilTexture2d = StencilTextureObject2d<>;
  using StencilTexture3d = StencilTextureObject3d<>;

  /* Preprocessor defines */

  #define MET_NONCOPYABLE_CONSTR(T)\
    T(const T &) = delete;\
    T & operator= (const T &) = delete;\
    T(T &&o) noexcept { swap(o); }\
    inline T & operator= (T &&o) noexcept { swap(o); return *this; }
} // namespace metameric::gl