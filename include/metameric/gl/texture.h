#pragma once

#include <metameric/gl/detail/fwd.h>
#include <metameric/gl/detail/handle.h>
#include <metameric/gl/enums.h>

namespace metameric::gl {
  namespace detail {
    // Use template specialization to determine constructor parameter dimensions at compile time.
    template <uint D, TextureType Ty> constexpr uint constr_dims();
    template <> constexpr uint constr_dims<1, TextureType::eBase>()              { return 1; }
    template <> constexpr uint constr_dims<2, TextureType::eBase>()              { return 2; }
    template <> constexpr uint constr_dims<3, TextureType::eBase>()              { return 3; }
    template <> constexpr uint constr_dims<1, TextureType::eArray>()             { return 2; }
    template <> constexpr uint constr_dims<2, TextureType::eArray>()             { return 3; }
    template <> constexpr uint constr_dims<2, TextureType::eCubemap>()           { return 2; }
    template <> constexpr uint constr_dims<2, TextureType::eCubemapArray>()      { return 3; }
    template <> constexpr uint constr_dims<2, TextureType::eMultisample>()       { return 2; }
    template <> constexpr uint constr_dims<2, TextureType::eMultisampleArray>()  { return 3; }

    template <TextureType Ty>
    constexpr bool is_cubemap_type 
      = Ty == TextureType::eCubemap
     || Ty == TextureType::eCubemapArray;

    template <TextureType Ty>
    constexpr bool is_array_type 
      = Ty == TextureType::eArray 
     || Ty == TextureType::eCubemapArray
     || Ty == TextureType::eMultisampleArray;

    template <TextureType Ty>
    constexpr bool is_multisample_type 
      = Ty == TextureType::eMultisample 
     || Ty == TextureType::eMultisampleArray;
  } // namespace detail

  // Declare tag objects, these designate special depth/stencil types for template
  // parameter T; the rest uses basic float/int/uint/short/ushort 
  struct DepthComponent { };
  struct StencilComponent { };
  
  /**
   * Helper data object to construct a texture with mostly default settings. Works 
   * with aggregate/designated initialization.
   */
  template <typename T, uint D, TextureType Ty = TextureType::eBase>
  class TextureCreateInfo {
    using ArrayXi = eig::Array<int, detail::constr_dims<D, Ty>(), 1>;
    
  public:
    ArrayXi dims = ArrayXi::Ones();
    uint levels = 1u;
    T const *data = nullptr;
    size_t data_size = 0;
  };

  /**
   * Texture object. 
   * 
   * Supports 1d/2d/3d textures, 1d/2d texture arrays, 2d cubemaps, 2d cubemap arrays, 
   * 2d multisampled textures, and 2d multisampled arrays. A large collection of template 
   * specializations is accessible through names declared below.
   */
  template <typename T, uint D, uint Components, TextureType Ty = TextureType::eBase>
  class Texture : public Handle<> {
    using Base = Handle<>;
    using ArrayXi = eig::Array<int, detail::constr_dims<D, Ty>(), 1>;
    using TextureCreateInfo = TextureCreateInfo<T, D, Ty>;

    uint _levels;
    ArrayXi _size;

  public: 
    /* getters/setters */

    uint levels() const { return _levels; }
    ArrayXi size() const { return _size; }

    /* primary constr/destr */
    
    Texture() = default;
    Texture(ArrayXi dims, uint levels = 1u,T const *data = nullptr,size_t data_size = 0);
    Texture(TextureCreateInfo info);
    ~Texture();

    /* function(...) implementations for most texture types */

    void set_image(const T* data, size_t data_size, uint level = 0)
      requires !detail::is_cubemap_type<Ty>;

    void set_subimage(const T *data, size_t data_size, uint level = 0,
      ArrayXi size = ArrayXi::Zero(), ArrayXi offset = ArrayXi::Zero())
      requires !detail::is_cubemap_type<Ty>;

    void clear_image(const T *data, uint level = 0)
      requires !detail::is_cubemap_type<Ty>;
    
    void clear_subimage(const T *data, uint level = 0, 
      ArrayXi size = ArrayXi::Zero(), ArrayXi offset = ArrayXi::Zero())
      requires !detail::is_cubemap_type<Ty>;

    void get_image(T *data, size_t data_size, uint level = 0) const
      requires !detail::is_cubemap_type<Ty>;

    void get_subimage(T *data, size_t data_size, uint level = 0,
      ArrayXi size = ArrayXi::Zero(), ArrayXi offset = ArrayXi::Zero()) const
      requires !detail::is_cubemap_type<Ty>;

    void generate_mipmaps();

    /* 
      TO DO:
      - image() vs subimage()
      - copy()
      - fix zero() clear_subimage() or add clear()
      - set_param()
      - get_param()
      - generate_mipmaps()
    */

    /* function(...) implementations explicitly for cubemaps and cubemap Dimss */

    void set_image(const T* data, size_t data_size, uint face, uint level = 0)
      requires detail::is_cubemap_type<Ty>;

    void set_subimage(const T *data, size_t data_size, uint level = 0, uint face = 0,
      ArrayXi size = ArrayXi::Zero(), ArrayXi offset = ArrayXi::Zero())
      requires detail::is_cubemap_type<Ty>;

    void clear_subimage(const T *data, uint level = 0, uint face = 0,
      ArrayXi size = ArrayXi::Zero(), ArrayXi offset = ArrayXi::Zero())
      requires detail::is_cubemap_type<Ty>;

    /* miscellaneous */

    inline void swap(Texture &o) {
      using std::swap;
      Base::swap(o);
      swap(_levels, o._levels);
      swap(_size, o._size);
    }

    inline bool operator==(const Texture &o) const {
      using std::tie;
      return Base::operator==(o)
          && _levels == o._levels
          && _size.isApprox(o._size);
    }

    MET_NONCOPYABLE_CONSTR(Texture);
  };

  /* 
    A giant list of templated specialization names follows below.
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
    Shorthand specialization names for the default texture type follow below.
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
} // namespace metameric::gl