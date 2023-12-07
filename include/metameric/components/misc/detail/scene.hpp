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
  inline
  eig::Array2u clamp_size_by_setting(Settings::TextureSize setting, eig::Array2u size) {
    switch (setting) {
      case Settings::TextureSize::eHigh: return size.cwiseMin(2048u);
      case Settings::TextureSize::eMed:  return size.cwiseMin(1024u);
      case Settings::TextureSize::eLow:  return size.cwiseMin(512u);
      default:                           return size;
    }
  }

  // Texture data structure
  // Holds gl-side packed image data in the scene, as well as
  // accompanying info blocks to read said data gl-side
  struct RTTextureData {
    // Uniform object layout;
    // provides information for accessing parts of
    // texture data from the texture atlases.
    struct TextureInfo {
      alignas(4) bool         is_3f;
      alignas(4) uint         layer;
      alignas(8) eig::Array2u offs, size;
      alignas(8) eig::Array2f uv0, uv1;
    };

  public:
    std::vector<TextureInfo> info;
    gl::Buffer               info_gl;

    // Texture atlases to store all loaded image data in f32 format on the GL side
    TextureAtlas<float, 3>   atlas_3f;
    TextureAtlas<float, 1>   atlas_1f;
  
  public:
    RTTextureData() = default;
    RTTextureData(const Scene &);

    bool is_stale(const Scene &scene) const;
    void update(const Scene &scene);
  };
  
  // Mesh data structure
  // Holds gl-side packed mesh data in the scene, as well as
  // accompanying info blocks to read said data gl-side
  struct RTMeshData {
    // Uniform object layout;
    // provides information for accessing parts of
    // mesh data from the packed buffer.
    struct MeshInfo {
      alignas(4) uint verts_offs;
      alignas(4) uint verts_size;
      alignas(4) uint elems_offs;
      alignas(4) uint elems_size;
    };
    
  public:
    std::vector<MeshInfo> info;
    gl::Buffer            info_gl;
    gl::Buffer            verts_a, verts_b;
    gl::Buffer            elems, elems_al;
    gl::Array             array;
    
  public:
    RTMeshData() = default;
    RTMeshData(const Scene &);

    bool is_stale(const Scene &scene) const;
    void update(const Scene &scene);
  };

  // Object data structure
  // Holds gl-side packed object data in the scene, as well as
  // accompanying info blocks to read said data gl-side
  class RTObjectData {
    mutable bool m_is_atlas_stale = true;

  public:
    // Uniform object layout;
    // provides information for a single object, and how to access
    // its mesh surface and material textures from other buffers.
    struct alignas(16) ObjectInfo {
      alignas(16) eig::Matrix4f trf;
      alignas(16) eig::Matrix4f trf_inv;

      alignas(4)  bool          is_active;
      alignas(4)  uint          mesh_i;
      alignas(4)  uint          uplifting_i;

      alignas(4)  bool          is_albedo_sampled;
      alignas(4)  uint          albedo_i;
      alignas(16) Colr          albedo_v;
      
      // barycentric atlas access info
      alignas(4) uint           layer;
      alignas(8) eig::Array2u   offs, size;
    };

  public:
    std::vector<ObjectInfo> info;
    gl::Buffer              info_gl;
    TextureAtlas<float, 4>  atlas_bary;

  public:
    RTObjectData() = default;
    RTObjectData(const Scene &);

    bool is_stale(const Scene &scene) const;
    void update(const Scene &scene);
  
  public:
    bool is_atlas_stale() const {
      return m_is_atlas_stale;
    }
  };

  // Object weight data structure
  // Holds gl-side texture atlas storing tesselation weights, as
  // well as accompanying info blocks to read said atlas
  struct RTObjectWeightData {
    // barycentric atlas access info
    struct ObjectWeightInfo {
      alignas(4) uint           layer;
      alignas(8) eig::Array2u   offs, size;
    };

  public:
    std::vector<ObjectWeightInfo> info;
    gl::Buffer                    info_gl;
    TextureAtlas<float, 4>        atls_4f;
  };
  
  // Uplifting data structure
  // Holds gl-side packed uplifting data in the scene, as well as
  // accompanying info blocks to read said data gl-side. Stores data
  // on a per-uplifting and per-object basis. Allocated but not filled
  // in, as content is generated in the rest of the uplifting pipeline.
  struct RTUpliftingData {
    using Texture1d4fArray = gl::Texture1d<float, 4, gl::TextureType::eImageArray>;
    using SpecPack         = eig::Array<float, wavelength_samples, 4>;

    // Uniform object layout;
    // provides information for accessing parts of
    // uplfifting data from the packed buffer/atlas.
    struct UpliftingInfo {
      uint elem_offs; 
      uint elem_size;
    };

  public:
    // Info objects to detail range of the spectra used by each uplifting
    std::vector<UpliftingInfo> info;
    gl::Buffer                 info_gl;

    // All constraint spectra per-uplifting are packed per tetrahedron
    // for fast sampled access during rendering
    gl::Buffer                   spectra_gl;         // Mapped buffer for pixel buffer copy
    std::span<SpecPack>          spectra_gl_mapping; // Corresponding map
    Texture1d4fArray             spectra_gl_texture; // Texture array layout for all spectra
    
  public:
    RTUpliftingData() = default;
    RTUpliftingData(const Scene &);

    bool is_stale(const Scene &scene) const;
    void update(const Scene &scene);
  };

  // C<FS spectra data structure
  // Holds gl-side packed cmfs data in the scene.
  struct RTObserverData {
    using Texture1d3fArray = gl::Texture1d<float, 3, gl::TextureType::eImageArray>;

    gl::Buffer       cmfs_gl;         // Mapped buffer for pixel buffer copy
    std::span<CMFS>  cmfs_gl_mapping; // Corresponding map
    Texture1d3fArray cmfs_gl_texture; // Texture array layout for interpolated lookups

  public:
    RTObserverData() = default;
    RTObserverData(const Scene &);

    bool is_stale(const Scene &scene) const;
    void update(const Scene &scene);
  };

  // Illuminant spectra data structure
  // Holds gl-side packed illm data in the scene.
  struct RTIlluminantData {
    using Texture1d1fArray = gl::Texture1d<float, 1, gl::TextureType::eImageArray>;

    gl::Buffer       illm_gl;         // Mapped buffer for pixel buffer copy
    std::span<Spec>  illm_gl_mapping; // Corresponding map
    Texture1d1fArray illm_gl_texture; // Texture array layout for interpolated lookups

  public:
    RTIlluminantData() = default;
    RTIlluminantData(const Scene &);

    bool is_stale(const Scene &scene) const;
    void update(const Scene &scene);
  };

  // Color system spectra data structure
  // Holds gl-side packed csys data in the scene.
  struct RTColorSystemData {
    using Texture1d3fArray = gl::Texture1d<float, 3, gl::TextureType::eImageArray>;

    gl::Buffer       csys_gl;         // Mapped buffer for pixel buffer copy
    std::span<CMFS>  csys_gl_mapping; // Corresponding map
    Texture1d3fArray csys_gl_texture; // Texture array layout for interpolated lookups

  public:
    RTColorSystemData() = default;
    RTColorSystemData(const Scene &);

    bool is_stale(const Scene &scene) const;
    void update(const Scene &scene);
  };
} // namespace met::detail