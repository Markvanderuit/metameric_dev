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
  struct RTObjectInfoLayout {
    alignas(4)  uint          is_active;
    alignas(4)  uint          mesh_i;
    alignas(4)  uint          uplifting_i;

    alignas(64) eig::Matrix4f trf;
    alignas(64) eig::Matrix4f trf_inv;
    
    alignas(4)  uint          albedo_use_sampler;
    alignas(4)  uint          albedo_i;
    alignas(16) Colr          albedo_v;
  };

  struct RTMeshInfoLayout {
    alignas(4)  uint          elems_begin;
    alignas(4)  uint          elems_extent;
    alignas(4)  uint          verts_begin;
    alignas(4)  uint          verts_extent;
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