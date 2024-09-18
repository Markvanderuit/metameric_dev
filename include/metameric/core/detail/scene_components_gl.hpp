#pragma once

#include <metameric/core/mesh.hpp>
#include <metameric/core/image.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/components.hpp>
#include <metameric/core/detail/bvh.hpp>
#include <metameric/core/detail/packing.hpp>
#include <metameric/core/detail/texture_atlas.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/texture.hpp>

namespace met::detail {
  // Define maximum supported components for some types
  // These aren't up to device limits, but mostly exist so some 
  // sizes can be hardcoded shader-side in uniform buffers and
  // can be crammed into shared memory in some places
  constexpr static uint met_max_meshes      = MET_SUPPORTED_MESHES;
  constexpr static uint met_max_objects     = MET_SUPPORTED_OBJECTS;
  constexpr static uint met_max_emitters    = MET_SUPPORTED_EMITTERS;
  constexpr static uint met_max_upliftings  = MET_SUPPORTED_UPLIFTINGS;
  constexpr static uint met_max_constraints = MET_SUPPORTED_CONSTRAINTS;
  constexpr static uint met_max_textures    = MET_SUPPORTED_TEXTURES;

  // Template specialization of SceneGLHandler.
  // Handles shader-side information about objects in the scene
  template <>
  class SceneGLHandler<met::Object> : public SceneGLHandlerBase {
    // Per-object block layout for std140 uniform buffer
    struct alignas(16) BlockLayout {
      alignas(16) eig::Matrix4f trf;
      alignas(16) eig::Matrix4f trf_inv;
      alignas(16) eig::Matrix4f trf_mesh;
      alignas(16) eig::Matrix4f trf_mesh_inv;
      alignas(4)  bool          is_active;
      alignas(4)  uint          mesh_i;
      alignas(4)  uint          uplifting_i;
      alignas(4)  bool          is_albedo_sampled;
      alignas(4)  uint          albedo_i;
      alignas(16) Colr          albedo_v;
    };
    
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
      alignas(16) eig::Matrix4f trf_inv;
      alignas(4)  uint          type;
      alignas(4)  bool          is_active;
      alignas(4)  uint          illuminant_i;
      alignas(4)  float         illuminant_scale;
      alignas(4)  eig::Array3f  center; // center for sphere/point, corner for rect
      alignas(4)  float         srfc_area_inv;
      alignas(4)  eig::Array3f  rect_n;
      alignas(4)  float         sphere_r;
    };
    
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
    using atlas_type_u = TextureAtlas<uint, 4>;
    using basis_type   = gl::Texture1d<float, 1, gl::TextureType::eImageArray>;

  public:
    // Atlas texture; each per-object patch stored in this atlas holds either
    // linear (ours) or moment (bounded MESE) coefficients
    atlas_type_u texture_coefficients;

    // Basis functions; each layer holds a basis function
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
  // Handles packed mesh buffers, bvh buffers, and info to unpack said buffers shader-side
  template <>
  class SceneGLHandler<met::Mesh> : public SceneGLHandlerBase {
    // Per-mesh block layout for std140 uniform buffer
    struct alignas(16) MeshBlockLayout {
      alignas(16) eig::Matrix4f trf; // Model packing transform
      alignas(4)  uint verts_offs;
      alignas(4)  uint verts_size;
      alignas(4)  uint elems_offs;
      alignas(4)  uint elems_size;
      alignas(4)  uint nodes_offs;
      alignas(4)  uint nodes_size;
    };
    static_assert(sizeof(MeshBlockLayout) == 96);
    
    // All-mesh block layout for std140 uniform buffer, mapped for write
    struct MeshBufferLayout {
      alignas(4) uint size;
      std::array<MeshBlockLayout, met_max_meshes> data;
    } *m_mesh_info_map;

  private:
    // Caches of simplified meshes and generated acceleration data
    std::vector<met::Mesh> m_meshes;
    std::vector<met::BVH>  m_bvhs;

  public:
    // This buffer contains offsets/sizes, ergo layout info necessary to
    // sample relevant parts of the other buffers
    gl::Buffer mesh_info;

    // Packed mesh data, used in gen_object_data and miscellaneous operations
    gl::Buffer mesh_verts; // Mesh vertices; packed position, normal, and reparameterized texture uvs
    gl::Buffer mesh_elems; // Mesh elements
    gl::Buffer mesh_txuvs; // Original (unreparameterized) texture coordinates, kept for baking

    // Packed BVH data, used during render/query operations
    gl::Buffer bvh_nodes;
    gl::Buffer bvh_prims;

    // CPU-side packed bvh data and original texture coordinates
    // Useful for ray interaction recovery that mirrors the gpu-side precision issues
    std::vector<PrimitivePack> bvh_prims_cpu;
    std::vector<eig::Array3u>  bvh_txuvs_cpu; // unparameterized texture coordinates per bvh prim

    // Draw array referencing packed, indexed mesh data
    // and a set of draw commands for assembling multidraw operations over this array;
    // one command for each mesh is present
    gl::Array array;
    std::vector<gl::MultiDrawInfo::DrawCommand> draw_commands;
    
    // Cache of mesh transforms, pre-applied to object transforms
    std::vector<eig::Matrix4f> transforms;
    
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
      alignas(8) eig::Array2u offs, size;
      alignas(8) eig::Array2f uv0, uv1;
    };

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
} // namespace met::detail