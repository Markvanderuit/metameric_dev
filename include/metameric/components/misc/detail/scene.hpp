#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/components/misc/detail/texture_atlas.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <memory>

namespace gl {
  using Texture2d3fArray = gl::Texture2d<float, 3, gl::TextureType::eImageArray>;
  using Texture2d1fArray = gl::Texture2d<float, 1, gl::TextureType::eImageArray>;
} // namespace gl

namespace met::detail {
  /* Object information structure, detailing indices and values
      referring to the object's mesh shape and surface materials. */
  struct alignas(16) RTObjectInfo {
    alignas(16) eig::Matrix4f trf;
    alignas(16) eig::Matrix4f trf_inv;

    alignas(4)  bool          is_active;
    alignas(4)  uint          mesh_i;
    alignas(4)  uint          uplifting_i;

    alignas(4)  bool          is_albedo_sampled;
    alignas(4)  uint          albedo_i;
    alignas(16) Colr          albedo_v;
  };

  /* Mesh information structure, detailing which range of the mesh
     data buffers describes a specific mesh. */
  struct RTMeshInfo {
    alignas(4) uint verts_offs;
    alignas(4) uint verts_size;
    alignas(4) uint elems_offs;
    alignas(4) uint elems_size;
  };

  /* Texture information structure, detailing which range of the
     texture atlas describes a specific texture. */
  struct RTTextureInfo {
    alignas(4) bool         is_3f;
    alignas(4) uint         layer;
    alignas(8) eig::Array2u offs, size;
    alignas(8) eig::Array2f uv0, uv1;
  };

  struct RTTextureData {
    std::vector<RTTextureInfo> info;
    gl::Buffer                 info_gl;

    // Texture atlases to store all loaded image data in f32 format on the GL side
    std::vector<uint>          atlas_indices;
    TextureAtlas<float, 3>     atlas_3f;
    TextureAtlas<float, 1>     atlas_1f;
    
    static RTTextureData realize(Settings::TextureSize texture_size, std::span<const detail::Resource<Image>>);
    void update(std::span<const detail::Resource<Image>>);
  };
  
  /* Mesh vertex/element data block; holds all packed-together
     mesh data used in a scene. Should preferably be made once
     at scene load. */
  struct RTMeshData {
    std::vector<RTMeshInfo> info;
    gl::Buffer              info_gl;
    gl::Buffer verts_a;
    gl::Buffer verts_b;
    gl::Buffer elems;
    gl::Buffer elems_al;
    gl::Array  array;

    static RTMeshData realize(std::span<const detail::Resource<AlMeshData>>);
  };

  struct RTObjectData {
    std::vector<RTObjectInfo> info;
    gl::Buffer                info_gl;

    // Texture atlas to hold all uplifting-related barycentric data in f32 format
    std::vector<uint>         atlas_indices;
    TextureAtlas<float, 3>    atlas_3f;

    static RTObjectData realize(std::span<const detail::Component<Scene::Object>>);
    void update(std::span<const detail::Component<Scene::Object>>);
  };
} // namespace met::detail