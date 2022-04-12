#pragma once

#include <metameric/gl/detail/fwd.h>
#include <glad/glad.h>

namespace metameric::gl {
  enum class BufferTargets : uint {
    eAtomicCounter      = GL_ATOMIC_COUNTER_BUFFER,
    eShaderStorage      = GL_SHADER_STORAGE_BUFFER,
    eTransformFeedback  = GL_TRANSFORM_FEEDBACK_BUFFER,
    eUniform            = GL_UNIFORM_BUFFER
  };

  enum class BufferStorageFlags : uint {
    eNone               = 0u,
    eStorageDynamic     = GL_DYNAMIC_STORAGE_BIT,
    eStorageClient      = GL_CLIENT_STORAGE_BIT,
    eMapRead            = GL_MAP_READ_BIT,
    eMapWrite           = GL_MAP_WRITE_BIT,
    eMapPersistent      = GL_MAP_PERSISTENT_BIT,  
    eMapCoherent        = GL_MAP_COHERENT_BIT
  };
  
  enum class TextureType {
    eBase,
    eArray,
    eCubemap,
    eCubemapArray,
    eMultisample,
    eMultisampleArray
  };
  
  enum class SamplerMinFilter : uint {
    eNearest                = GL_NEAREST,
    eLinear                 = GL_LINEAR,
    eNearestMipmapNearest   = GL_NEAREST_MIPMAP_NEAREST,
    eLinearMipmapNearest    = GL_LINEAR_MIPMAP_NEAREST,
    eNearestMipmapLinear    = GL_NEAREST_MIPMAP_LINEAR,
    eLinearMipmapLinear     = GL_LINEAR_MIPMAP_LINEAR
  };
  
  enum class SamplerMagFilter : uint {
    eNearest                = GL_NEAREST,
    eLinear                 = GL_LINEAR
  };

  enum class SamplerWrap : uint {
    eRepeat                 = GL_REPEAT,
    eMirroredRepeat         = GL_MIRRORED_REPEAT,
    eClampToEdge            = GL_CLAMP_TO_EDGE,
    eClampToBorder          = GL_CLAMP_TO_BORDER
  };

  enum class SamplerCompareFunc : uint {
    eLessOrEqual            = GL_LEQUAL,
    eGreaterOrEqual         = GL_GEQUAL,
    eLess                   = GL_LESS,
    eGreater                = GL_GREATER,
    eEqual                  = GL_EQUAL,
    eNotEqual               = GL_NOTEQUAL,
    eAlways                 = GL_ALWAYS,
    eNever                  = GL_NEVER
  };

  enum class SamplerCompareMode : uint {
    eNone                   = GL_NONE,
    eCompare                = GL_COMPARE_REF_TO_TEXTURE
  };
} // metameric::gl