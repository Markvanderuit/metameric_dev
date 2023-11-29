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
    
    // atlas4f access info
    alignas(4) uint           layer;
    alignas(8) eig::Array2u   offs, size;
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

  /* Texture data block; holds all packed together image data
     in an f32 format. Generated on image import/load. */
  struct RTTextureData {
    std::vector<RTTextureInfo> info;
    gl::Buffer                 info_gl;

    // Texture atlases to store all loaded image data in f32 format on the GL side
    std::vector<uint>          atlas_indices;
    TextureAtlas<float, 3>     atlas_3f;
    TextureAtlas<float, 1>     atlas_1f;
  
  public:
    RTTextureData() = default;
    RTTextureData(const Scene &);

    bool is_stale(const Scene &scene) const;
    void update(const Scene &scene);
  };
  
  /* Mesh vertex/element data block; holds all packed-together
     mesh data used in a scene. Generated on obj scene import/load. */
  struct RTMeshData {
    std::vector<RTMeshInfo> info;
    gl::Buffer              info_gl;
    gl::Buffer              verts_a, verts_b;
    gl::Buffer              elems, elems_al;
    gl::Array               array;

    
  public:
    RTMeshData() = default;
    RTMeshData(const Scene &);

    bool is_stale(const Scene &scene) const;
    void update(const Scene &scene);
  };

  /* Object layout data block; holds all packed-together scene
     object data (mostly indices to other things). Generated on
     scene load, and updated whenever scene is edited. */
  struct RTObjectData {
    std::vector<RTObjectInfo> info;
    gl::Buffer                info_gl;

    // Texture atlas to hold barycentrics per-object
    std::vector<uint>      atlas_indices;
    TextureAtlas<float, 4> atlas_4f;
  public:
    RTObjectData() = default;
    RTObjectData(const Scene &);

    bool is_stale(const Scene &scene) const;
    void update(const Scene &scene);
  };

  /* Uplifting information structure, detailing which range of the
     tesselation spectra is used for this uplifting. */
  struct RTUpliftingInfo {
    uint elem_offs; 
    uint elem_size;
  };

  /* Gathered uplifting data block; holds all packed-together
     uplifting data used to render a scene, on a per-uplifting 
     and a per-object basis. Allocated but not filled in; 
     content is generated on the fly by the uplifting pipeline. */
  struct RTUpliftingData {
    using Texture1d4fArray = gl::Texture1d<float, 4, gl::TextureType::eImageArray>;
    using SpecPack         = eig::Array<float, wavelength_samples, 4>;
    
    // Info objects to detail range of the spectra used by each uplifting
    std::vector<RTUpliftingInfo> info;
    gl::Buffer                   info_gl;

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

  /* Gathered color system data block; holds all packed-together color system
     spectra  */
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

  /* Gathered illuminant data block; holds all packed-together illuminant
     spectra  */
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