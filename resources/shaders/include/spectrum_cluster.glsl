#ifndef SPECTRUM_CLUSTER_GLSL_GUARD
#define SPECTRUM_CLUSTER_GLSL_GUARD
#if defined(GL_KHR_shader_subgroup_basic)      \
 && defined(GL_KHR_shader_subgroup_arithmetic) \
 && defined(GL_KHR_shader_subgroup_clustered)

#define SgSpec vec4

// Nr. of bins per invocation
const uint sg_spectrum_nbins = 4;

// Nr of invocations per spectrum
const uint sg_spectrum_ninvc = (wavelength_samples + (sg_spectrum_nbins - 1)) / sg_spectrum_nbins;

// Remainder dealt with by last invoc
const uint sg_spectrum_mod = wavelength_samples - sg_spectrum_nbins * (sg_spectrum_ninvc - 1);

// Index of current spectrum in n, and invoc in spectrum
uint sg_spectrum_global_i = gl_GlobalInvocationID.x / sg_spectrum_ninvc;
uint sg_spectrum_local_i  = gl_GlobalInvocationID.x % sg_spectrum_ninvc;

#define sg_bin_iter(i) for (uint i = 0; i < sg_spectrum_nbins; ++i)

// Scatter large array 'src' to smaller array 'dst' over a cluster
#define sg_bin_scatter(dst, src) sg_bin_iter(j) dst[j] = src[sg_spectrum_local_i * sg_spectrum_nbins + j];

// Gather small array 'src' to larger array 'dst' over a cluster
#define sg_bin_gather(dst, src) sg_bin_iter(j) dst[sg_spectrum_local_i * sg_spectrum_nbins + j] = src[j];

#define sg_spec_scatter(dst, src) { sg_bin_scatter(dst, src) }
#define sg_spec_gather(dst, src)  { sg_bin_gather(dst, src)  }

bool sg_bin_elect() { return sg_spectrum_local_i == 0; }

/* END OF DEFINE FILE */

/* BEGIN OF OBJECT FILE */

// Define max/min single precision float constants for masked components
#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1.175494351e-38

/* Reductions */

float sg_hsum(in SgSpec s) {
  float f = 0.f;
  sg_bin_iter(i) f += s[i];
  return subgroupClusteredAdd(f, sg_spectrum_ninvc);
}

float sg_hmean(in SgSpec s) {
  return sg_hsum(s) * wavelength_samples_inv;
}

float sg_hdot(in SgSpec s, in SgSpec o) {
  return sg_hsum(s * o);
}

float sg_hdot(in SgSpec s) {
  return sg_hsum(s * s);
}

float sg_max_value(in SgSpec s) {
  float f = FLT_MIN;
  sg_bin_iter(i) f = max(f, s[i]);
  return subgroupClusteredMax(f, sg_spectrum_ninvc);
}

float sg_min_value(in SgSpec s) {
  float f = FLT_MAX;
  sg_bin_iter(i) f = min(f, s[i]);
  return subgroupClusteredMin(f, sg_spectrum_ninvc);
}

/* END OF OBJECT FILE */

#endif // EXTENSIONS
#endif // SPECTRUM_CLUSTER_GLSL_GUARD