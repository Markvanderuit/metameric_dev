#pragma once

#include <metameric/gl/detail/fwd.h>
#include <metameric/gl/detail/handle.h>
#include <span>

namespace metameric::gl {
  namespace detail {
    // Use template specialization to determine constructor parameter dimensions at compile time.
    template <uint D, TextureType Ty> consteval uint constr_dims();
    template <> consteval uint constr_dims<1, TextureType::eBase>()              { return 1; }
    template <> consteval uint constr_dims<2, TextureType::eBase>()              { return 2; }
    template <> consteval uint constr_dims<3, TextureType::eBase>()              { return 3; }
    template <> consteval uint constr_dims<1, TextureType::eArray>()             { return 2; }
    template <> consteval uint constr_dims<2, TextureType::eArray>()             { return 3; }
    template <> consteval uint constr_dims<2, TextureType::eCubemap>()           { return 2; }
    template <> consteval uint constr_dims<2, TextureType::eCubemapArray>()      { return 3; }
    template <> consteval uint constr_dims<2, TextureType::eMultisample>()       { return 2; }
    template <> consteval uint constr_dims<2, TextureType::eMultisampleArray>()  { return 3; }
   
    template <TextureType Ty>
    concept is_cubemap_type = Ty == TextureType::eCubemap || Ty == TextureType::eCubemapArray;
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
    using Array = eig::Array<int, detail::constr_dims<D, Ty>(), 1>;
    
  public:
    Array size;
    uint levels = 1u;
    std::span<T> data = { };
  };

  /**
   * Texture object. 
   * 
   * Supports 1d/2d/3d textures, 1d/2d texture arrays, 2d cubemaps, 2d cubemap arrays, 
   * 2d multisampled textures, and 2d multisampled arrays. A large collection of template 
   * specializations is accessible through names declared below.
   */
  template <typename T, uint D, uint Components, TextureType Ty>
  class Texture : public Handle<> {
    using Base = Handle<>;
    using Info = TextureCreateInfo<T, D, Ty>;
    using Array = eig::Array<int, detail::constr_dims<D, Ty>(), 1>;

    uint _levels;
    Array _size;

  public: 
    /* constr/destr */
    
    Texture() = default;
    Texture(Info info);
    ~Texture();

    /* getters/setters */

    inline uint levels() const { return _levels; }
    inline Array size() const { return _size; }

    /* state */

    void bind_to(uint index) const;

    /* operand implementation for most texture types */

    void get(std::span<T> data,
             uint level = 0,
             Array size = Array::Zero(),
             Array offset = Array::Zero()) const
             requires(!detail::is_cubemap_type<Ty>);

    void set(std::span<T> data,
             uint level = 0,
             Array size = Array::Zero(),
             Array offset = Array::Zero()) 
             requires(!detail::is_cubemap_type<Ty>);

    void clear(std::span<T> data = { }, 
               uint level = 0,
               Array size = Array::Zero(),
               Array offset = Array::Zero()) 
               requires(!detail::is_cubemap_type<Ty>);

    /* operand implementations for cubemap texture types */

    void get(std::span<T> data,
             uint face = 0,
             uint level = 0,
             Array size = Array::Zero(),
             Array offset = Array::Zero()) const
             requires(detail::is_cubemap_type<Ty>);
    
    void set(std::span<T> data,
             uint face = 0,
             uint level = 0,
             Array size = Array::Zero(),
             Array offset = Array::Zero()) 
             requires(detail::is_cubemap_type<Ty>);

    void clear(std::span<T> data = { }, 
               uint face = 0,
               uint level = 0,
               Array size = Array::Zero(),
               Array offset = Array::Zero()) 
               requires(detail::is_cubemap_type<Ty>);
    
    /* miscellaneous */

    void generate_mipmaps();

    inline void swap(Texture &o) {
      using std::swap;
      Base::swap(o);
      swap(_levels, o._levels);
      swap(_size, o._size);
    }

    inline bool operator==(const Texture &o) const {
      return Base::operator==(o) && _levels == o._levels && _size.isApprox(o._size);
    }

    MET_NONCOPYABLE_CONSTR(Texture);
  };
} // namespace metameric::gl