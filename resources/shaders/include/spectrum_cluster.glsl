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

#endif // EXTENSIONS
#endif // SPECTRUM_CLUSTER_GLSL_GUARD