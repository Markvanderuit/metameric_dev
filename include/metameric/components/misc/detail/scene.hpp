#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/scene.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <memory>

namespace met::detail {
  /* Object information structure, detailing indices and values
      referring to the object's mesh shape and surface materials. */
  struct RTObjectInfo {
    alignas(64) eig::Matrix4f trf;
    alignas(64) eig::Matrix4f trf_inv;

    alignas(4) uint           is_active;

    alignas(4)  uint          mesh_i;
    alignas(4)  uint          uplifting_i;

    alignas(4)  uint          padd; // TODO was here

    // alignas(4)  uint          albedo_use_sampler;
    // alignas(4)  uint          albedo_i;
    // alignas(16) Colr          albedo_v;
  };

  /* Mesh information structure, detailing which range of the mesh
     data buffers describes a specific mesh. */
  struct RTMeshInfo {
    alignas(4) uint verts_offs;
    alignas(4) uint verts_size;
    alignas(4) uint elems_offs;
    alignas(4) uint elems_size;
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
  };

  struct RTObjectData {
    std::vector<RTObjectInfo> info;
    gl::Buffer                info_gl;
  };

  struct ObjectUnifLayout {
    eig::Matrix4f alignas(64) trf;
  };

  // TODO alignas
  struct EmitterUnifLayout {
    eig::Array3f alignas(16) p;
    Spec                     value;
  };

  // TODO alignas
  struct ColrSystemUnifLayout {
    CMFS observer;
    Spec illuminant;
  };

  // Structure for gpu-side mesh data
  struct MeshLayout {
    gl::Buffer verts;
    gl::Buffer norms;
    gl::Buffer texuv;
    gl::Buffer elems;
    gl::Array  array;

    static MeshLayout realize(const AlMeshData &);
  };

  // Structure for gpu-side image data and attached sampler
  struct TextureLayout {
    std::unique_ptr<gl::AbstractTexture> texture;
    gl::Sampler                          sampler;

    static TextureLayout realize(const DynamicImage &);
  };

  // Structure for gpu-side illuminant spectrum data
  struct IlluminantLayout {
    
  };
} // namespace met::detail