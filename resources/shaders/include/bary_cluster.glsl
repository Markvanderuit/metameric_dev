#ifndef BARY_CLUSTER_GLSL_GUARD
#define BARY_CLUSTER_GLSL_GUARD

#if defined(GL_KHR_shader_subgroup_basic)      \
 && defined(GL_KHR_shader_subgroup_arithmetic) \
 && defined(GL_KHR_shader_subgroup_clustered)

#include <math.glsl>
#include <spectrum.glsl>

/* Constants and defines */

// Nr. of values per invocation
const uint cl_bary_values_n = 4;

// Nr. of invocations for a given barycentric weight
const uint cl_bary_invoc_n = CEIL_DIV(generalized_weights, cl_bary_values_n);

// Index of current invocation inside a given barycentric weight
uint cl_bary_invoc_i = gl_GlobalInvocationID.x % cl_bary_invoc_n;

// Offset and size of values for current invocation
uint cl_bary_invoc_offs = cl_bary_invoc_i * cl_bary_values_n;
uint cl_bary_invoc_size = min(cl_bary_invoc_offs + cl_bary_values_n, generalized_weights) - cl_bary_invoc_offs;

// Define to perform commonly occuring iteration
#define cl_bary_iter(__b)           for (uint __b = 0; __b < cl_bary_invoc_size; ++__b)
#define cl_bary_iter_remainder(__b) for (uint __b = cl_bary_invoc_size; __b < cl_bary_values_n; ++__b)

// Scatter/gather barycentric data into/from an array representation
#define cl_spec_scatter(dst, src) { cl_bary_iter(j) dst[j] = src[cl_bary_invoc_offs + j];        \
                                    cl_bary_iter_remainder(j) dst[j] = 0.f; /* mask remainder */ }
#define cl_spec_gather(dst, src)  { cl_bary_iter(j) dst[cl_bary_invoc_offs + j] = src[j]; }

bool cl_bin_elect() { return cl_bary_invoc_i == 0; }

// Per-invocation barycentric weights are just a vec4 of values
#define InBary float[generalized_weights]
#define ClBary vec4
#define ClMask bvec4

/* Reductions */

float cl_hsum(in ClBary s) {
  return subgroupClusteredAdd(hsum(s), cl_bary_invoc_n);
}

float cl_hdot(in ClBary s, in ClBary o) {
  return subgroupClusteredAdd(dot(s, o), cl_bary_invoc_n);
}

float cl_hdot(in ClBary s) {
  return subgroupClusteredAdd(dot(s, s), cl_bary_invoc_n);
}

float cl_hmean(in ClBary s) {
  return wavelength_samples_inv * cl_hsum(s);
}

float cl_hmax(in ClBary s) {
  return subgroupClusteredMax(hmax(s), cl_bary_invoc_n);
}

float cl_hmin(in ClBary s) {
  return subgroupClusteredMin(hmin(s), cl_bary_invoc_n);
}

/* Logic comparators */

ClMask cl_eq(in ClBary s, in float f) { return equal(s, ClBary(f)); }
ClMask cl_neq(in ClBary s, in float f) { return notEqual(s, ClBary(f)); }
ClMask cl_gr(in ClBary s, in float f) { return greaterThan(s, ClBary(f)); }
ClMask cl_ge(in ClBary s, in float f) { return greaterThanEqual(s, ClBary(f)); }
ClMask cl_lr(in ClBary s, in float f) { return lessThan(s, ClBary(f)); }
ClMask cl_le(in ClBary s, in float f) { return lessThanEqual(s, ClBary(f)); }

ClMask cl_eq(in ClBary s, in ClBary o) { return equal(s, o); }
ClMask cl_neq(in ClBary s, in ClBary o) { return notEqual(s, o); }
ClMask cl_gr(in ClBary s, in ClBary o) { return greaterThan(s, o); }
ClMask cl_ge(in ClBary s, in ClBary o) { return greaterThanEqual(s, o);}
ClMask cl_lr(in ClBary s, in ClBary o) { return lessThan(s, o); }
ClMask cl_le(in ClBary s, in ClBary o) { return lessThanEqual(s, o); }

ClMask cl_eq(in float f, in ClBary o) { return equal(ClBary(f), o); }
ClMask cl_neq(in float f, in ClBary o) { return notEqual(ClBary(f), o); }
ClMask cl_gr(in float f, in ClBary o) { return greaterThan(ClBary(f), o); }
ClMask cl_ge(in float f, in ClBary o) { return greaterThanEqual(ClBary(f), o); }
ClMask cl_lr(in float f, in ClBary o) { return lessThan(ClBary(f), o); }
ClMask cl_le(in float f, in ClBary o) { return lessThanEqual(ClBary(f), o); }

/* Mask operations */

ClBary cl_select(in ClMask m, in ClBary x, in ClBary y) { return mix(y, x, m); }
ClBary cl_select(in ClMask m, in ClBary x, in float y) { return mix(ClBary(y), x, m); }
ClBary cl_select(in ClMask m, in float x, in ClBary y) { return mix(y, ClBary(x), m); }
ClBary cl_select(in ClMask m, in float x, in float y) { return mix(ClBary(y), ClBary(x), m); }

#else
#error "Subgroup extensions are not defined"
#endif // EXTENSIONS
#endif // BARY_CLUSTER_GLSL_GUARD