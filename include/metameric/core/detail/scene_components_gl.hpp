#pragma once

#include <metameric/core/mesh.hpp>
#include <metameric/core/image.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/scene_components.hpp>
#include <metameric/core/detail/scene_components_state.hpp>
#include <metameric/core/detail/texture_atlas.hpp>
#include <metameric/core/detail/bvh.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/texture.hpp>

namespace met::detail {
  // GL-side object data
  // Handles shader-side information about objects in the scene
  template <>
  class GLPacking<met::Object> {
    struct alignas(16) ObjectInfoLayout {
      alignas(16) eig::Matrix4f trf;
      alignas(16) eig::Matrix4f trf_inv;

      alignas(4)  bool          is_active;

      alignas(4)  uint          mesh_i;
      alignas(4)  uint          uplifting_i;

      alignas(4)  bool          is_albedo_sampled;
      alignas(4)  uint          albedo_i;
      alignas(16) Colr          albedo_v;
    };

    // Mapped buffer accessors
    uint*                       m_buffer_map_size;
    std::span<ObjectInfoLayout> m_buffer_map_data;

  public:
    // This buffer stores one instance of ObjectInfoLayout per object component
    gl::Buffer object_info;
    
  public:
    GLPacking();
    void update(std::span<const detail::Component<met::Object>>, const Scene &);
  };
  
  /* template <>
  class GLPacking<met::Emitter> {

  public:
  
    void update(std::span<const detail::Component<met::Emitter>>, const Scene &);
  }; */

  // GL-side uplifting data
  // Handles gl-side uplifted texture data, though on a per-object basis. Most
  // contents of .texture_weights and .texture_spectra are filled in by the
  // uplifting pipeline, which is part of the program pipeline 
  template <>
  class GLPacking<met::Uplifting> {
    using texture_type = gl::Texture1d<float, 4, gl::TextureType::eImageArray>;
    using atlas_type   = TextureAtlas<float, 4>;

  public:
    // Atlas texture; each per-object texture stored in this atlas holds barycentric
    // weights and an index, referring to one set of four spectra in `texture_spectra`
    atlas_type texture_weights;

    // Array texture; each layer holds one set of four spectra, packed together s.t.
    // one texture sample equates four reflectances at a single wavelength
    texture_type texture_spectra;

  public:
    GLPacking();
    void update(std::span<const detail::Component<met::Uplifting>>, const Scene &);
  };
  
  /* template <>
  class GLPacking<met::ColorSystem> {
    struct alignas(16) ColorSystemUniformLayout {
      alignas(4) uint cmfs_i;
      alignas(4) uint illuminant_i;
      alignas(4) uint n_scatters;
    };
    
  public:
  
    void update(std::span<const detail::Component<met::ColorSystem>>, const Scene &);
  }; */

  // GL-side mesh data
  // Handles packed mesh buffers, bvh buffers, and info to unpack
  // said buffers shader-side
  template <>
  class GLPacking<met::Mesh> {
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

    // Caches of simplified meshes and generated acceleration data
    std::vector<met::Mesh> m_meshes;
    std::vector<met::BVH>  m_bvhs;

    // Mapped buffer accessors
    uint*                      m_buffer_layout_map_size;
    std::span<MeshInfoLayout> m_buffer_layout_map_data;

  public:
    // This buffer contains offsets/sizes, ergo layout info necessary to
    // sample relevant parts of the other buffers, storing one instance
    // of MeshInfoLayout per mesh resource
    gl::Buffer mesh_info;

    // Packed indexed mesh data
    gl::Buffer mesh_verts;
    gl::Buffer mesh_elems;
    gl::Buffer mesh_elems_al;

    // Packed BVH data
    gl::Buffer bvh_nodes;
    gl::Buffer bvh_prims;
    
    // Draw array referencing packed, indexed mesh data
    // and a set of draw commands for assembling multidraw operations over this array;
    // one command for each mesh is present
    gl::Array array;
    std::vector<gl::MultiDrawInfo::DrawCommand> draw_commands;
    
  public:
    GLPacking();
    void update(std::span<const detail::Resource<met::Mesh>>, const Scene &);
  };
  
  // GL-side texture data
  // Handles texture atlases for 1-component and 3-component textures in the scene,
  // as well as information on how to access the corresponding texture atlas regions.
  template <>
  class GLPacking<met::Image> {
    struct TextureInfoLayout {
      alignas(4) bool         is_3f;
      alignas(4) uint         layer;
      alignas(8) eig::Array2u offs, size;
      alignas(8) eig::Array2f uv0, uv1;
    };

  public:
    // This bufer contains offsets/sizes, ergo layout info necessary to
    // sample relevant parts of the texture atlases, storing one instance
    // of TextureInfoLayout per image resource
    gl::Buffer texture_info;
    
    // Texture atlases store packed image data in f32 format; one atlas for 3-component
    // images, another for 1-component images
    TextureAtlas<float, 3> texture_atlas_3f;
    TextureAtlas<float, 1> texture_atlas_1f;
  
  public:
    void update(std::span<const detail::Resource<met::Image>>, const Scene &);
  };
  
  // GL-side spectrum data
  // Handles shader-side per-wavelength access of illuminant spectral data.
  template <>
  class GLPacking<met::Spec> {
    using texture_type = gl::Texture1d<float, 1, gl::TextureType::eImageArray>;
    
    // Pixel buffer copy helpers
    gl::Buffer      spec_buffer;
    std::span<Spec> spec_buffer_map;

  public:
    // Array texture which stores one full spectral reflectance per layer,
    // s.t. one sample equals the reflectance at one wavelength
    texture_type spec_texture;

  public:
    GLPacking();
    void update(std::span<const detail::Resource<met::Spec>>, const Scene &);
  };
  
  // GL-side color-matching-function data
  // Handles shader-side per-wavelength access of observer spectral data.
  template <>
  class GLPacking<met::CMFS> {
    using texture_type = gl::Texture1d<float, 3, gl::TextureType::eImageArray>;

    // Pixel buffer copy helpers
    gl::Buffer      cmfs_buffer;
    std::span<CMFS> cmfs_buffer_map;

  public:
    // Array texture which stores one full trio of color matching functions per layer,
    // s.t. one sample equals the color matching function at one wavelength
    texture_type cmfs_texture;

  public:
    GLPacking();
    void update(std::span<const detail::Resource<met::CMFS>>, const Scene &);
  };
} // namespace met::detail