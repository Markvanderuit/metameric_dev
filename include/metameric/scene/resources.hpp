#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/atlas.hpp>
#include <metameric/core/bvh.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/image.hpp>
#include <metameric/core/detail/packing.hpp>
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
      met::Mesh     mesh;
      met::BVH<8>   bvh;
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
      alignas(8) eig::Array2f uv0, uv1;
    };
    static_assert(sizeof(BlockLayout) == 32);

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
    TextureAtlas2d3f texture_atlas_3f;
    TextureAtlas2d1f texture_atlas_1f;
  
  public:
    // Class constructor and update function handle GL-side data
    SceneGLHandler();
    void update(const Scene &) override;
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
    // Block layout for std140 uniform buffer
    struct alignas(16) BLASInfoBlockLayout {
      alignas(4) uint prims_offs; // Offset/extent into blas_prims buffer
      alignas(4) uint nodes_offs; // Offset/extent into blas_nodes buffer
    };
    static_assert(sizeof(BLASInfoBlockLayout) == 16);

    // Block layout for std140 uniform buffer
    struct alignas(16) TLASInfoBufferLayout {
      alignas(16) eig::Matrix4f trf; // Transformation into unit cube for TLAS rays
    } *m_tlas_info_map;
    static_assert(sizeof(TLASInfoBufferLayout) == 64u);
    
  public:
    // Packed BLAS BVH data, used in render/query primitives
    gl::Buffer blas_info;  // Per-mesh offsets into blas_nodes_* and blas_prims 
    gl::Buffer blas_nodes; // Traversal data, parent AABB, and compressed child AABBS
    gl::Buffer blas_prims; // Packed mesh primitive data in bvh construction order

    // Packed TLAS BVH data, used in render/ray query primitives
    gl::Buffer tlas_info;  // Info object containing ray transforms
    gl::Buffer tlas_nodes; // Traversal data, parent AABB, and compressed child AABBS
    gl::Buffer tlas_prims; // Indices referring to underlying revelant BLAS structure

  public:
    // Class constructor and update function handle GL-side data
    SceneGLHandler();
    void update(const Scene &) override;
  };
} // namespace met::detail