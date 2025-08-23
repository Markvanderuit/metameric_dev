#ifndef LOAD_DEFAULTS_GLSL_GUARD
#define LOAD_DEFAULTS_GLSL_GUARD

// Includes relevant to renderer
#include <math.glsl>
#include <spectrum.glsl>
#include <distribution.glsl>
#include <render/detail/packing.glsl>
#include <render/detail/scene_types.glsl>
#include <render/detail/path_types.glsl>

#ifndef LOAD_BLAS_GLSL_GUARD
BLASInfo      scene_blas_info(uint i) { BLASInfo d; return d;      }
PrimitivePack scene_blas_prim(uint i) { PrimitivePack d; return d; }
BVHNodePack   scene_blas_node(uint i) { BVHNodePack d; return d;  } 
#endif // !LOAD_BLAS_GLSL_GUARD

#ifndef LOAD_TLAS_GLSL_GUARD
TLASInfo     scene_tlas_info()       { TLASInfo d; return d;     }
uint         scene_tlas_prim(uint i) { return 0;                 }
BVHNodePack  scene_tlas_node(uint i) { BVHNodePack d; return d; } 
#endif // !LOAD_TLAS_GLSL_GUARD

#ifndef LOAD_EMTITER_GLSL_GUARD
Emitter scene_emitter_info(uint i) { Emitter d; return d; }
uint scene_emitter_count()         { return 0;            } 
declare_distr_sampler_default(wavelength);
declare_distr_sampler_default(emitters);
#endif // !LOAD_EMTITER_GLSL_GUARD

#ifndef LOAD_ENVMAP_GLSL_GUARD
bool scene_has_envmap()              { return false; }
uint scene_envmap_emitter_i()        { return 0;     }
declare_alias_sampler_default(envm_alias_table);
#endif // !LOAD_ENVMAP_GLSL_GUARD

#ifndef LOAD_OBJECT_GLSL_GUARD
Object scene_object_info(uint i) { Object d; return d; }
uint scene_object_count()        { return 0;           }
#endif // !LOAD_OBJECT_GLSL_GUARD

#ifndef LOAD_TEXTURE_GLSL_GUARD
  AtlasInfo scene_texture_object_coef_info(uint i)        { AtlasInfo d; return d; }
  AtlasInfo scene_texture_object_brdf_info(uint i)        { AtlasInfo d; return d; }
  AtlasInfo scene_texture_emitter_coef_info(uint i)       { AtlasInfo d; return d; }
  vec2      scene_texture_object_coef_size()              { return vec2(0);        }
  vec2      scene_texture_object_brdf_size()              { return vec2(0);        }
  vec2      scene_texture_emitter_coef_size()             { return vec2(0);        }
  uvec4     scene_texture_object_coef_fetch(ivec3 p)      { return uvec4(0);       }
  vec4      scene_texture_object_brdf_fetch(ivec3 p)      { return vec4(0);        }
  uvec4     scene_texture_emitter_coef_fetch(ivec3 p)     { return uvec4(0);       }
  float     scene_texture_emitter_scle_fetch(ivec3 p)     { return 0.f;            }
  float     scene_texture_basis_sample(float wvl, uint j) { return 0.f;            }
#endif // !LOAD_TEXTURE_GLSL_GUARD

#ifndef LOAD_CMFS_GLSL_GUARD
mat4x3 scene_cmfs(uint cmfs_i, vec4 wvls) {
  return mat4x3(1);
}
#endif // !LOAD_CMFS_GLSL_GUARD

#ifndef LOAD_ILLUMINANT_GLSL_GUARD
vec4 scene_illuminant(uint cmfs_i, vec4 wvls) {
  return vec4(1);
}
#endif // !LOAD_ILLUMINANT_GLSL_GUARD

#endif // LOAD_DEFAULTS_GLSL_GUARD