#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/atlas.hpp>
#include <metameric/core/bvh.hpp>
#include <metameric/core/components.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/image.hpp>
#include <metameric/core/detail/packing.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/texture.hpp>

namespace met::detail {
  // Template specialization of SceneGLHandler.
  // Handles shader-side information about objects in the scene
  template <>
  class SceneGLHandler<met::Object> : public SceneGLHandlerBase {
    // Per-object block layout for std140 uniform buffer
    struct alignas(16) BlockLayout {
      alignas(16) eig::Matrix4f trf;
      alignas(4)  bool          is_active;
      alignas(4)  uint          mesh_i;
      alignas(4)  uint          uplifting_i;
      alignas(4)  uint          brdf_type;
      alignas(8)  eig::Array2u  albedo_data;
      alignas(4)  uint          metallic_data;
      alignas(4)  uint          roughness_data;
    };
    static_assert(sizeof(BlockLayout) == 64 + 16 + 16);
    
    // All-object block layout for std140 uniform buffer, mapped for write
    struct BufferLayout {
      alignas(4) uint size;
      std::array<BlockLayout, met_max_objects> data;
    } *m_object_info_map;

  public:
    // This buffer stores one instance of BlockLayout per object component
    gl::Buffer object_info;

  public:
    // Class constructor
    SceneGLHandler();

    // Update GL-side data for objects indicated as changed
    void update(const Scene &) override;
  };

  // Template specialization of SceneGLHandler.
  // Handles GL data about emitters in the scene.
  template <>
  class SceneGLHandler<met::Emitter> : public SceneGLHandlerBase {
    // Per-object block layout for std140 uniform buffer
    struct alignas(16) EmBlockLayout {
      alignas(16) eig::Matrix4f trf;
      alignas(4)  uint          type;
      alignas(4)  bool          is_active;
      alignas(4)  uint          illuminant_i;
      alignas(4)  float         illuminant_scale;
    };
    static_assert(sizeof(EmBlockLayout) == 80);
    
    // All-object block layout for std140 uniform buffer, mapped for write
    struct EmBufferLayout {
      alignas(4) uint size;
      std::array<EmBlockLayout, met_max_emitters> data;
    } *m_em_info_map;

    // Single block layout for std140 uniform buffer, mapped for write
    struct EnvBufferLayout {
      alignas(4) bool envm_is_present;
      alignas(4) uint envm_i;
    } *m_envm_info_data;

  public:
    // This buffer stores one instance of EmBlockLayout per emitter component
    gl::Buffer emitter_info;

    // Information on a background constant emitter to sample, if rays escape
    gl::Buffer emitter_envm_info;

    // Sampling distribution based on each emitter's individual power output
    gl::Buffer emitter_distr_buffer;

  public:
    // Class constructor
    SceneGLHandler();

    // Update GL-side data for objects indicated as changed
    void update(const Scene &) override;
  };

  // Template specialization of SceneGLHandler.
  // Handles gl-side uplifted texture data, though on a per-object basis. Most
  // data is filled in by the uplifting pipeline, which is part of the program pipeline 
  template <>
  class SceneGLHandler<met::Uplifting> : public SceneGLHandlerBase {
    using basis_type = gl::Texture1d<float, 1, gl::TextureType::eImageArray>;

  public:
    // Atlas texture; each per-object patch stored in this atlas holds either
    // linear (ours) or moment (bounded MESE) coefficients
    // Atlas textures; each scene object has a patch in the atlas with some material parameters
    TextureAtlas<uint, 4> texture_coef; // Stores packed linear coefficients representing surface spectral reflectances
    TextureAtlas<uint, 1> texture_brdf; // Stores packing of other brdf parameters (roughness, metallic at fp16)

    // Array texture; each layer holds an available set of basis functions
    basis_type texture_basis;

    // Warped phase data for bounded MESE method
    gl::Texture1d1f texture_warp;

  public:
    // Class constructor
    SceneGLHandler();

    // Update GL-side data for objects indicated as changed
    void update(const Scene &) override;
  };

  // Template specialization of SceneGLHandler.
  // Handles packed mesh buffers, blas bvh buffers, and info to unpack said buffers shader-side
  template <>
  class SceneGLHandler<met::Mesh> : public SceneGLHandlerBase {
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

    // Caches of simplified meshes and generated acceleration structure data
    std::vector<met::Mesh>   m_meshes;
    std::vector<met::BVH<8>> m_blas;

  public:
    // Packed mesh data, used in gen_object_data to bake surface textures
    gl::Buffer mesh_verts; // Mesh vertices; packed position, normal, and reparameterized texture uvs
    gl::Buffer mesh_elems; // Mesh elements data

    // Packed BLAS BVH data, used in render/query primitives
    gl::Buffer blas_info;    // Per-mesh offsets into blas_nodes_* and blas_prims 
    gl::Buffer blas_nodes_0; // Parent AABB and traversal data (is_leaf, size, offs)
    gl::Buffer blas_nodes_1; // Child AABBs at 8 bits per child; requires parent AABB to unpack
    gl::Buffer blas_prims;   // Packed mesh primitive data in bvh construction order

    // CPU-side packed primitive data
    // Useful for ray interaction recovery that mirrors the gpu's packed version exactly
    std::vector<PrimitivePack> blas_prims_cpu;

    // Draw array referencing packed, indexed mesh data
    // and a set of draw commands for assembling multidraw operations over this array;
    // one command for each mesh is present
    gl::Array array;
    std::vector<gl::MultiDrawInfo::DrawCommand> draw_commands;
    
    // Cache of data to undo mesh transform into a unit cube
    std::vector<eig::Matrix4f> unit_transforms;
    
  public:
    // Class constructor
    SceneGLHandler();

    // Update GL-side data for objects indicated as changed
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
    TextureAtlas<float, 3> texture_atlas_3f;
    TextureAtlas<float, 1> texture_atlas_1f;
  
  public:
    // Class constructor
    SceneGLHandler();
    
    // Update GL-side data for objects indicated as changed
    void update(const Scene &) override;
  };
  
  // Template specialization of SceneGLHandler.
  // Handles shader-side per-wavelength access of illuminant spectral data.
  template <>
  class SceneGLHandler<met::Spec> : public SceneGLHandlerBase {
    using texture_type = gl::Texture1d<float, 1, gl::TextureType::eImageArray>;
    
    // Pixel buffer copy helpers
    gl::Buffer      spec_buffer;
    std::span<Spec> spec_buffer_map;

  public:
    // Array texture which stores one full spectral reflectance per layer,
    // s.t. one sample equals the reflectance at one wavelength
    texture_type spec_texture;

  public:
    // Class constructor
    SceneGLHandler();

    // Update GL-side data for objects indicated as changed
    void update(const Scene &) override;
  };
  
  // Template specialization of SceneGLHandler.
  // Handles shader-side per-wavelength access of observer spectral data.
  template <>
  class SceneGLHandler<met::CMFS> : public SceneGLHandlerBase {
    using texture_type = gl::Texture1d<float, 3, gl::TextureType::eImageArray>;

    // Pixel buffer copy helpers
    gl::Buffer      cmfs_buffer;
    std::span<CMFS> cmfs_buffer_map;

  public:
    // Array texture which stores one full trio of color matching functions per layer,
    // s.t. one sample equals the color matching function at one wavelength
    texture_type cmfs_texture;

  public:
    // Class constructor
    SceneGLHandler();

    // Update GL-side data for objects indicated as changed
    void update(const Scene &) override;
  };

  template <>
  class SceneGLHandler<met::Scene> : public SceneGLHandlerBase {
    // Block layout for std140 uniform buffer
    struct alignas(16) TLASInfoBufferLayout {
      alignas(16) eig::Matrix4f trf;
      alignas(16) eig::Matrix4f inv;
    } *m_tlas_info_map;
    
  public:
    // Packed TLAS BVH data, used in render/ray query primitives
    gl::Buffer tlas_info;    // Info object containing ray transforms
    gl::Buffer tlas_nodes_0; // Parent AABB and traversal data (is_leaf, size, offs)
    gl::Buffer tlas_nodes_1; // Child AABBs at 8 bits per child; requires parent AABB to unpack
    gl::Buffer tlas_prims;   // Indices referring to underlying revelant BLAS structure

  public:
    // Class constructor
    SceneGLHandler();

    // Update GL-side data for objects indicated as changed
    void update(const Scene &) override;
  };
} // namespace met::detail