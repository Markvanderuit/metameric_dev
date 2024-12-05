// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/image.hpp>
#include <metameric/core/detail/packing.hpp>
#include <metameric/scene/detail/atlas.hpp>
#include <metameric/scene/detail/bvh.hpp>
#include <metameric/scene/detail/utility.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/texture.hpp>

namespace met::detail {
  // Template specialization of SceneGLHandler.
  // Handles packed mesh buffers, blas bvh buffers, and info to unpack said buffers shader-side
  template <>
  struct SceneGLHandler<met::Mesh> : public SceneGLHandlerBase {
    // All cpu-side data is cached per mesh resource; meshes are simplified to the 
    // maximum BVH size and transformed to fit a unit cube before the BVH is computed
    struct MeshData {
      Mesh          mesh;
      BVH<8>        bvh;
      eig::Matrix4f unit_trf;   // Transform to undo mesh' packing into a unit cube
      uint          prims_offs; // Offset/extent into blas_prims buffer
      uint          nodes_offs; // Offset/extent into blas_nodes buffer
    };

    // Cache of processed mesh data and associated BLAS
    std::vector<MeshData> mesh_cache;

  private:
    // Block layout for std140 uniform buffer
    struct alignas(16) BLASInfoBlockLayout {
      alignas(4) uint prims_offs; // Offset/extent into blas_prims buffer
      alignas(4) uint nodes_offs; // Offset/extent into blas_nodes buffer
    };
    static_assert(sizeof(BLASInfoBlockLayout) == 16);
    
    // Buffer layout for std140 uniform buffer, mapped for write
    struct BLASInfoBufferLayout {
      alignas(4) uint size;
      std::array<BLASInfoBlockLayout, met_max_meshes> data;
    } *m_blas_info_map;

  public:
    // Packed mesh data, used in gen_object_data to bake surface textures
    gl::Buffer mesh_verts; // Mesh vertices; packed position, normal, and reparameterized texture uvs
    gl::Buffer mesh_elems; // Mesh elements data
    gl::Array  mesh_array; // Vertex array for draw dispatch over mesh data

    // Packed BLAS BVH data, used in render/query primitives
    gl::Buffer blas_info;  // Per-mesh offsets into blas_nodes and blas_prims 
    gl::Buffer blas_nodes; // Traversal data, parent AABB, and compressed child AABBS
    gl::Buffer blas_prims; // Packed mesh primitive data in bvh construction order

    // Draw commands to assemble multidraw dispatch over the indexed mesh data in `mesh_array`;
    std::vector<gl::MultiDrawInfo::DrawCommand> mesh_draw;
    
  public:
    // Class constructor and update function handle GL-side data
    SceneGLHandler();
    void update(const Scene &) override;
  };
  
  // Template specialization of SceneGLHandler.
  // Handles texture atlases for 1-component and 3-component textures in the scene,
  // as well as information on how to access the corresponding texture atlas regions.
  template <>
  class SceneGLHandler<met::Image> : public SceneGLHandlerBase {
    // Per-texture block layout for std140 uniform buffer
    struct alignas(16) BlockLayout {
      alignas(4) bool         is_3f;
      alignas(4) uint         layer;
      alignas(8) eig::Array2u offs, size;
      alignas(8) eig::Array2f uv0, uv1;
    };
    static_assert(sizeof(BlockLayout) == 48);

    // All-texture block layout for std140 uniform buffer, mapped for write
    struct BufferLayout {
      alignas(4) uint size;
      std::array<BlockLayout, met_max_textures> data;
    } *m_texture_info_map;

  public:
    // This buffer contains offsets/sizes, ergo layout info necessary to
    // sample relevant parts of the texture atlases, storing one instance
    // of BlockLayout per image resource
    gl::Buffer texture_info;
    
    // Texture atlases store packed image data in f32 format; one atlas for 3-component
    // images, another for 1-component images
    detail::TextureAtlas2d3f texture_atlas_3f;
    detail::TextureAtlas2d1f texture_atlas_1f;
  
  public:
    // Class constructor and update function handle GL-side data
    SceneGLHandler();
    void update(const Scene &) override;

    // SceneGLHandler<Uplifting> becomes friend as it bakes some texture data per-object
    // SceneGLHandler<Object> becomes friend as it bakes some texture data per-object
    friend class detail::SceneGLHandler<Uplifting>;
    friend class detail::SceneGLHandler<Object>;
  };
  
  // Template specialization of SceneGLHandler.
  // Handles shader-side per-wavelength access of illuminant spectral data.
  template <>
  class SceneGLHandler<met::Spec> : public SceneGLHandlerBase {
    // Pixel buffer copy helpers
    gl::Buffer      spec_buffer;
    std::span<Spec> spec_buffer_map;

  public:
    // Array texture which stores one full spectral reflectance per layer,
    // s.t. one sample equals the reflectance at one wavelength
    gl::TextureArray1d1f spec_texture;

  public:
    // Class constructor and update function handle GL-side data
    SceneGLHandler();
    void update(const Scene &) override;
  };
  
  // Template specialization of SceneGLHandler.
  // Handles shader-side per-wavelength access of observer spectral data.
  template <>
  class SceneGLHandler<met::CMFS> : public SceneGLHandlerBase {
    // Pixel buffer copy helpers
    gl::Buffer      cmfs_buffer;
    std::span<CMFS> cmfs_buffer_map;

  public:
    // Array texture which stores one full trio of color matching functions per layer,
    // s.t. one sample equals the color matching function at one wavelength
    gl::TextureArray1d3f cmfs_texture;

  public:
    // Class constructor and update function handle GL-side data
    SceneGLHandler();
    void update(const Scene &) override;
  };

  template <>
  class SceneGLHandler<met::Scene> : public SceneGLHandlerBase {
    // // General scene information
    // struct BlockLayout {
    //   alignas(4) uint n_objects;
    //   alignas(4) uint n_emitters;
    //   alignas(4) uint n_views;
    //   alignas(4) uint n_upliftings;
    // } *m_scene_info_map;
    
  public:
    // // General buffer with scene info; nr of objects/emitters/etc
    // gl::Buffer scene_info;

  public:
    // Class constructor and update function handle GL-side data
    SceneGLHandler();
    void update(const Scene &) override;
  };
} // namespace met::detail