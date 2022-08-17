#ifndef SPECTRUM_INVOC_GLSL_GUARD
#define SPECTRUM_INVOC_GLSL_GUARD

#include <constants_spectrum.glsl>

/* Per-invocation Spectrum object */

#define Spec float[wavelength_samples]
#define Mask bool[wavelength_samples]

/* Constructors */

Spec in_spectrum(in float f) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = f;
  return s;
}

/* Component-wise math operators */

Spec in_mul(in Spec a, in float f) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = a[i] * f;
  return s;
}

Spec in_div(in Spec a, in float f) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = a[i] / f;
  return s;
}

Spec in_add(in Spec a, in float f) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = a[i] + f;
  return s;
}

Spec in_sub(in Spec a, in float f) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = a[i] - f;
  return s;
}

Spec in_pow(in Spec v, in float p) {
  Spec s = v;

  if (p > 1.f)
    for (uint i = 0; i < wavelength_samples; ++i)
      s[i] = pow(s[i], p);

  return s;
}

/* Column-wise math operators */

Spec in_mul(in Spec a, in Spec b) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = a[i] * b[i];
  return s;
}

Spec in_div(in Spec a, in Spec b) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = a[i] / b[i];
  return s;
}

Spec in_add(in Spec a, in Spec b) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = a[i] + b[i];
  return s;
}

Spec in_sub(in Spec a, in Spec b) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = a[i] - b[i];
  return s;
}

Spec in_pow(in Spec v, in Spec p) {
  Spec s = v;

  for (uint i = 0; i < wavelength_samples; ++i)
    if (p[i] > 1.f)
      s[i] = pow(s[i], p[i]);

  return s;
}

/* Component-wise comparators */

Mask in_eq(in Spec a, in float b) {
  Mask m;
  for (uint i = 0; i < wavelength_samples; ++i)
    m[i] = a[i] == b;
  return m;
}

Mask in_neq(in Spec a, in float b) {
  Mask m;
  for (uint i = 0; i < wavelength_samples; ++i)
    m[i] = a[i] != b;
  return m;
}

Mask in_gr(in Spec a, in float b) {
  Mask m;
  for (uint i = 0; i < wavelength_samples; ++i)
    m[i] = a[i] > b;
  return m;
}

Mask in_ge(in Spec a, in float b) {
  Mask m;
  for (uint i = 0; i < wavelength_samples; ++i)
    m[i] = a[i] >= b;
  return m;
}

Mask in_lr(in Spec a, in float b) {
  Mask m;
  for (uint i = 0; i < wavelength_samples; ++i)
    m[i] = a[i] < b;
  return m;
}

Mask in_le(in Spec a, in float b) {
  Mask m;
  for (uint i = 0; i < wavelength_samples; ++i)
    m[i] = a[i] <= b;
  return m;
}

Spec in_max(in Spec a, in float b) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = max(a[i], b);
  return s;
}

Spec in_min(in Spec a, in float b) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = min(a[i], b);
  return s;
}

Spec in_clamp(in Spec a, in float b, in float c) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = clamp(a[i], b, c);
  return s;
}

Spec in_select(in Mask m, in float a, in Spec b) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = m[i] ? a : b[i];
  return s;
}

Spec in_select(in Mask m, in Spec a, in float b) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = m[i] ? a[i] : b;
  return s;
}

/* Column-wise comparators */

Mask in_eq(in Spec a, in Spec b) {
  Mask m;
  for (uint i = 0; i < wavelength_samples; ++i)
    m[i] = a[i] == b[i];
  return m;
}

Mask in_neq(in Spec a, in Spec b) {
  Mask m;
  for (uint i = 0; i < wavelength_samples; ++i)
    m[i] = a[i] != b[i];
  return m;
}

Mask in_gr(in Spec a, in Spec b) {
  Mask m;
  for (uint i = 0; i < wavelength_samples; ++i)
    m[i] = a[i] > b[i];
  return m;
}

Mask in_ge(in Spec a, in Spec b) {
  Mask m;
  for (uint i = 0; i < wavelength_samples; ++i)
    m[i] = a[i] >= b[i];
  return m;
}

Mask in_lr(in Spec a, in Spec b) {
  Mask m;
  for (uint i = 0; i < wavelength_samples; ++i)
    m[i] = a[i] < b[i];
  return m;
}

Mask in_le(in Spec a, in Spec b) {
  Mask m;
  for (uint i = 0; i < wavelength_samples; ++i)
    m[i] = a[i] <= b[i];
  return m;
}

Spec in_max(in Spec a, in Spec b) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = max(a[i], b[i]);
  return s;
}

Spec in_min(in Spec a, in Spec b) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = min(a[i], b[i]);
  return s;
}

Spec in_clamp(in Spec a, in Spec b, in Spec c) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = clamp(a[i], b[i], c[i]);
  return s;
}

Spec in_select(in Mask m, in Spec a, in Spec b) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = m[i] ? a[i] : b[i];
  return s;
}

/* Reductions */

// TODO extract to unrelated shader
float in_hsum(vec3 v) {
  return v.x + v.y + v.z;
}

float in_hsum(in Spec s) {
  float f = s[0];
  for (uint i = 1; i < wavelength_samples; ++i)
    f += s[i];
  return f;
}

float in_hmean(vec3 v) {
  return in_hsum(v) / float(3);
}

float in_hmean(in Spec s) {
  return in_hsum(s) * wavelength_samples_inv;
}

float in_dot(in Spec a, in Spec b) {
  return in_hsum(in_add(a, b));
}

float in_dot(in Spec a) {
  return in_hsum(in_add(a, a));
}

float in_max_value(in Spec a) {
  float f = a[0];
  for (uint i = 1; i < wavelength_samples; ++i)
    f = max(f, a[i]);
  return f;
}

float in_min_value(in Spec a) {
  float f = a[0];
  for (uint i = 1; i < wavelength_samples; ++i)
    f = min(f, a[i]);
  return f;
}

/* Miscellaneous */

float wavelength_at_index(uint i) {
  return wavelength_min + wavelength_ssize * (float(i) + .5f);
}

uint index_at_wavelength(float wvl) {
  uint i = uint((wvl - wavelength_min) * wavelength_ssinv);
  return min(i, wavelength_samples - 1);
}

#endif // SPECTRUM_INVOC_GLSL_GUARD