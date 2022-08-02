#ifndef SPECTRUM_SUBGROUP_GLSL_GUARD
#define SPECTRUM_SUBGROUP_GLSL_GUARD

// Enable subgroup extensions
#extension GL_KHR_shader_subgroup_basic      : require
#extension GL_KHR_shader_subgroup_arithmetic : require

// Define max/min single precision float constants for masked components
// #define FLT_MAX 3.402823466e+38
// #define FLT_MIN 1.175494351e-38

// Define metameric's spectral range layout
const float wavelength_min     = MET_WAVELENGTH_MIN;
const float wavelength_max     = MET_WAVELENGTH_MAX;
const uint  wavelength_samples = MET_WAVELENGTH_SAMPLES;

// Define derived variables from metameric's spectral range layout
const float wavelength_range = wavelength_max - wavelength_min;
const float wavelength_ssize = wavelength_range / float(wavelength_samples);
const float wavelength_ssinv = float(wavelength_samples) / wavelength_range;

// Define derived variables for subgroup operations
const uint sg_size     = gl_SubgroupSize;
const uint sg_iters    = (wavelength_samples + (sg_size - 1)) / sg_size;
const uint sg_iter_mod = sg_size * sg_iters - wavelength_samples;
const uint sg_size_end = sg_size - sg_iter_mod;
const uint sg_masked_iters 
                       = (sg_iters - 1 )
                       + (gl_SubgroupInvocationID < sg_size_end ? 1 : 0);

/* Base Spectrum object */

#define Spec float[sg_iters];

/* Constructors */

Spec s_constr(in float f) {
  Spec s;
  for (uint i = 0; i < sg_masked_iters; ++i)
    s[i] = f;
  return s;
}

/* Component-wise math operators */

Spec s_mul(in Spec a, in float f) {
  Spec s;
  for (uint i = 0; i < sg_masked_iters; ++i)
    s[i] = a[i] * f;
  return s;
}

Spec s_div(in Spec a, in float f) {
  Spec s;
  for (uint i = 0; i < sg_masked_iters; ++i)
    s[i] = a[i] / f;
  return s;
}

Spec s_add(in Spec a, in float f) {
  Spec s;
  for (uint i = 0; i < sg_masked_iters; ++i)
    s[i] = a[i] + f;
  return s;
}

Spec s_sub(in Spec a, in float f) {
  Spec s;
  for (uint i = 0; i < sg_masked_iters; ++i)
    s[i] = a[i] - f;
  return s;
}

Spec s_pow(in Spec a, in float p) {
  Spec s;
  for (uint i = 0; i < sg_masked_iters; ++i)
    s[i] = pow(a[i], p);
  return s;
}

/* Column-wise math operators */

Spec s_mul(in Spec a, in Spec b) {
  Spec s;
  for (uint i = 0; i < sg_masked_iters; ++i)
    s[i] = a[i] * b[i];
  return s;
}

Spec s_div(in Spec a, in Spec b) {
  Spec s;
  for (uint i = 0; i < sg_masked_iters; ++i)
    s[i] = a[i] / b[i];
  return s;
}

Spec s_add(in Spec a, in Spec b) {
  Spec s;
  for (uint i = 0; i < sg_masked_iters; ++i)
    s[i] = a[i] + b[i];
  return s;
}

Spec s_sub(in Spec a, in Spec b) {
  Spec s;
  for (uint i = 0; i < sg_masked_iters; ++i)
    s[i] = a[i] - b[i];
  return s;
}

Spec s_pow(in Spec a, in Spec p) {
  Spec s;
  for (uint i = 0; i < sg_masked_iters; ++i)
    s[i] = pow(a[i], p[i]);
  return s;
}

/* Component-wise comparators */

Spec s_max(in Spec a, in float b) {
  Spec s;
  for (uint i = 0; i < sg_masked_iters; ++i)
    s[i] = max(a[i], b);
  return s;
}

Spec s_min(in Spec a, in float b) {
  Spec s;
  for (uint i = 0; i < sg_masked_iters; ++i)
    s[i] = min(a[i], b);
  return s;
}

/* Column-wise comparators */

Spec s_max(in Spec a, in Spec b) {
  Spec s;
  for (uint i = 0; i < sg_masked_iters; ++i)
    s[i] = max(a[i], b[i]);
  return s;
}

Spec s_min(in Spec a, in Spec b) {
  Spec s;
  for (uint i = 0; i < sg_masked_iters; ++i)
    s[i] = min(a[i], b[i]);
  return s;
}

/* Reductions */

float s_hsum(in Spec s) {
  float f = 0.f;
  for (uint i = 0; i < sg_masked_iters; ++i)
    f += s[i];
  return subgroupAdd(f);
}

float s_hmean(in Spec s) {
  return s_hsum(s) / float(wavelength_samples);
}

float s_dot(in Spec a, in Spec b) {
  return s_hsum(s_add(a, b));
}

float s_dot(in Spec a) {
  return s_hsum(s_add(a, a));
}

float s_max_value(in Spec a) {
  float f = FLT_MIN;
  for (uint i = 0; i < sg_masked_iters; ++i)
    f = max(a[i], f);
  return subgroupMax(f);
}

float s_min_value(in Spec a) {
  float f = FLT_MAX;
  for (uint i = 0; i < sg_masked_iters; ++i)
    f = min(a[i], f);
  return subgroupMin(f);
}

/* Miscellaneous */

#endif // SPECTRUM_SUBGROUP_GLSL_GUARD