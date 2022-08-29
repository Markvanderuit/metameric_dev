#ifndef SPECTRUM_SUBGROUP_GLSL_GUARD
#define SPECTRUM_SUBGROUP_GLSL_GUARD

#if defined(GL_KHR_shader_subgroup_basic)     \
 && defined(GL_KHR_shader_subgroup_arithmetic)

#include <math.glsl>
#include <spectrum.glsl>

// Compile-time const variants of gl_SubgroupSize and gl_NumSubgroups;
// replaced by the parser for the specific machine on which  the shader is compiled
const uint subgroup_size_const = MET_SUBGROUP_SIZE;
const uint num_subgroups_const = HPROD_3(gl_WorkGroupSize) / subgroup_size_const;

// Define derived variables for subgroup layout
const uint sg_spectrum_bins_n = CEIL_DIV(wavelength_samples, subgroup_size_const);
const uint sg_spectrum_mod    = wavelength_samples - subgroup_size_const * (sg_spectrum_bins_n - 1);

// Starting index and nr. of iterations to perform for a specific invocation
uint sg_spectrum_bin_offs = gl_SubgroupInvocationID;
uint sg_spectrum_bin_size = (sg_spectrum_bins_n - 1) + (sg_spectrum_bin_offs < sg_spectrum_mod  ? 1 : 0);

// Define to perform commonly occuring iteration
#define sg_bin_iter(__b) for (uint __b = 0; __b < sg_spectrum_bin_size; ++__b)

// Scatter/gather spectrum data into/from subgroup representation
#define sg_spec_scatter(dst, src)                                            \
  { sg_bin_iter(i) dst[i] = src[i * gl_SubgroupSize + sg_spectrum_bin_offs]; }
#define sg_spec_gather(dst, src)                                            \
  { sg_bin_iter(i) dst[i * gl_SubgroupSize + sg_spectrum_bin_offs] = src[i]; }

/* Per-Subgroup spectrum object; size is likely 2, 1, or 0 */

#define SgSpec float[sg_spectrum_bins_n]

/* Constructors */

SgSpec sg_spectrum(in float f) {
  SgSpec s;
  sg_bin_iter(i) s[i] = f;
  return s;
}

/* Component-wise math operators */

SgSpec sg_mul(in SgSpec s, in float f) {
  sg_bin_iter(i) s[i] *= f;
  return s;
}

SgSpec sg_div(in SgSpec s, in float f) {
  sg_bin_iter(i) s[i] /= f;
  return s;
}

SgSpec sg_add(in SgSpec s, in float f) {
  sg_bin_iter(i) s[i] += f;
  return s;
}

SgSpec sg_sub(in SgSpec s, in float f) {
  sg_bin_iter(i) s[i] -= f;
  return s;
}

SgSpec sg_pow(in SgSpec s, in float p) {
  if (p > 1.f)
    sg_bin_iter(i) s[i] = pow(s[i], p);
  return s;
}

/* Column-wise math operators */

SgSpec sg_mul(in SgSpec s, in SgSpec o) {
  sg_bin_iter(i) s[i] *= o[i];
  return s;
}

SgSpec sg_div(in SgSpec s, in SgSpec o) {
  sg_bin_iter(i) s[i] /= o[i];
  return s;
}

SgSpec sg_add(in SgSpec s, in SgSpec o) {
  sg_bin_iter(i) s[i] += o[i];
  return s;
}

SgSpec sg_sub(in SgSpec s, in SgSpec o) {
  sg_bin_iter(i) s[i] -= o[i];
  return s;
}

SgSpec sg_pow(in SgSpec s, in SgSpec p) {
  sg_bin_iter(i) 
    if (p[i] > 1.f)
      s[i] = pow(s[i], p[i]);
  return s;
}

/* Component-wise comparators */

SgSpec sg_max(in SgSpec s, in float b) {
  sg_bin_iter(i) s[i] = max(s[i], b);
  return s;
}

SgSpec sg_min(in SgSpec s, in float b) {
  sg_bin_iter(i) s[i] = min(s[i], b);
  return s;
}

SgSpec sg_clamp(in SgSpec s, in float minv, in float maxv) {
  sg_bin_iter(i) s[i] = clamp(s[i], minv, maxv);
  return s;
}

/* Column-wise comparators */

SgSpec sg_max(in SgSpec s, in SgSpec o) {
  sg_bin_iter(i)  s[i] = max(s[i], o[i]);
  return s;
}

SgSpec sg_min(in SgSpec s, in SgSpec o) {
  sg_bin_iter(i) s[i] = min(s[i], o[i]);
  return s;
}

SgSpec sg_clamp(in SgSpec s, in SgSpec minv, in SgSpec maxv) {
  sg_bin_iter(i) s[i] = clamp(s[i], minv[i], maxv[i]);
  return s;
}

/* Reductions */

float sg_hsum(in SgSpec s) {
  float f = 0.f;
  sg_bin_iter(i) f += s[i];
  return subgroupAdd(f);
}

float sg_hmean(in SgSpec s) {
  return sg_hsum(s) * wavelength_samples_inv;
}

float sg_dot(in SgSpec a, in SgSpec b) {
  return sg_hsum(sg_mul(a, b));
}

float sg_dot(in SgSpec a) {
  return sg_hsum(sg_mul(a, a));
}

float sg_max_value(in SgSpec a) {
  float f = FLT_MIN;
  sg_bin_iter(i) f = max(a[i], f);
  return subgroupMax(f);
}

float sg_min_value(in SgSpec a) {
  float f = FLT_MAX;
  sg_bin_iter(i) f = min(a[i], f);
  return subgroupMin(f);
}

#endif // EXTENSIONS
#endif // SPECTRUM_SUBGROUP_GLSL_GUARD