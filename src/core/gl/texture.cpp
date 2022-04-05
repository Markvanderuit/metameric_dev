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

  { TextureFormat::eR16UInt,  GL_UNSIGNED_INT }, { TextureFormat::eRG16UInt,  GL_UNSIGNED_INT },
  { TextureFormat::eR16Int,   GL_INT }, { TextureFormat::eRG16Int,   GL_INT },
  { TextureFormat::eR16Float, GL_FLOAT }, { TextureFormat::eRG16Float, GL_FLOAT },
  { TextureFormat::eRGB16UInt,  GL_UNSIGNED_INT }, { TextureFormat::eRGBA16UInt,  GL_UNSIGNED_INT },
  { TextureFormat::eRGB16Int,   GL_INT }, { TextureFormat::eRGBA16Int,   GL_INT },
  { TextureFormat::eRGB16Float, GL_FLOAT }, { TextureFormat::eRGBA16Float, GL_FLOAT },
  
  { TextureFormat::eDepth32, GL_FLOAT },
  { TextureFormat::eDepth24, GL_FLOAT },
  { TextureFormat::eDepth24Stencil8, GL_FLOAT },
  { TextureFormat::eStencil8, GL_FLOAT }
});

constexpr EnumMap<TextureFormat, 28> _format_map({
  { TextureFormat::eR32UInt,  GL_RED }, { TextureFormat::eRG32UInt,  GL_RG },
  { TextureFormat::eR32Int,   GL_RED }, { TextureFormat::eRG32Int,   GL_RG },
  { TextureFormat::eR32Float, GL_RED }, { TextureFormat::eRG32Float, GL_RG },
  { TextureFormat::eRGB32UInt,  GL_RGB }, { TextureFormat::eRGBA32UInt,  GL_RGBA },
  { TextureFormat::eRGB32Int,   GL_RGB }, { TextureFormat::eRGBA32Int,   GL_RGBA },
  { TextureFormat::eRGB32Float, GL_RGB }, { TextureFormat::eRGBA32Float, GL_RGBA },
  
  { TextureFormat::eR16UInt,  GL_RED }, { TextureFormat::eRG16UInt,  GL_RG },
  { TextureFormat::eR16Int,   GL_RED }, { TextureFormat::eRG16Int,   GL_RG },
  { TextureFormat::eR16Float, GL_RED }, { TextureFormat::eRG16Float, GL_RG },
  { TextureFormat::eRGB16UInt,  GL_RGB }, { TextureFormat::eRGBA16UInt,  GL_RGBA },
  { TextureFormat::eRGB16Int,   GL_RGB }, { TextureFormat::eRGBA16Int,   GL_RGBA },
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


Texture::Texture(TextureFormat format, uint levels, VectorXi dims)
: AbstractObject(true), _format(format), _levels(levels) {
  guard(_is_init);
  runtime_assert(dims.size() <= 3, 
    "Texture::Texture(...), specified dims argument exceeds supported dimensionality");

  std::cout << dims.size() << std::endl;
  std::cout << dims.x() << std::endl;
  std::cout << dims.y() << std::endl;
  
  gl_assert("Texture::Texture(...)");
}

Texture::Texture(TextureFormat format, uint levels, uint w, uint h, uint d)
: AbstractObject(true), _format(format), _levels(levels), _w(w), _h(h), _d(d) {
  guard(_is_init);

  auto _dc = { _w, _h, _d };
  _dims = std::count_if(_dc.begin(), _dc.end(), [](uint i) { return i > 1; });

  auto internal_format = _internal_format_map[_format];
  switch (_dims) {
    case 3:
      glCreateTextures(GL_TEXTURE_3D, 1, &_handle);
      glTextureStorage3D(_handle, _levels, internal_format, _w, _h, _d);
      break;
    case 2:
      glCreateTextures(GL_TEXTURE_2D, 1, &_handle);
      glTextureStorage2D(_handle, _levels, internal_format, _w, _h);
      break;
    case 1:
      glCreateTextures(GL_TEXTURE_1D, 1, &_handle);
      glTextureStorage1D(_handle, _levels, internal_format, _w);
      break;
    default:
      runtime_assert(false, "Texture dimensionality is faulty");
  }
  
  gl_assert("Texture::Texture(...)");
}

/* Texture::Texture(TextureFormat format, const void *ptr, uint levels, uint w, uint h, uint d)
: AbstractObject(true), _format(format), _w(w), _h(h), _d(d) {
  guard(_is_init);

  auto _dc = { _w, _h, _d };
  _dims = std::count_if(_dc.begin(), _dc.end(),[](uint i) { return i > 1; });

  auto internal_format = _internal_format_map[_format];
  auto base_format = _format_map[_format];
  auto type = _type_map[_format];
  switch (_dims) {
    case 3:
      glCreateTextures(GL_TEXTURE_3D, 1, &_handle);
      glTextureStorage3D(_handle, _levels, internal_format, _w, _h, _d);
      for (uint i = 0; i < levels; ++i) {
        glTextureSubImage3D(_handle, i, 0, 0, 0, _w, _h, _d, base_format, type, ptr);
      }
      break;
    case 2:
      glCreateTextures(GL_TEXTURE_2D, 1, &_handle);
      glTextureStorage2D(_handle, _levels, internal_format, _w, _h);
      for (uint i = 0; i < levels; ++i) {
        glTextureSubImage2D(_handle, i, 0, 0, _w, _h, base_format, type, ptr);
      }
      break;
    case 1:
      glCreateTextures(GL_TEXTURE_1D, 1, &_handle);
      glTextureStorage1D(_handle, _levels, internal_format, _w);
      for (uint i = 0; i < levels; ++i) {
        glTextureSubImage1D(_handle, i, 0, _w, base_format, type, ptr);
      }
      break;
    default:
      runtime_assert(false, "Texture dimensionality is faulty");
  }

  gl_assert("Texture::Texture(...)");  
} */

Texture::~Texture() {
  guard(_is_init);
  glDeleteTextures(1, &_handle);
  gl_assert("Texture::~Texture(...)");
}