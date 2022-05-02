#include <metameric/gl/texture.h>
#include <metameric/gl/detail/assert.h>
#include <metameric/gl/detail/texture_spec.h>
#include <glad/glad.h>

namespace metameric::gl {
  template <typename T, uint D, uint Components, TextureType Ty>
  Texture<T, D, Components, Ty>::Texture(Info info)
  : Handle(true), _size(info.size), _levels(info.levels) {
    constexpr auto target = detail::target_from_type<D, Ty>();
    constexpr auto storage_dims_type = detail::storage_dims_from_type<D, Ty>();
    constexpr auto internal_format = detail::internal_format_from_type<Components, T>();
    
    guard(_is_init);
    glCreateTextures(target, 1, &_object);
    
    if constexpr (storage_dims_type == detail::StorageDimsType::e1D) {
      glTextureStorage1D(_object, _levels, internal_format, _size.x());
    } else if constexpr (storage_dims_type == detail::StorageDimsType::e2D) {
      glTextureStorage2D(_object, _levels, internal_format, _size.x(), _size.y());
    } else if constexpr (storage_dims_type == detail::StorageDimsType::e3D) {
      GLsizei size_z = detail::is_cubemap_type<Ty> ? _size.z() * 6 : _size.z(); // array cubemap
      glTextureStorage3D(_object, _levels, internal_format, _size.x(), _size.y(), size_z);
    } else if constexpr (storage_dims_type == detail::StorageDimsType::e2DMultisample) {
      glTextureStorage2DMultisample(_object, 4, internal_format, _size.x(), _size.y(), true);
    } else if constexpr (storage_dims_type == detail::StorageDimsType::e3DMultisample) {
      glTextureStorage3DMultisample(_object, 4, internal_format, _size.x(), _size.y(), _size.z(), true);
    }

    if (info.data.data()) {
      set(info.data);
      generate_mipmaps();
    }
  }

  template <typename T, uint D, uint Components, TextureType Ty>
  Texture<T, D, Components, Ty>::~Texture() {
    guard(_is_init);
    glDeleteTextures(1, &_object);
  }

  template <typename T, uint D, uint Components, TextureType Ty>
  void Texture<T, D, Components, Ty>::generate_mipmaps() {
    guard(_levels > 1);
    glGenerateTextureMipmap(_object);
  }

  /* 
    Non-cubemap function implementations follow
  */

  template <typename T, uint D, uint Components, TextureType Ty>
  void Texture<T, D, Components, Ty>::set(std::span<T> data, uint level, Array size, Array offset)
  requires(!detail::is_cubemap_type<Ty>) {
    constexpr auto storage_dims_type = detail::storage_dims_from_type<D, Ty>();
    constexpr auto format = detail::format_from_type<Components, T>();
    constexpr auto pixel_type = detail::pixel_type_from_type<T>();
    const Array safe_size = (size == 0).all() ? _size : size;

    if constexpr (storage_dims_type == detail::StorageDimsType::e1D) {
      glTextureSubImage1D(_object, level, 
        offset.x(), 
        safe_size.x(), 
        format, pixel_type, data.data());
    } else if constexpr (storage_dims_type == detail::StorageDimsType::e2D
                      || storage_dims_type == detail::StorageDimsType::e2DMultisample) {
      glTextureSubImage2D(_object, level, 
        offset.x(), offset.y(), 
        safe_size.x(), safe_size.y(), 
        format, pixel_type, data.data());              
    } else if constexpr (storage_dims_type == detail::StorageDimsType::e3D
                      || storage_dims_type == detail::StorageDimsType::e3DMultisample) {
      glTextureSubImage3D(_object, level, 
        offset.x(), offset.y(), offset.z(),
        safe_size.x(), safe_size.y(), safe_size.z(),
        format, pixel_type, data.data());
    }
  }

  template <typename T, uint D, uint Components, TextureType Ty>
  void Texture<T, D, Components, Ty>::get(std::span<T> data, uint level, Array size, Array offset) const
  requires(!detail::is_cubemap_type<Ty>) {
    constexpr auto storage_dims_type = detail::storage_dims_from_type<D, Ty>();
    constexpr auto format = detail::format_from_type<Components, T>();
    constexpr auto pixel_type = detail::pixel_type_from_type<T>();
    const Array safe_size = (size == 0).all() ? _size : size;

    if constexpr (storage_dims_type == detail::StorageDimsType::e1D) {
      glGetTextureSubImage(_object, level, 
        offset.x(), 0, 0, 
        safe_size.x(), 1, 1, 
        format, pixel_type, data.size_bytes(), data.data());
    } else if constexpr (storage_dims_type == detail::StorageDimsType::e2D
                      || storage_dims_type == detail::StorageDimsType::e2DMultisample) {
      glGetTextureSubImage(_object, level, 
        offset.x(), offset.y(), 0, 
        safe_size.x(), safe_size.y(), 1, 
        format, pixel_type, data.size_bytes(), data.data());
    } else if constexpr (storage_dims_type == detail::StorageDimsType::e3D
                      || storage_dims_type == detail::StorageDimsType::e3DMultisample) {
      glGetTextureSubImage(_object, level, 
        offset.x(), offset.y(), offset.z(), 
        safe_size.x(), safe_size.y(), safe_size.z(), 
        format, pixel_type, data.size_bytes(), data.data());
    }
  }

  template <typename T, uint D, uint Components, TextureType Ty>
  void Texture<T, D, Components, Ty>::clear(std::span<T> data, uint level, Array size, Array offset)
  requires(!detail::is_cubemap_type<Ty>) {
    constexpr auto storage_dims_type = detail::storage_dims_from_type<D, Ty>();
    constexpr auto format = detail::format_from_type<Components, T>();
    constexpr auto pixel_type = detail::pixel_type_from_type<T>();
    const Array safe_size = (size == 0).all() ? _size : size;

    if constexpr (storage_dims_type == detail::StorageDimsType::e1D) {
      glClearTexSubImage(_object, level, 
        offset.x(), 0, 0, 
        safe_size.x(), 1, 1, 
        format, pixel_type, data.data());
    } else if constexpr (storage_dims_type == detail::StorageDimsType::e2D
                      || storage_dims_type == detail::StorageDimsType::e2DMultisample) {
      glClearTexSubImage(_object, level, 
        offset.x(), offset.y(), 0, 
        safe_size.x(), safe_size.y(), 1, 
        format, pixel_type, data.data());
    } else if constexpr (storage_dims_type == detail::StorageDimsType::e3D
                      || storage_dims_type == detail::StorageDimsType::e3DMultisample) {
      glClearTexSubImage(_object, level, 
        offset.x(), offset.y(), offset.z(), 
        safe_size.x(), safe_size.y(), safe_size.z(), 
        format, pixel_type, data.data());
    }
  }

  /* 
    Cubemap function implementations follow
  */

  template <typename T, uint D, uint Components, TextureType Ty>
  void Texture<T, D, Components, Ty>::set(std::span<T> data, uint face, uint level, Array size, Array offset)
  requires(detail::is_cubemap_type<Ty>) {
    constexpr auto storage_dims_type = detail::storage_dims_from_type<D, Ty>();
    constexpr auto format = detail::format_from_type<Components, T>();
    constexpr auto pixel_type = detail::pixel_type_from_type<T>();
    const Array safe_size = (size == 0).all() ? _size : size;

    if constexpr (storage_dims_type == detail::StorageDimsType::e2D) {
      glTextureSubImage3D(_object, level, 
      offset.x(), offset.y(), face, 
      safe_size.x(), safe_size.y(), 1, 
      format, pixel_type, data.data());
    } else if constexpr (storage_dims_type == detail::StorageDimsType::e3D) {
      glTextureSubImage3D(_object, level, 
      offset.x(), offset.y(), safe_size.z() * face, 
      safe_size.x(), safe_size.y(), safe_size.z(), 
      format, pixel_type, data.data());
    }
  }

  template <typename T, uint D, uint Components, TextureType Ty>
  void Texture<T, D, Components, Ty>::get(std::span<T> data, uint face, uint level, Array size, Array offset) const 
  requires(detail::is_cubemap_type<Ty>) {
    constexpr auto storage_dims_type = detail::storage_dims_from_type<D, Ty>();
    constexpr auto format = detail::format_from_type<Components, T>();
    constexpr auto pixel_type = detail::pixel_type_from_type<T>();
    const Array safe_size = (size == 0).all() ? _size : size;
    
    if constexpr (storage_dims_type == detail::StorageDimsType::e2D) {
      glGetTextureSubImage(_object, level, 
        offset.x(), offset.y(), face, 
        safe_size.x(), safe_size.y(), 1, 
        format, pixel_type, data.size_bytes(), data.data());
    } else if constexpr (storage_dims_type == detail::StorageDimsType::e3D) {
      glGetTextureSubImage(_object, level, 
        offset.x(), offset.y(), offset.z() * face, 
        safe_size.x(), safe_size.y(), safe_size.z(), 
        format, pixel_type, data.size_bytes(), data.data());
    }
  }

  template <typename T, uint D, uint Components, TextureType Ty>
  void Texture<T, D, Components, Ty>::clear(std::span<T> data, uint face, uint level, Array size, Array offset)
  requires(detail::is_cubemap_type<Ty>) {
    constexpr auto storage_dims_type = detail::storage_dims_from_type<D, Ty>();
    constexpr auto format = detail::format_from_type<Components, T>();
    constexpr auto pixel_type = detail::pixel_type_from_type<T>();
    const Array safe_size = (size == 0).all() ? _size : size;
    
    if constexpr (storage_dims_type == detail::StorageDimsType::e2D) {
      glClearTexSubImage(_object, level,
        offset.x(), offset.y(), face,
        safe_size.x(), safe_size.y(), 1,
        format, pixel_type, data.data());
    } else if constexpr (storage_dims_type == detail::StorageDimsType::e3D) {
      glClearTexSubImage(_object, level,
        offset.x(), offset.y(), offset.z() * face,
        safe_size.x(), safe_size.y(), safe_size.z(),
        format, pixel_type, data.data());
    }
  }
  
  template <typename T, uint D, uint Components, TextureType Ty>
  void Texture<T, D, Components, Ty>::bind_to(uint index) const {
    glBindTextureUnit(index, _object);
  }
  
  /* 
    Explicit template instantiations follow.
  */

  #define MET_TEXTURE_INST_FOR(type, dims, components, texture_type)\
    template class Texture<type, dims, components, texture_type>;

  #define MET_TEXTURE_INST_COMPONENTS_1(type, dims, texture_type)\
    MET_TEXTURE_INST_FOR(type, dims, 1, texture_type)

  #define MET_TEXTURE_INST_COMPONENTS_1_2(type, dims, texture_type)\
    MET_TEXTURE_INST_COMPONENTS_1(type, dims, texture_type)\
    MET_TEXTURE_INST_FOR(type, dims, 2, texture_type)

  #define MET_TEXTURE_INST_COMPONENTS_1_2_3(type, dims, texture_type)\
    MET_TEXTURE_INST_COMPONENTS_1_2(type, dims, texture_type)\
    MET_TEXTURE_INST_FOR(type, dims, 3, texture_type)

  #define MET_TEXTURE_INST_COMPONENTS_1_2_3_4(type, dims, texture_type)\
    MET_TEXTURE_INST_COMPONENTS_1_2_3(type, dims, texture_type)\
    MET_TEXTURE_INST_FOR(type, dims, 4, texture_type)

  #define MET_TEXTURE_INST_DIMS_1_2_3_special(type, texture_type)\
    MET_TEXTURE_INST_COMPONENTS_1(type, 1, texture_type)\
    MET_TEXTURE_INST_COMPONENTS_1(type, 2, texture_type)\
    MET_TEXTURE_INST_COMPONENTS_1(type, 3, texture_type)

  #define MET_TEXTURE_INST_DIMS_1_2_rgba(type, texture_type)\
    MET_TEXTURE_INST_COMPONENTS_1_2_3_4(type, 1, texture_type)\
    MET_TEXTURE_INST_COMPONENTS_1_2_3_4(type, 2, texture_type)

  #define MET_TEXTURE_INST_DIMS_1_2_3_rgba(type, texture_type)\
    MET_TEXTURE_INST_DIMS_1_2_rgba(type, texture_type)\
    MET_TEXTURE_INST_COMPONENTS_1_2_3_4(type, 3, texture_type)

  #define MET_TEXTURE_INST_TYPES_2d_rgba(texture_type)\
    MET_TEXTURE_INST_COMPONENTS_1_2_3_4(ushort, 2, texture_type)\
    MET_TEXTURE_INST_COMPONENTS_1_2_3_4(short, 2, texture_type)\
    MET_TEXTURE_INST_COMPONENTS_1_2_3_4(uint, 2, texture_type)\
    MET_TEXTURE_INST_COMPONENTS_1_2_3_4(int, 2, texture_type)\
    MET_TEXTURE_INST_COMPONENTS_1_2_3_4(float, 2, texture_type)

  MET_TEXTURE_INST_DIMS_1_2_3_rgba(ushort, TextureType::eBase)
  MET_TEXTURE_INST_DIMS_1_2_3_rgba(short, TextureType::eBase)
  MET_TEXTURE_INST_DIMS_1_2_3_rgba(uint, TextureType::eBase)
  MET_TEXTURE_INST_DIMS_1_2_3_rgba(int, TextureType::eBase)
  MET_TEXTURE_INST_DIMS_1_2_3_rgba(float, TextureType::eBase)

  MET_TEXTURE_INST_DIMS_1_2_rgba(ushort, TextureType::eArray)
  MET_TEXTURE_INST_DIMS_1_2_rgba(short, TextureType::eArray)
  MET_TEXTURE_INST_DIMS_1_2_rgba(uint, TextureType::eArray)
  MET_TEXTURE_INST_DIMS_1_2_rgba(int, TextureType::eArray)
  MET_TEXTURE_INST_DIMS_1_2_rgba(float, TextureType::eArray)

  MET_TEXTURE_INST_TYPES_2d_rgba(TextureType::eCubemap)
  MET_TEXTURE_INST_TYPES_2d_rgba(TextureType::eCubemapArray)
  MET_TEXTURE_INST_TYPES_2d_rgba(TextureType::eMultisample)
  MET_TEXTURE_INST_TYPES_2d_rgba(TextureType::eMultisampleArray)

  MET_TEXTURE_INST_DIMS_1_2_3_special(gl::DepthComponent, TextureType::eBase)
  MET_TEXTURE_INST_COMPONENTS_1(gl::DepthComponent, 1, TextureType::eArray)
  MET_TEXTURE_INST_COMPONENTS_1(gl::DepthComponent, 2, TextureType::eArray)
  MET_TEXTURE_INST_COMPONENTS_1(gl::DepthComponent, 2, TextureType::eCubemap)
  MET_TEXTURE_INST_COMPONENTS_1(gl::DepthComponent, 2, TextureType::eCubemapArray)
  MET_TEXTURE_INST_COMPONENTS_1(gl::DepthComponent, 2, TextureType::eMultisample)
  MET_TEXTURE_INST_COMPONENTS_1(gl::DepthComponent, 2, TextureType::eMultisampleArray)

  MET_TEXTURE_INST_DIMS_1_2_3_special(gl::StencilComponent, TextureType::eBase)
  MET_TEXTURE_INST_COMPONENTS_1(gl::StencilComponent, 1, TextureType::eArray)
  MET_TEXTURE_INST_COMPONENTS_1(gl::StencilComponent, 2, TextureType::eArray)
  MET_TEXTURE_INST_COMPONENTS_1(gl::StencilComponent, 2, TextureType::eCubemap)
  MET_TEXTURE_INST_COMPONENTS_1(gl::StencilComponent, 2, TextureType::eCubemapArray)
  MET_TEXTURE_INST_COMPONENTS_1(gl::StencilComponent, 2, TextureType::eMultisample)
  MET_TEXTURE_INST_COMPONENTS_1(gl::StencilComponent, 2, TextureType::eMultisampleArray)
} // namespace metameric::gl
