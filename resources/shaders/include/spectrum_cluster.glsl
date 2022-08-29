#ifndef SPECTRUM_CLUSTER_GLSL_GUARD
#define SPECTRUM_CLUSTER_GLSL_GUARD

#if defined(GL_KHR_shader_subgroup_basic)      \
 && defined(GL_KHR_shader_subgroup_arithmetic) \
 && defined(GL_KHR_shader_subgroup_clustered)

#include <math.glsl>
#include <spectrum.glsl>

// Nr. of wavelength bins per invocation
const uint cl_spectrum_bins_n = 4;

// Nr of invocations and index of current invocation in spectrum
const uint cl_spectrum_invc_n = CEIL_DIV(wavelength_samples, cl_spectrum_bins_n);
uint       cl_spectrum_invc_i = gl_GlobalInvocationID.x % cl_spectrum_invc_n;

// Offset and size of wavelength bins for current invocation
uint cl_spectrum_bin_offs = cl_spectrum_invc_i * cl_spectrum_bins_n;
uint cl_spectrum_bin_size = min(cl_spectrum_bin_offs + cl_spectrum_bins_n, wavelength_samples) - cl_spectrum_bin_offs;

// Define to perform commonly occuring iteration
#define cl_bin_iter(__b) for (uint __b = 0; __b < cl_spectrum_bin_size; ++__b)
#define cl_bin_iter_remainder(__b) for (uint __b = cl_spectrum_bin_size; __b < cl_spectrum_bins_n; ++__b)

// Scatter/gather spectrum data into/from clustered representation
#define cl_spec_scatter(dst, src) { cl_bin_iter(j) dst[j] = src[cl_spectrum_bin_offs + j];       \
                                    cl_bin_iter_remainder(j) dst[j] = 0.f; /* mask remainder */  }
#define cl_spec_gather(dst, src)  { cl_bin_iter(j) dst[cl_spectrum_bin_offs + j] = src[j]; }

bool cl_bin_elect() { return cl_spectrum_invc_i == 0; }

/* Per-cluster Spectrum object is just a vec4 of wavelengths */

#define ClSpec vec4
#define ClMask bvec4

/* Reductions */

float cl_hsum(in ClSpec s) {
  return subgroupClusteredAdd(hsum(s), cl_spectrum_invc_n);
}

float cl_hdot(in ClSpec s, in ClSpec o) {
  return subgroupClusteredAdd(dot(s, o), cl_spectrum_invc_n);
}

float cl_hdot(in ClSpec s) {
  return subgroupClusteredAdd(dot(s, s), cl_spectrum_invc_n);
}

float cl_hmean(in ClSpec s) {
  return wavelength_samples_inv * cl_hsum(s);
}

float cl_hmax(in ClSpec s) {
  cl_bin_iter_remainder(i) s[i] = FLT_MIN;
  return subgroupClusteredMax(hmax(s), cl_spectrum_invc_n);
}

float cl_hmin(in ClSpec s) {
  cl_bin_iter_remainder(i) s[i] = FLT_MAX;
  return subgroupClusteredMin(hmin(s), cl_spectrum_invc_n);
}

/* Logic comparators */

ClMask cl_eq(in ClSpec s, in float f) { return equal(s, ClSpec(f)); }
ClMask cl_neq(in ClSpec s, in float f) { return notEqual(s, ClSpec(f)); }
ClMask cl_gr(in ClSpec s, in float f) { return greaterThan(s, ClSpec(f)); }
ClMask cl_ge(in ClSpec s, in float f) { return greaterThanEqual(s, ClSpec(f)); }
ClMask cl_lr(in ClSpec s, in float f) { return lessThan(s, ClSpec(f)); }
ClMask cl_le(in ClSpec s, in float f) { return lessThanEqual(s, ClSpec(f)); }

ClMask cl_eq(in ClSpec s, in ClSpec o) { return equal(s, o); }
ClMask cl_neq(in ClSpec s, in ClSpec o) { return notEqual(s, o); }
ClMask cl_gr(in ClSpec s, in ClSpec o) { return greaterThan(s, o); }
ClMask cl_ge(in ClSpec s, in ClSpec o) { return greaterThanEqual(s, o);}
ClMask cl_lr(in ClSpec s, in ClSpec o) { return lessThan(s, o); }
ClMask cl_le(in ClSpec s, in ClSpec o) { return lessThanEqual(s, o); }

ClMask cl_eq(in float f, in ClSpec o) { return equal(ClSpec(f), o); }
ClMask cl_neq(in float f, in ClSpec o) { return notEqual(ClSpec(f), o); }
ClMask cl_gr(in float f, in ClSpec o) { return greaterThan(ClSpec(f), o); }
ClMask cl_ge(in float f, in ClSpec o) { return greaterThanEqual(ClSpec(f), o); }
ClMask cl_lr(in float f, in ClSpec o) { return lessThan(ClSpec(f), o); }
ClMask cl_le(in float f, in ClSpec o) { return lessThanEqual(ClSpec(f), o); }

/* Mask operations */

ClSpec cl_select(in ClMask m, in ClSpec x, in ClSpec y) { return mix(y, x, m); }
ClSpec cl_select(in ClMask m, in ClSpec x, in float y) { return mix(ClSpec(y), x, m); }
ClSpec cl_select(in ClMask m, in float x, in ClSpec y) { return mix(y, ClSpec(x), m); }
ClSpec cl_select(in ClMask m, in float x, in float y) { return mix(ClSpec(y), ClSpec(x), m); }

#endif // EXTENSIONS
#endif // SPECTRUM_CLUSTER_GLSL_GUARD