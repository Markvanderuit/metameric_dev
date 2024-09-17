#pragma once

#include <metameric/core/mesh.hpp>
#include <metameric/core/image.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/components.hpp>
#include <metameric/core/detail/scene_components_state.hpp>
#include <metameric/core/detail/packing.hpp>
#include <metameric/core/detail/texture_atlas.hpp>
#include <metameric/core/detail/bvh.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/texture.hpp>

namespace met::detail {
  // GL-side object data
  // Handles shader-side information about objects in the scene
  template <>
  class SceneGLType<met::Object> {
    struct alignas(16) ObjectInfoLayout {
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

    // Mapped buffer accessors
    uint*                       m_info_map_size;
    std::span<ObjectInfoLayout> m_info_map_data;

  public:
    // This buffer stores one instance of ObjectInfoLayout per object component
    gl::Buffer object_info;

  public:
    // Class constructor
    SceneGLType();

    // Update packing data for this scene; objects whose state is indicated as changed
    // are repacked for GL-side
    void update(const Scene &);

    std::span<const ObjectInfoLayout> objects() const { return m_info_map_data; }

  };

  template <>
  class SceneGLType<met::Emitter> {
    struct alignas(16) EmitterEnvmapInfoLayout {
      alignas(4) bool envm_is_present;
      alignas(4) uint envm_i;
    };

    struct alignas(16) EmitterInfoLayout {
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

    // Mapped buffer accessors
    uint*                        m_info_map_size;
    std::span<EmitterInfoLayout> m_info_map_data;
    EmitterEnvmapInfoLayout     *m_envm_map_data;

  public:
    // This buffer stores one instance of EmitterInfoLayout per emitter component
    gl::Buffer emitter_info;

    // Information on a background constant emitter to sample, if rays escape
    gl::Buffer emitter_envm_info;

    // Sampling distribution based on each emitter's individual power output
    gl::Buffer emitter_distr_buffer;

    SceneGLType();
    void update(const Scene &);
  };

  // GL-side uplifting data
  // Handles gl-side uplifted texture data, though on a per-object basis. Most
  // data is filled in by the uplifting pipeline, which is part of the program pipeline 
  template <>
  class SceneGLType<met::Uplifting> {
    using atlas_type_f = TextureAtlas<float, 4>;
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
    SceneGLType();
    void update(const Scene &);
  };
  
  template <>
  struct SceneGLType<met::ColorSystem> {
    Spec       wavelength_distr;
    gl::Buffer wavelength_distr_buffer;

  public:
    SceneGLType();
    void update(const Scene &);
  };

  // GL-side mesh data
  // Handles packed mesh buffers, bvh buffers, and info to unpack
  // said buffers shader-side
  template <>
  class SceneGLType<met::Mesh> {
    // Mesh layout data
    struct MeshInfoLayout {
      alignas(16) eig::Matrix4f trf; // Model packing transform

      alignas(4)  uint verts_offs;
      alignas(4)  uint verts_size;

      alignas(4)  uint elems_offs;
      alignas(4)  uint elems_size;

      alignas(4)  uint nodes_offs;
      alignas(4)  uint nodes_size;
    };
    static_assert(sizeof(MeshInfoLayout) == 96);

  private:
    // Caches of simplified meshes and generated acceleration data
    std::vector<met::Mesh> m_meshes;
    std::vector<met::BVH>  m_bvhs;

    // Mapped buffer accessors
    uint*                     m_buffer_layout_map_size;
    std::span<MeshInfoLayout> m_buffer_layout_map_data;

  public:
    // This buffer contains offsets/sizes, ergo layout info necessary to
    // sample relevant parts of the other buffers, storing one instance
    // of MeshInfoLayout per mesh resource
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
    std::vector<PrimitivePack>  bvh_prims_cpu;
    std::vector<eig::Array3u>   bvh_txuvs_cpu; // unparameterized texture coordinates per bvh prim

    // Draw array referencing packed, indexed mesh data
    // and a set of draw commands for assembling multidraw operations over this array;
    // one command for each mesh is present
    gl::Array array;
    std::vector<gl::MultiDrawInfo::DrawCommand> draw_commands;
    
    // Cache of mesh transforms, pre-applied to object transforms
    std::vector<eig::Matrix4f> transforms;
    
  public:
    SceneGLType();

    void update(const Scene &);
  };
  
  // GL-side texture data
  // Handles texture atlases for 1-component and 3-component textures in the scene,
  // as well as information on how to access the corresponding texture atlas regions.
  template <>
  class SceneGLType<met::Image> {
    struct alignas(16) TextureInfoLayout {
      alignas(4)  bool         is_3f;
      alignas(4)  uint         layer;
      alignas(8)  eig::Array2u offs, size;
      alignas(8)  eig::Array2f uv0, uv1;
    };

    // Mapped nr. of textures and texture data
    uint*                        m_info_map_size;
    std::span<TextureInfoLayout> m_info_map_data;

  public:
    // This buffer contains offsets/sizes, ergo layout info necessary to
    // sample relevant parts of the texture atlases, storing one instance
    // of TextureInfoLayout per image resource
    gl::Buffer texture_info;
    
    // Texture atlases store packed image data in f32 format; one atlas for 3-component
    // images, another for 1-component images
    TextureAtlas<float, 3> texture_atlas_3f;
    TextureAtlas<float, 1> texture_atlas_1f;
  
  public:
    SceneGLType();
    
    void update(const Scene &);
  };
  
  // GL-side spectrum data
  // Handles shader-side per-wavelength access of illuminant spectral data.
  template <>
  class SceneGLType<met::Spec> {
    using texture_type = gl::Texture1d<float, 1, gl::TextureType::eImageArray>;
    
    // Pixel buffer copy helpers
    gl::Buffer      spec_buffer;
    std::span<Spec> spec_buffer_map;

  public:
    // Array texture which stores one full spectral reflectance per layer,
    // s.t. one sample equals the reflectance at one wavelength
    texture_type spec_texture;

  public:
    SceneGLType();
    void update(const Scene &);
  };
  
  // GL-side color-matching-function data
  // Handles shader-side per-wavelength access of observer spectral data.
  template <>
  class SceneGLType<met::CMFS> {
    using texture_type = gl::Texture1d<float, 3, gl::TextureType::eImageArray>;

    // Pixel buffer copy helpers
    gl::Buffer      cmfs_buffer;
    std::span<CMFS> cmfs_buffer_map;

  public:
    // Array texture which stores one full trio of color matching functions per layer,
    // s.t. one sample equals the color matching function at one wavelength
    texture_type cmfs_texture;

  public:
    SceneGLType();
    void update(const Scene &);
  };
} // namespace met::detail