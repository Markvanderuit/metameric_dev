#ifndef SPECTRUM_SUBGROUP_GLSL_GUARD
#define SPECTRUM_SUBGROUP_GLSL_GUARD

#include <spectrum_layout.glsl>

// Define max/min single precision float constants for masked components
#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1.175494351e-38

// Define derived variables for subgroup layout
const uint sg_size     = 32; // TODO extract to provided/queried device const
const uint sg_iters    = (wavelength_samples + (sg_size - 1)) / sg_size;
const uint sg_iter_mod = sg_size * sg_iters - wavelength_samples;
const uint sg_size_end = sg_size - sg_iter_mod;

// Nr of iterations to perform for a specific invocation; non-const
uint wavelength_samples_mask = (sg_iters - 1 ) 
                             + (gl_SubgroupInvocationID < sg_size_end ? 1 : 0);

/* Per-Subgroup spectrum object */

#define SgSpec float[sg_iters]

/* Constructors */

SgSpec sg_spectrum(in float f) {
  SgSpec s;
  for (uint i = 0; i < wavelength_samples_mask; ++i)
    s[i] = f;
  return s;
}

/* Component-wise math operators */

SgSpec sg_mul(in SgSpec a, in float f) {
  SgSpec s;
  for (uint i = 0; i < wavelength_samples_mask; ++i)
    s[i] = a[i] * f;
  return s;
}

SgSpec sg_div(in SgSpec a, in float f) {
  SgSpec s;
  for (uint i = 0; i < wavelength_samples_mask; ++i)
    s[i] = a[i] / f;
  return s;
}

SgSpec sg_add(in SgSpec a, in float f) {
  SgSpec s;
  for (uint i = 0; i < wavelength_samples_mask; ++i)
    s[i] = a[i] + f;
  return s;
}

SgSpec sg_sub(in SgSpec a, in float f) {
  SgSpec s;
  for (uint i = 0; i < wavelength_samples_mask; ++i)
    s[i] = a[i] - f;
  return s;
}

SgSpec sg_pow(in SgSpec v, in float p) {
  SgSpec s = v;

  if (p > 1.f)
    for (uint i = 0; i < wavelength_samples_mask; ++i)
      s[i] = pow(s[i], p);

  return s;
}

/* Column-wise math operators */

SgSpec sg_mul(in SgSpec a, in SgSpec b) {
  SgSpec s;
  for (uint i = 0; i < wavelength_samples_mask; ++i)
    s[i] = a[i] * b[i];
  return s;
}

SgSpec sg_div(in SgSpec a, in SgSpec b) {
  SgSpec s;
  for (uint i = 0; i < wavelength_samples_mask; ++i)
    s[i] = a[i] / b[i];
  return s;
}

SgSpec sg_add(in SgSpec a, in SgSpec b) {
  SgSpec s;
  for (uint i = 0; i < wavelength_samples_mask; ++i)
    s[i] = a[i] + b[i];
  return s;
}

SgSpec sg_sub(in SgSpec a, in SgSpec b) {
  SgSpec s;
  for (uint i = 0; i < wavelength_samples_mask; ++i)
    s[i] = a[i] - b[i];
  return s;
}

SgSpec sg_pow(in SgSpec v, in SgSpec p) {
  SgSpec s = v;

  for (uint i = 0; i < wavelength_samples_mask; ++i)
    if (p[i] > 1.f)
      s[i] = pow(s[i], p[i]);

  return s;
}

/* Component-wise comparators */

SgSpec sg_max(in SgSpec a, in float b) {
  SgSpec s;
  for (uint i = 0; i < wavelength_samples_mask; ++i)
    s[i] = max(a[i], b);
  return s;
}

SgSpec sg_min(in SgSpec a, in float b) {
  SgSpec s;
  for (uint i = 0; i < wavelength_samples_mask; ++i)
    s[i] = min(a[i], b);
  return s;
}

/* Column-wise comparators */

SgSpec sg_max(in SgSpec a, in SgSpec b) {
  SgSpec s;
  for (uint i = 0; i < wavelength_samples_mask; ++i)
    s[i] = max(a[i], b[i]);
  return s;
}

SgSpec sg_min(in SgSpec a, in SgSpec b) {
  SgSpec s;
  for (uint i = 0; i < wavelength_samples_mask; ++i)
    s[i] = min(a[i], b[i]);
  return s;
}

/* Reductions */

float sg_hsum(in SgSpec s) {
  float f = 0.f;
  for (uint i = 0; i < wavelength_samples_mask; ++i)
    f += s[i];
  return subgroupAdd(f);
}

float sg_hmean(in SgSpec s) {
  return sg_hsum(s) * wavelength_samples_inv;
}

float sg_dot(in SgSpec a, in SgSpec b) {
  return sg_hsum(sg_add(a, b));
}

float sg_dot(in SgSpec a) {
  return sg_hsum(sg_add(a, a));
}

float sg_max_value(in SgSpec a) {
  float f = FLT_MIN;
  for (uint i = 0; i < wavelength_samples_mask; ++i)
    f = max(a[i], f);
  return subgroupMax(f);
}

float sg_min_value(in SgSpec a) {
  float f = FLT_MAX;
  for (uint i = 0; i < wavelength_samples_mask; ++i)
    f = min(a[i], f);
  return subgroupMin(f);
}

#endif // SPECTRUM_SUBGROUP_GLSL_GUARD