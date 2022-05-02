#pragma once

#include <metameric/core/define.h>
#include <glad/glad.h>

namespace metameric::gl {
  /*
    Buffer enums
  */

  // Binding target for gl::Buffer::bind_to(...)
  enum class BufferTargetType : uint {
    eAtomicCounter      = GL_ATOMIC_COUNTER_BUFFER,
    eShaderStorage      = GL_SHADER_STORAGE_BUFFER,
    eTransformFeedback  = GL_TRANSFORM_FEEDBACK_BUFFER,
    eUniform            = GL_UNIFORM_BUFFER
  };

  // Storage flags for gl::Buffer(...) construction
  enum class BufferStorageFlags : uint {
    eStorageDynamic     = GL_DYNAMIC_STORAGE_BIT,
    eStorageClient      = GL_CLIENT_STORAGE_BIT,
    eMapRead            = GL_MAP_READ_BIT,
    eMapWrite           = GL_MAP_WRITE_BIT,
    eMapPersistent      = GL_MAP_PERSISTENT_BIT,  
    eMapCoherent        = GL_MAP_COHERENT_BIT
  };
  MET_DECLARE_BITFLAG(BufferStorageFlags);

  // Access flags for gl::Buffer::map(...)
  enum class BufferAccessFlags : uint {
    eMapRead            = GL_MAP_READ_BIT,
    eMapWrite           = GL_MAP_WRITE_BIT,
    eMapInvalidate      = GL_MAP_INVALIDATE_RANGE_BIT,
    eMapPersistent      = GL_MAP_PERSISTENT_BIT,  
    eMapCoherent        = GL_MAP_COHERENT_BIT
  };
  MET_DECLARE_BITFLAG(BufferAccessFlags);

  /*
    Draw and state enums
  */

  //  Draw capabilities for gl::state::set(...)/get(...)
  enum class DrawCapability : uint {
    // Misc capabilities
    eCullFace           = GL_CULL_FACE,
    eFramebufferSRGB    = GL_FRAMEBUFFER_SRGB,
    eMSAA               = GL_MULTISAMPLE,
    eDebugOutput        = GL_DEBUG_OUTPUT,
    eDebugOutputSync    = GL_DEBUG_OUTPUT_SYNCHRONOUS,

    // Blending capabilities
    eBlendOp            = GL_BLEND,
    eLogicOp            = GL_COLOR_LOGIC_OP,

    // Depth/stencil/scissor test capabilities
    eDepthClamp         = GL_DEPTH_CLAMP,
    eDepthTest          = GL_DEPTH_TEST,
    eStencilTest        = GL_STENCIL_TEST,
    eScissorTest        = GL_SCISSOR_TEST,

    // Smoothing capabilities
    eLineSmooth         = GL_LINE_SMOOTH,
    ePolySmooth         = GL_POLYGON_SMOOTH,
  };

  // Blend operations for gl::state::set_op(...)
  enum class BlendOp : uint {
    eZero               = GL_ZERO,
    eOne                = GL_ONE,
    eSrcColor           = GL_SRC_COLOR,
    eOneMinusSrcColor   = GL_ONE_MINUS_SRC_COLOR,
    eDstColor           = GL_DST_COLOR,
    eOneMinusDstColor   = GL_ONE_MINUS_DST_COLOR,
    eSrcAlpha           = GL_SRC_ALPHA,
    eOneMinusSrcAlpha   = GL_ONE_MINUS_SRC_ALPHA,
    eDstAlpha           = GL_DST_ALPHA,
    eOneMinusDstAlpha   = GL_ONE_MINUS_DST_ALPHA,
    eConstColor         = GL_CONSTANT_COLOR,
    eOneMinusConstColor = GL_ONE_MINUS_CONSTANT_COLOR,
    eConstAlpha         = GL_CONSTANT_ALPHA,
    eOneMinusConstAlpha = GL_ONE_MINUS_CONSTANT_ALPHA,
    eSrcAlphaSaturate   = GL_SRC_ALPHA_SATURATE,
    eSrc1Color          = GL_SRC1_COLOR,
    eOneMinusSrc1Color  = GL_ONE_MINUS_SRC1_COLOR,
    eSrc1Alpha          = GL_SRC1_ALPHA,
    eOneMinusSrc1Alpha  = GL_ONE_MINUS_SRC1_ALPHA
  };

  // Logic operations for gl::state::set_op(...)
  enum class LogicOp : uint {
    eClear              = GL_CLEAR,
    eSet                = GL_SET,
    eCopy               = GL_COPY,
    eCopyInverted       = GL_COPY_INVERTED,
    eNoop               = GL_NOOP,
    eInvert             = GL_INVERT,
    eAnd                = GL_AND,
    eNand               = GL_NAND,
    eOr                 = GL_OR,
    eNor                = GL_NOR,
    eXor                = GL_XOR,
    eEquiv              = GL_EQUIV,
    eAndReverse         = GL_AND_REVERSE,
    eAndInverse         = GL_AND_INVERTED,
    eOrReverse          = GL_OR_REVERSE,
    eOrInverse          = GL_OR_INVERTED,
  };

  // Primitive types for gl::draw(...)
  enum class PrimitiveType : uint{
    ePoints             = GL_POINTS,
    eLines              = GL_LINES,
    eTriangles          = GL_TRIANGLES,
    ePatches            = GL_PATCHES,

    eLineStrip          = GL_LINE_STRIP,
    eLineLoop           = GL_LINE_LOOP,
    eLinesAdj           = GL_LINES_ADJACENCY,
    eLineStripAdj       = GL_LINE_STRIP_ADJACENCY,

    eTriangleStrip      = GL_TRIANGLE_STRIP,
    eTriangleFan        = GL_TRIANGLE_FAN,
    eTrianglesAdj       = GL_TRIANGLES_ADJACENCY,
    eTriangleStripAdj   = GL_TRIANGLE_STRIP_ADJACENCY,
  };


  /*
    Framebuffer enums
  */

  // Attachment types for gl::Framebuffer(...) construction
  // and for gl::Framebuffer::clear(...) targets
 enum class FramebufferAttachmentType : uint {
   eColor               = GL_COLOR,
   eDepth               = GL_DEPTH,
   eStencil             = GL_STENCIL
 };


  /*
    Shader enums
  */

  // Created type for gl::Program(...) internal construction
  enum class ShaderType : uint {
    eCompute            = GL_COMPUTE_SHADER,
    eVertex             = GL_VERTEX_SHADER,
    eGeometry           = GL_GEOMETRY_SHADER,
    eFragment           = GL_FRAGMENT_SHADER,
    eTesselationEval    = GL_TESS_EVALUATION_SHADER,
    eTesselationCtrl    = GL_TESS_CONTROL_SHADER
  };


  /*
    Texture enums
  */
  
  // Created type for gl::Texture<...>(...) construction
  enum class TextureType {
    eBase,
    eArray,
    eCubemap,
    eCubemapArray,
    eMultisample,
    eMultisampleArray
  };


  /*
    Sampler enums
  */
  
  // Filter used for minimization in gl::Sampler
  enum class SamplerMinFilter : uint {
    eNearest                = GL_NEAREST,
    eLinear                 = GL_LINEAR,
    eNearestMipmapNearest   = GL_NEAREST_MIPMAP_NEAREST,
    eLinearMipmapNearest    = GL_LINEAR_MIPMAP_NEAREST,
    eNearestMipmapLinear    = GL_NEAREST_MIPMAP_LINEAR,
    eLinearMipmapLinear     = GL_LINEAR_MIPMAP_LINEAR
  };
  
  // Filter used for magnification in gl::Sampler
  enum class SamplerMagFilter : uint {
    eNearest                = GL_NEAREST,
    eLinear                 = GL_LINEAR
  };

  // Technique used for wrapping in gl::Sampler
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


  /*
    Sync enums
  */

  // Barrier types for gl::memory_barrier(...)
  enum class BarrierFlags : uint {
    // Data sourced from element buffers reflects shader writes prior to barrier
    eElementArray           = GL_ELEMENT_ARRAY_BARRIER_BIT,

    // Data sourced from attrib buffers reflects shader writes prior to barrier
    eVertexAttribArray      = GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT,

    // Operations on framebuffer attachments reflect shader writes prior to barrier
    eFramebuffer            = GL_FRAMEBUFFER_BARRIER_BIT,

    // Writes via transform feedback reflect shader writes prior to barrier
    eTransformFeedback      = GL_TRANSFORM_FEEDBACK_BARRIER_BIT,

    // Data sourced from textures reflects shader writes prior to barrier
    eTextureFetch           = GL_TEXTURE_FETCH_BARRIER_BIT,

    // Operations using set*/get*/copy* reflect shader writes prior to barrier
    eTextureUpdate          = GL_TEXTURE_UPDATE_BARRIER_BIT,

    // Shader image load/store/atomics reflect shader writes prior to barrier
    eShaderImageAccess      = GL_SHADER_IMAGE_ACCESS_BARRIER_BIT,

    // Operations using set*/get*/copy*/mapped memory reflect shader writes prior to barrier
    eBufferUpdate           = GL_BUFFER_UPDATE_BARRIER_BIT,

    // Client operations on persistent mapped buffers reflect shader writes prior to barrier
    eClientMappedBuffer     = GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT,

    // Data sourced from bound buffer targets reflects shader writes prior to barrier
    eAtomicCounterBuffer    = GL_ATOMIC_COUNTER_BARRIER_BIT,
    eIndirectBuffer         = GL_COMMAND_BARRIER_BIT,
    eShaderStorageBuffer    = GL_SHADER_STORAGE_BARRIER_BIT,
    ePixelBuffer            = GL_PIXEL_BUFFER_BARRIER_BIT,
    eUniformBuffer          = GL_UNIFORM_BARRIER_BIT,
    eQueryBuffer            = GL_QUERY_BUFFER_BARRIER_BIT,
  };
  MET_DECLARE_BITFLAG(BarrierFlags);

  /*
    Vertexarray enums
  */

  //  Format used for gl::VertexArray(...) in gl::VertexAttribInfo(...) object
  enum class VertexFormatType : uint {
    eByte                   = GL_BYTE,
    eUByte                  = GL_UNSIGNED_BYTE,
    eInt                    = GL_INT,
    eUInt                   = GL_UNSIGNED_INT,
    eFloat                  = GL_FLOAT
  };

  // Size used for gl::VertexArray(...) in gl::VertexAttribInfo(...) object
  enum class VertexFormatSize : uint {
    e1                      = 1,
    e2                      = 2,
    e3                      = 3,
    e4                      = 4
  };


  /*
    Window/context enums
  */

  // Window hint flags to pass to GLFW
  enum class WindowFlags : uint {
    eDebug                  = 0x001u,
    eDecorated              = 0x002u,
    eFloating               = 0x004u,
    eFullscreen             = 0x008u,
    eFocused                = 0x010u,
    eMaximized              = 0x020u,
    eVisible                = 0x040u,
    eResizable              = 0x080u,
    eSRGB                   = 0x100u,
    eMSAA                   = 0x200u
  };  
  MET_DECLARE_BITFLAG(WindowFlags);

  // Preferred OpenGL profile for GLFW to support
  enum class ProfileType {
    eAny,
    eCore,
    eCompatibility,
  };
} // metameric::gl