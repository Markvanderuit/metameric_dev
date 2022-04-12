#pragma once

#include <metameric/gl/detail/fwd.h>
#include <metameric/gl/enums.h>
#include <glad/glad.h>

namespace metameric::gl::detail {
  // Use template specializations to map corresponding texture formats at compile time
  template <uint Components, typename T> constexpr uint format_from_type();
  template <> constexpr uint format_from_type<1, ushort>()                    { return GL_RED_INTEGER; }
  template <> constexpr uint format_from_type<1, short>()                     { return GL_RED_INTEGER; }
  template <> constexpr uint format_from_type<1, uint>()                      { return GL_RED_INTEGER; }
  template <> constexpr uint format_from_type<1, int>()                       { return GL_RED_INTEGER; }
  template <> constexpr uint format_from_type<1, float>()                     { return GL_RED; }
  template <> constexpr uint format_from_type<2, ushort>()                    { return GL_RG_INTEGER; }
  template <> constexpr uint format_from_type<2, short>()                     { return GL_RG_INTEGER; }
  template <> constexpr uint format_from_type<2, uint>()                      { return GL_RG_INTEGER; }
  template <> constexpr uint format_from_type<2, int>()                       { return GL_RG_INTEGER; }
  template <> constexpr uint format_from_type<2, float>()                     { return GL_RG; }
  template <> constexpr uint format_from_type<3, ushort>()                    { return GL_RGB_INTEGER; }
  template <> constexpr uint format_from_type<3, short>()                     { return GL_RGB_INTEGER; }
  template <> constexpr uint format_from_type<3, uint>()                      { return GL_RGB_INTEGER; }
  template <> constexpr uint format_from_type<3, int>()                       { return GL_RGB_INTEGER; }
  template <> constexpr uint format_from_type<3, float>()                     { return GL_RGB; }
  template <> constexpr uint format_from_type<4, ushort>()                    { return GL_RGBA_INTEGER; }
  template <> constexpr uint format_from_type<4, short>()                     { return GL_RGBA_INTEGER; }
  template <> constexpr uint format_from_type<4, uint>()                      { return GL_RGBA_INTEGER; }
  template <> constexpr uint format_from_type<4, int>()                       { return GL_RGBA_INTEGER; }
  template <> constexpr uint format_from_type<4, float>()                     { return GL_RGBA; }
  template <> constexpr uint format_from_type<1, DepthComponent>()            { return GL_DEPTH_COMPONENT; }
  template <> constexpr uint format_from_type<1, StencilComponent>()          { return GL_STENCIL_INDEX; }

  // Use template specializations to map corresponding texture pixel types at compile time
  template <typename T> constexpr uint pixel_type_from_type();
  template <> constexpr uint pixel_type_from_type<ushort>()                  { return GL_UNSIGNED_SHORT; }
  template <> constexpr uint pixel_type_from_type<short>()                   { return GL_SHORT; }
  template <> constexpr uint pixel_type_from_type<uint>()                    { return GL_UNSIGNED_INT; }
  template <> constexpr uint pixel_type_from_type<int>()                     { return GL_INT; }
  template <> constexpr uint pixel_type_from_type<float>()                   { return GL_FLOAT; }
  template <> constexpr uint pixel_type_from_type<DepthComponent>()          { return GL_FLOAT; }
  template <> constexpr uint pixel_type_from_type<StencilComponent>()        { return GL_UNSIGNED_BYTE; }

  // Use template specializations to map corresponding texture internal formats at compile time
  template <uint Components, typename T> constexpr uint internal_format_from_type();
  template <> constexpr uint internal_format_from_type<1, ushort>()           { return GL_R16UI; }
  template <> constexpr uint internal_format_from_type<1, short>()            { return GL_R16I; }
  template <> constexpr uint internal_format_from_type<1, uint>()             { return GL_R32UI; }
  template <> constexpr uint internal_format_from_type<1, int>()              { return GL_R32I; }
  template <> constexpr uint internal_format_from_type<1, float>()            { return GL_R32F; }
  template <> constexpr uint internal_format_from_type<2, ushort>()           { return GL_RG16UI; }
  template <> constexpr uint internal_format_from_type<2, short>()            { return GL_RG16I; }
  template <> constexpr uint internal_format_from_type<2, uint>()             { return GL_RG32UI; }
  template <> constexpr uint internal_format_from_type<2, int>()              { return GL_RG32I; }
  template <> constexpr uint internal_format_from_type<2, float>()            { return GL_RG32F; }
  template <> constexpr uint internal_format_from_type<3, ushort>()           { return GL_RGB16UI; }
  template <> constexpr uint internal_format_from_type<3, short>()            { return GL_RGB16I; }
  template <> constexpr uint internal_format_from_type<3, uint>()             { return GL_RGB32UI; }
  template <> constexpr uint internal_format_from_type<3, int>()              { return GL_RGB32I; }
  template <> constexpr uint internal_format_from_type<3, float>()            { return GL_RGB32F; }
  template <> constexpr uint internal_format_from_type<4, ushort>()           { return GL_RGBA16UI; }
  template <> constexpr uint internal_format_from_type<4, short>()            { return GL_RGBA16I; }
  template <> constexpr uint internal_format_from_type<4, uint>()             { return GL_RGBA32UI; }
  template <> constexpr uint internal_format_from_type<4, int>()              { return GL_RGBA32I; }
  template <> constexpr uint internal_format_from_type<4, float>()            { return GL_RGBA32F; }
  template <> constexpr uint internal_format_from_type<1, DepthComponent>()   { return GL_DEPTH_COMPONENT32F; }
  template <> constexpr uint internal_format_from_type<1, StencilComponent>() { return GL_STENCIL_INDEX8; }

  // Use template specializations to map corresponding texture targets at compile time
  template <uint D, TextureType Ty> constexpr uint target_from_type();
  template <> constexpr uint target_from_type<1, TextureType::eBase>()              { return GL_TEXTURE_1D; }
  template <> constexpr uint target_from_type<2, TextureType::eBase>()              { return GL_TEXTURE_2D; }
  template <> constexpr uint target_from_type<3, TextureType::eBase>()              { return GL_TEXTURE_3D; }
  template <> constexpr uint target_from_type<1, TextureType::eArray>()             { return GL_TEXTURE_1D_ARRAY; }
  template <> constexpr uint target_from_type<2, TextureType::eArray>()             { return GL_TEXTURE_2D_ARRAY; }
  template <> constexpr uint target_from_type<2, TextureType::eCubemap>()           { return GL_TEXTURE_CUBE_MAP; }
  template <> constexpr uint target_from_type<2, TextureType::eCubemapArray>()      { return GL_TEXTURE_CUBE_MAP_ARRAY; }
  template <> constexpr uint target_from_type<2, TextureType::eMultisample>()       { return GL_TEXTURE_2D_MULTISAMPLE; }
  template <> constexpr uint target_from_type<2, TextureType::eMultisampleArray>()  { return GL_TEXTURE_2D_MULTISAMPLE_ARRAY; }

  // Use template specialization to map enum types for the five kinds of glTextureStorage*(...) functions at compile time.
  enum class StorageDimsType { e1D, e2D, e3D, e2DMultisample, e3DMultisample };
  template <uint D, TextureType Ty> constexpr StorageDimsType storage_dims_from_type();
  template <> constexpr StorageDimsType storage_dims_from_type<1, TextureType::eBase>()             { return StorageDimsType::e1D; }
  template <> constexpr StorageDimsType storage_dims_from_type<2, TextureType::eBase>()             { return StorageDimsType::e2D; }
  template <> constexpr StorageDimsType storage_dims_from_type<3, TextureType::eBase>()             { return StorageDimsType::e3D; }
  template <> constexpr StorageDimsType storage_dims_from_type<1, TextureType::eArray>()            { return StorageDimsType::e2D; }
  template <> constexpr StorageDimsType storage_dims_from_type<2, TextureType::eArray>()            { return StorageDimsType::e3D; }
  template <> constexpr StorageDimsType storage_dims_from_type<2, TextureType::eCubemap>()          { return StorageDimsType::e2D; }
  template <> constexpr StorageDimsType storage_dims_from_type<2, TextureType::eCubemapArray>()     { return StorageDimsType::e3D; }
  template <> constexpr StorageDimsType storage_dims_from_type<2, TextureType::eMultisample>()      { return StorageDimsType::e2DMultisample; }
  template <> constexpr StorageDimsType storage_dims_from_type<2, TextureType::eMultisampleArray>() { return StorageDimsType::e3DMultisample; }
} // metameric::gl::detail