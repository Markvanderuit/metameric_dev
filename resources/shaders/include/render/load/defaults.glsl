#ifndef LOAD_DEFAULTS_GLSL_GUARD
#define LOAD_DEFAULTS_GLSL_GUARD

#include <distribution.glsl>
#include <render/detail/scene_types.glsl>

#ifndef SCENE_DATA_MESH
MeshVertPack scene_mesh_vert(uint i) { MeshVertPack d; return d;  }
uvec3 scene_mesh_elem(uint i)        { return uvec3(0);           }
MeshPrimPack scene_mesh_prim(uint i) { MeshPrimPack d; return d;  }
BVHNode0Pack scene_mesh_node0(uint i) { BVHNode0Pack d; return d; } 
BVHNode1Pack scene_mesh_node1(uint i) { BVHNode1Pack d; return d; } 
MeshInfo scene_mesh_info(uint i)     { MeshInfo d; return d;      }
uint scene_mesh_count()              { return 0;                  }
#endif // !SCENE_DATA_MESH

#ifndef SCENE_DATA_BVH
void scene_set_stack_value(uint i, uint v) {            }
uint scene_get_stack_value(uint i)         {  return 0; }
#endif // !SCENE_DATA_BVH

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
  AtlasInfo scene_texture_coef_info(uint i) { AtlasInfo d; return d; }
  AtlasInfo scene_texture_brdf_info(uint i) { AtlasInfo d; return d; }
  vec2 scene_texture_coef_size()            { return vec2(0);        }
  vec2 scene_texture_brdf_size()            { return vec2(0);        }
  uvec4 scene_texture_coef_fetch(ivec3 p)   { return uvec4(0);       }
  uint  scene_texture_brdf_fetch(ivec3 p)   { return 0u;             }
  float scene_texture_basis_sample(float wvl, uint j) { return 0.f;  }
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