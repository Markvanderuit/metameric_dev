#ifndef SPECTRUM_CLUSTER_GLSL_GUARD
#define SPECTRUM_CLUSTER_GLSL_GUARD
#if defined(GL_KHR_shader_subgroup_basic)      \
 && defined(GL_KHR_shader_subgroup_arithmetic) \
 && defined(GL_KHR_shader_subgroup_clustered)

// Nr. of invocs per spectrum
const uint sg_spectrum_ninvc = 4; 

// Nr of wavelength bins per invoc
const uint sg_spectrum_nbins = (wavelength_samples + (sg_spectrum_ninvc - 1)) 
                             / sg_spectrum_ninvc;

// Remainder dealt with by last invoc
const uint sg_spectrum_mod = wavelength_samples - sg_spectrum_ninvc * (sg_spectrum_nbins - 1);

// Index of current spectrum in n, and invoc in spectrum
uint sg_spectrum_global_i = gl_GlobalInvocationID.x / sg_spectrum_ninvc;
uint sg_spectrum_local_i  = gl_GlobalInvocationID.x % sg_spectrum_ninvc;

#define sg_bin_iter(i)           for (uint i = 0; i < sg_spectrum_nbins; ++i)
#define sg_bin_iter_apply(i, op) for (uint i = 0; i < sg_spectrum_nbins; ++i) { s.v[i] = op; }

// Scatter large array 'src' to smaller array 'dst' over a cluster
/* #define sg_bin_scatter(dst, src)                                 \
  for (uint j = 0; j < sg_spectrum_nbins; ++j) {                 \
    dst.v[j] = src[sg_spectrum_local_i * sg_spectrum_nbins + j]; \
  }       

#define sg_bin_gather(dst, src)                                  \                    
  for (uint j = 0; j < sg_spectrum_nbins; ++j) {                 \
    dst[sg_spectrum_local_i * sg_spectrum_nbins + j] = src.v[j]; \
  } */

// Scatter large array 'src' to smaller array 'dst' over a cluster
#define sg_bin_scatter(dst, src) sg_bin_iter(j) { dst.v[j] = src[sg_spectrum_local_i * sg_spectrum_nbins + j]; }       
#define sg_bin_gather(dst, src) sg_bin_iter(j) { dst[sg_spectrum_local_i * sg_spectrum_nbins + j] = src.v[j]; }

/* END OF DEFINE FILE */

/* BEGIN OF OBJECT FILE */

// Define max/min single precision float constants for masked components
#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1.175494351e-38

struct SgSpec {
  float v[sg_spectrum_nbins];
};

/* Constructors */

SgSpec sg_spectrum(in float f) {
  SgSpec s;
  sg_bin_iter_apply(i, f);
  return s;
}

/* Component-wise math operators */

SgSpec sg_add(in SgSpec s, in float f) {
  sg_bin_iter_apply(i, s.v[i] + f);
  return s;
}

SgSpec sg_sub(in SgSpec s, in float f) {
  sg_bin_iter_apply(i, s.v[i] - f);
  return s;
}

SgSpec sg_mul(in SgSpec s, in float f) {
  sg_bin_iter_apply(i, s.v[i] * f);
  return s;
}

SgSpec sg_div(in SgSpec s, in float f) {
  sg_bin_iter_apply(i, s.v[i] / f);
  return s;
}

SgSpec sg_pow(in SgSpec s, in float p) {
  if (p > 1.f) 
    sg_bin_iter_apply(i, pow(s.v[i], p));
  return s;
}

/* Column-wise math operators */

SgSpec sg_add(in SgSpec s, in SgSpec o) {
  sg_bin_iter_apply(i, s.v[i] + o.v[i]);
  return s;
}

SgSpec sg_sub(in SgSpec s, in SgSpec o) {
  sg_bin_iter_apply(i, s.v[i] - o.v[i]);
  return s;
}

SgSpec sg_mul(in SgSpec s, in SgSpec o) {
  sg_bin_iter_apply(i, s.v[i] * o.v[i]);
  return s;
}

SgSpec sg_div(in SgSpec s, in SgSpec o) {
  sg_bin_iter_apply(i, s.v[i] / o.v[i]);
  return s;
}

SgSpec sg_pow(in SgSpec s, in SgSpec o) {
  sg_bin_iter(i) 
    if (o.v[i] > 1.f) 
      s.v[i] = pow(s.v[i], o.v[i]);
  return s;
}

// ...

/* Component-wise comparators */

SgSpec sg_max(in SgSpec s, in float f) {
  sg_bin_iter_apply(i, max(s.v[i], f));
  return s;
}

SgSpec sg_min(in SgSpec s, in float f) {
  sg_bin_iter_apply(i, min(s.v[i], f));
  return s;
}

SgSpec sg_clamp(in SgSpec s, in float minv, in float maxv) {
  sg_bin_iter_apply(i, clamp(s.v[i], minv, maxv));
  return s;
}

/* Column-wise comparators */

SgSpec sg_max(in SgSpec s, in SgSpec o) {
  sg_bin_iter_apply(i, max(s.v[i], o.v[i]));
  return s;
}

SgSpec sg_min(in SgSpec s, in SgSpec o) {
  sg_bin_iter_apply(i, min(s.v[i], o.v[i]));
  return s;
}

SgSpec sg_clamp(in SgSpec s, in SgSpec minv, in SgSpec maxv) {
  sg_bin_iter_apply(i, clamp(s.v[i], minv.v[i], maxv.v[i]));
  return s;
}

/* Reductions */

float sg_hsum(in SgSpec s) {
  float f = 0.f;
  sg_bin_iter(i) f += s.v[i];
  return subgroupClusteredAdd(f, sg_spectrum_ninvc);
}

float sg_hmean(in SgSpec s) {
  return sg_hsum(s) * wavelength_samples_inv;
}

float sg_dot(in SgSpec s, in SgSpec o) {
  return sg_hsum(sg_mul(s, o));
}

float sg_dot(in SgSpec s) {
  return sg_hsum(sg_mul(s, s));
}

float sg_max_value(in SgSpec s) {
  float f = FLT_MIN;
  sg_bin_iter(i) f = min(f, s.v[i]);
  return subgroupClusteredMin(f, sg_spectrum_ninvc);
}

/* END OF OBJECT FILE */

#endif // EXTENSIONS
#endif // SPECTRUM_CLUSTER_GLSL_GUARD