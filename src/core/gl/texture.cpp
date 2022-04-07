#include <metameric/core/gl/detail/enum.h>
#include <metameric/core/gl/detail/exception.h>
#include <metameric/core/gl/texture.h>
#include <glad/glad.h>
#include <iostream>

using namespace metameric;
using namespace metameric::gl;

constexpr EnumMap<TextureFormat, 28> _type_map({
  { TextureFormat::eR32UInt,  GL_UNSIGNED_INT }, { TextureFormat::eRG32UInt,  GL_UNSIGNED_INT },
  { TextureFormat::eR32Int,   GL_INT }, { TextureFormat::eRG32Int,   GL_INT },
  { TextureFormat::eR32Float, GL_FLOAT }, { TextureFormat::eRG32Float, GL_FLOAT },
  { TextureFormat::eRGB32UInt,  GL_UNSIGNED_INT }, { TextureFormat::eRGBA32UInt,  GL_UNSIGNED_INT },
  { TextureFormat::eRGB32Int,   GL_INT }, { TextureFormat::eRGBA32Int,   GL_INT },
  { TextureFormat::eRGB32Float, GL_FLOAT }, { TextureFormat::eRGBA32Float, GL_FLOAT },

  { TextureFormat::eR16UInt,  GL_UNSIGNED_SHORT }, { TextureFormat::eRG16UInt,  GL_UNSIGNED_SHORT },
  { TextureFormat::eR16Int,   GL_SHORT }, { TextureFormat::eRG16Int,   GL_SHORT },
  { TextureFormat::eR16Float, GL_HALF_FLOAT }, { TextureFormat::eRG16Float, GL_HALF_FLOAT },
  { TextureFormat::eRGB16UInt,  GL_UNSIGNED_SHORT }, { TextureFormat::eRGBA16UInt,  GL_UNSIGNED_SHORT },
  { TextureFormat::eRGB16Int,   GL_SHORT }, { TextureFormat::eRGBA16Int,   GL_SHORT },
  { TextureFormat::eRGB16Float, GL_HALF_FLOAT }, { TextureFormat::eRGBA16Float, GL_HALF_FLOAT },
  
  { TextureFormat::eDepth32, GL_FLOAT },
  { TextureFormat::eDepth24, GL_FLOAT },
  { TextureFormat::eDepth24Stencil8, GL_FLOAT },
  { TextureFormat::eStencil8, GL_FLOAT }
});

constexpr EnumMap<TextureFormat, 28> _format_map({
  { TextureFormat::eR32UInt,  GL_RED_INTEGER }, { TextureFormat::eRG32UInt,  GL_RG_INTEGER },
  { TextureFormat::eR32Int,   GL_RED_INTEGER }, { TextureFormat::eRG32Int,   GL_RG_INTEGER },
  { TextureFormat::eR32Float, GL_RED }, { TextureFormat::eRG32Float, GL_RG },
  { TextureFormat::eRGB32UInt,  GL_RGB_INTEGER }, { TextureFormat::eRGBA32UInt,  GL_RGBA_INTEGER },
  { TextureFormat::eRGB32Int,   GL_RGB_INTEGER }, { TextureFormat::eRGBA32Int,   GL_RGBA_INTEGER },
  { TextureFormat::eRGB32Float, GL_RGB }, { TextureFormat::eRGBA32Float, GL_RGBA },
  
  { TextureFormat::eR16UInt,  GL_RED_INTEGER }, { TextureFormat::eRG16UInt,  GL_RG_INTEGER },
  { TextureFormat::eR16Int,   GL_RED_INTEGER }, { TextureFormat::eRG16Int,   GL_RG_INTEGER },
  { TextureFormat::eR16Float, GL_RED }, { TextureFormat::eRG16Float, GL_RG },
  { TextureFormat::eRGB16UInt,  GL_RGB_INTEGER }, { TextureFormat::eRGBA16UInt,  GL_RGBA_INTEGER },
  { TextureFormat::eRGB16Int,   GL_RGB_INTEGER }, { TextureFormat::eRGBA16Int,   GL_RGBA_INTEGER },
  { TextureFormat::eRGB16Float, GL_RGB }, { TextureFormat::eRGBA16Float, GL_RGBA },

  { TextureFormat::eDepth32, GL_DEPTH_COMPONENT },
  { TextureFormat::eDepth24, GL_DEPTH_COMPONENT },
  { TextureFormat::eDepth24Stencil8, GL_DEPTH_COMPONENT },
  { TextureFormat::eStencil8, GL_STENCIL_INDEX }
});

constexpr EnumMap<TextureFormat, 28> _internal_format_map({
  { TextureFormat::eR32UInt,  GL_R32UI }, { TextureFormat::eRG32UInt,  GL_RG32UI },
  { TextureFormat::eR32Int,   GL_R32I  }, { TextureFormat::eRG32Int,   GL_RG32I  },
  { TextureFormat::eR32Float, GL_R32F  }, { TextureFormat::eRG32Float, GL_RG32F  },
  { TextureFormat::eRGB32UInt,  GL_RGB32UI }, { TextureFormat::eRGBA32UInt,  GL_RGBA32UI },
  { TextureFormat::eRGB32Int,   GL_RGB32I  }, { TextureFormat::eRGBA32Int,   GL_RGBA32I  },
  { TextureFormat::eRGB32Float, GL_RGB32F  }, { TextureFormat::eRGBA32Float, GL_RGBA32F  },

  { TextureFormat::eR16UInt,  GL_R16UI }, { TextureFormat::eRG16UInt,  GL_RG16UI },
  { TextureFormat::eR16Int,   GL_R16I  }, { TextureFormat::eRG16Int,   GL_RG16I  },
  { TextureFormat::eR16Float, GL_R16F  }, { TextureFormat::eRG16Float, GL_RG16F  },
  { TextureFormat::eRGB16UInt,  GL_RGB16UI }, { TextureFormat::eRGBA16UInt,  GL_RGBA16UI },
  { TextureFormat::eRGB16Int,   GL_RGB16I  }, { TextureFormat::eRGBA16Int,   GL_RGBA16I  },
  { TextureFormat::eRGB16Float, GL_RGB16F  }, { TextureFormat::eRGBA16Float, GL_RGBA16F  },

  { TextureFormat::eDepth32, GL_DEPTH_COMPONENT32F },
  { TextureFormat::eDepth24, GL_DEPTH_COMPONENT24 },
  { TextureFormat::eDepth24Stencil8, GL_DEPTH24_STENCIL8 },
  { TextureFormat::eStencil8, GL_STENCIL_INDEX8 }
});


Texture::Texture(TextureFormat format, Array dims, uint levels, const void *ptr)
: AbstractObject(true), _format(format), _dims(dims), _levels(levels) {
  guard(_is_init);
  runtime_assert(_dims.size() <= 3, 
    "Texture::Texture(...), specified dims argument exceeds supported dimensionality");
  
  auto internal_format = _internal_format_map[_format];
  switch (_dims.size()) {
    case 3:
      glCreateTextures(GL_TEXTURE_3D, 1, &_handle);
      glTextureStorage3D(_handle, _levels, internal_format, _dims.x(), _dims.y(), _dims.z());
      break;
    case 2:
      glCreateTextures(GL_TEXTURE_2D, 1, &_handle);
      glTextureStorage2D(_handle, _levels, internal_format, _dims.x(), _dims.y());
      break;
    case 1:
      glCreateTextures(GL_TEXTURE_1D, 1, &_handle);
      glTextureStorage1D(_handle, _levels, internal_format, _dims.x());
      break;
    default:
      runtime_assert(false, "Texture::Texture(...), _dims param is faulty");
  }

  gl_assert("Texture::Texture(...)");
}

Texture::~Texture() {
  guard(_is_init);
  glDeleteTextures(1, &_handle);
  gl_assert("Texture::~Texture(...)");
}

void Texture::set_image_mem(void const *ptr, size_t ptr_size, uint level, Array dims, Array off) {
  auto set_dims = dims.isZero() ? _dims : ArrayXi(dims);
  auto set_off = off.isZero() ? ArrayXi::Zero(_dims.size()) : ArrayXi(off);
  auto base_format = _format_map[_format];
  auto type = _type_map[_format];

  switch (_dims.size()) {
    case 3:
      glTextureSubImage3D(_handle, level, set_off.x(), set_off.y(), set_off.z(), set_dims.x(), set_dims.y(), set_dims.z(), base_format, type, ptr);
      break;
    case 2:
      glTextureSubImage2D(_handle, level, set_off.x(), set_off.y(), set_dims.x(), set_dims.y(), base_format, type, ptr);
      break;
    case 1:
      glTextureSubImage1D(_handle, level, set_off.x(), set_dims.x(), base_format, type, ptr);
      break;
    default:
      runtime_assert(false, "Texture::set_image(...), internal _dims param is faulty");
  }

  gl_assert("Texture::set_image(...)");
}

void Texture::get_image_mem(void *ptr, size_t ptr_size, uint level) const {
  auto base_format = _format_map[_format];
  auto base_type = _type_map[_format];
  glGetTextureImage(_handle, level, base_format, base_type, ptr_size, ptr);
  gl_assert("Texture::get_image(...)");
}

void Texture::copy_from(const Texture &o, uint level, Array dims, Array off) {
  
}


/* void Texture::get_subimage_mem(void *ptr, size_t ptr_size, uint level,  const eig::Ref<const ArrayXi> &dims, const eig::Ref<const ArrayXi> &off) const {
  auto set_dims = dims.isZero() ? _dims : ArrayXi(dims);
  auto set_off = off.isZero() ? ArrayXi::Zero(_dims.size()) : ArrayXi(off);
  auto base_format = _format_map[_format];
  auto type = _type_map[_format];

  uint n_dims = set_dims.size();
  
  // glBindTexture()
  glGetTextureSubImage(_handle, level, 
    set_off.x(), n_dims > 1 ? set_off.y() : 0, n_dims > 2 ? set_off.z() : 0,
    set_dims.x(), n_dims > 1 ? set_dims.y() : 1, n_dims > 2 ? set_dims.z() : 1,
    base_format, type, ptr_size, ptr); // TODO assert input texture size, instead of being an idiot
  gl_assert("Texture::get_subimage(...)");
} */


/*
  Attempt 2 below
*/

template <uint D, TextureType Ty>
constexpr
uint gl_get_texture_type() {
  if constexpr (Ty == TextureType::eDefault) {
    static_assert(D > 0 && D <= 3, "TextureType::eDefault, dimensionality must be > 0 || <= 3");
    switch (D) {
      case 1 : return GL_TEXTURE_1D;
      case 2 : return GL_TEXTURE_2D;
      case 3 : return GL_TEXTURE_3D;
    }
  } else if constexpr (Ty == TextureType::eArray) {
    static_assert(D > 0 && D <= 2, "TextureType::eArray, dimensionality must be > 0 || <= 2");
    switch (D) {
      case 1 : return GL_TEXTURE_1D_ARRAY;
      case 2 : return GL_TEXTURE_2D_ARRAY;
    }
  } else if constexpr (Ty == TextureType::eBuffer) {
    static_assert(D == 1, "TextureType::eBuffer, dimensionality must be 1");
    return GL_TEXTURE_BUFFER;
  } else if constexpr (Ty == TextureType::eCubemap) {
    static_assert(D == 2, "TextureType::eCubemap, dimensionality must be 2");
    return GL_TEXTURE_CUBE_MAP;
  } else if constexpr (Ty == TextureType::eCubemapArray) {
    static_assert(D == 2, "TextureType::eCubemapArray, dimensionality must be 2");
    return GL_TEXTURE_CUBE_MAP_ARRAY;
  } else if constexpr (Ty == TextureType::eMultisample) {
    // ...
  }
}

template <typename T, uint D, TextureType Ty>
AbstractTexture<T, D, Ty>::AbstractTexture(Array dims, uint levels = 1, T const *ptr)
: AbstractObject(true) {
  guard(_is_init);

}

template <typename T, uint D, TextureType Ty>
AbstractTexture<T, D, Ty>::~AbstractTexture() {
  guard(_is_init);
  glDeleteTextures(1, &_handle);
}

