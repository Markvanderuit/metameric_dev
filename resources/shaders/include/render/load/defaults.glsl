#ifndef LOAD_DEFAULTS_GLSL_GUARD
#define LOAD_DEFAULTS_GLSL_GUARD

// Includes relevant to renderer
#include <math.glsl>
#include <spectrum.glsl>
#include <distribution.glsl>
#include <render/detail/packing.glsl>
#include <render/detail/scene_types.glsl>
#include <render/detail/path_types.glsl>

#ifndef SCENE_DATA_BLAS
BLASInfo      scene_blas_info(uint i) { BLASInfo d; return d;      }
PrimitivePack scene_blas_prim(uint i) { PrimitivePack d; return d; }
BVHNodePack   scene_blas_node(uint i) { BVHNodePack d; return d;  } 
#endif // !SCENE_DATA_BLAS

#ifndef SCENE_DATA_TLAS
TLASInfo     scene_tlas_info()       { TLASInfo d; return d;     }
uint         scene_tlas_prim(uint i) { return 0;                 }
BVHNodePack  scene_tlas_node(uint i) { BVHNodePack d; return d; } 
#endif // !SCENE_DATA_TLAS

#ifndef SCENE_DATA_EMITTER
EmitterInfo scene_emitter_info(uint i) { EmitterInfo d; return d; }
uint scene_emitter_count()             { return 0;                } 
bool scene_has_envm_emitter() { return false;                     }
uint scene_envm_emitter_idx() { return 0;                         }
declare_distr_sampler_default(wavelength);
declare_distr_sampler_default(emitters);
#endif // !SCENE_DATA_EMITTER

#ifndef SCENE_DATA_OBJECT
ObjectInfo scene_object_info(uint i) { ObjectInfo d; return d; }
uint scene_object_count()            { return 0;               }
#endif // !SCENE_DATA_OBJECT

#ifndef SCENE_DATA_TEXTURE
  AtlasInfo scene_texture_coef_info(uint i)               { AtlasInfo d; return d; }
  AtlasInfo scene_texture_brdf_info(uint i)               { AtlasInfo d; return d; }
  vec2      scene_texture_coef_size()                     { return vec2(0);        }
  vec2      scene_texture_brdf_size()                     { return vec2(0);        }
  uvec4     scene_texture_coef_fetch(ivec3 p)             { return uvec4(0);       }
  uint      scene_texture_brdf_fetch(ivec3 p)             { return 0u;             }
  float     scene_texture_basis_sample(float wvl, uint j) { return 0.f;            }
#endif // !SCENE_DATA_TEXTURE

#ifndef SCENE_DATA_CMFS
mat4x3 scene_cmfs(uint cmfs_i, vec4 wvls) {
  return mat4x3(1);
}
#endif // !SCENE_DATA_CMFS

#ifndef SCENE_DATA_ILLUMINANT
vec4 scene_illuminant(uint cmfs_i, vec4 wvls) {
  return vec4(1);
}
#endif // !SCENE_DATA_ILLUMINANT

#endif // LOAD_DEFAULTS_GLSL_GUARD