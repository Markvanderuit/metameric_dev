#ifndef LOAD_DEFAULTS_GLSL_GUARD
#define LOAD_DEFAULTS_GLSL_GUARD

#include <distribution.glsl>
#include <render/detail/scene_types.glsl>

#ifndef SCENE_DATA_MESH
MeshVertPack scene_mesh_vert(uint i) { MeshVertPack d; return d; }
uvec3 scene_mesh_elem(uint i)        { return uvec3(0);          }
MeshPrimPack scene_mesh_prim(uint i) { MeshPrimPack d; return d; }
BVHNodePack scene_mesh_node(uint i)  { BVHNodePack d; return d;  } 
MeshInfo scene_mesh_info(uint i)     { MeshInfo d; return d;     }
uint scene_mesh_count()              { return 0;                 }
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
declare_distr_sampler_default(wavelength, buff_wvls_distr, wavelength_samples)
declare_distr_sampler_default(emitters, buff_emitters_distr, max_supported_emitters)
#endif // !SCENE_DATA_EMITTER

#ifndef SCENE_DATA_OBJECT
ObjectInfo scene_object_info(uint i) { ObjectInfo d; return d; }
uint scene_object_count()            { return 0;               }
#endif // !SCENE_DATA_OBJECT

#ifndef SCENE_DATA_REFLECTANCE
#endif // !SCENE_DATA_REFLECTANCE

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