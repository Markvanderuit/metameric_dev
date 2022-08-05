#ifndef SUBGROUP_CONSTANTS_GLSL_GUARD
#define SUBGROUP_CONSTANTS_GLSL_GUARD

// Compile-time const variants of gl_SubgroupSize and gl_NumSubgroups;
// these are replaced by the parser for the specific machine on which 
// the shader is compiled
const uint subgroup_size_const = MET_SUBGROUP_SIZE;
const uint num_subgroups_const = (gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z) 
                               / subgroup_size_const;

// Scatter large array 'src' to smaller array 'dst' over a subgroup
#define sg_scatter(dst, src, dst_size)                           \
  for (uint i = 0; i < dst_size; ++i) {                          \
    dst[i] = src[i * gl_SubgroupSize + gl_SubgroupInvocationID]; \
  }                                                              \

#endif // SUBGROUP_CONSTANTS_GLSL_GUARD