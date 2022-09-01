#ifndef SPECTRUM_INVOC_GLSL_GUARD
#define SPECTRUM_INVOC_GLSL_GUARD

#include <math.glsl>
#include <spectrum.glsl>

/* Per-invocation Spectrum object */

#define InSpec float[wavelength_samples]
#define InMask bool[wavelength_samples]

// Define to perform commonly occuring iteration
#define in_bin_iter(__b) for (uint __b = 0; __b < wavelength_samples; ++__b)

/* Constructors */

InSpec in_spectrum(in float f) {
  InSpec s;
  in_bin_iter(i) s[i] = f;
  return s;
}

/* Component-wise math operators */

InSpec in_mul(in InSpec s, in float f) {
  in_bin_iter(i) s[i] *= f;
  return s;
}

InSpec in_div(in InSpec s, in float f) {
  in_bin_iter(i) s[i] /= f;
  return s;
}

InSpec in_add(in InSpec s, in float f) {
  in_bin_iter(i) s[i] += f;
  return s;
}

InSpec in_sub(in InSpec s, in float f) {
  in_bin_iter(i) s[i] -= f;
  return s;
}

InSpec in_pow(in InSpec s, in float p) {
  if (p > 1.f)
    in_bin_iter(i) s[i] = pow(s[i], p);
  return s;
}

/* Column-wise math operators */

InSpec in_mul(in InSpec s, in InSpec b) {
  in_bin_iter(i) s[i] *= b[i];
  return s;
}

InSpec in_div(in InSpec s, in InSpec b) {
  in_bin_iter(i) s[i] /= b[i];
  return s;
}

InSpec in_add(in InSpec s, in InSpec b) {
  in_bin_iter(i) s[i] += b[i];
  return s;
}

InSpec in_sub(in InSpec s, in InSpec b) {
  in_bin_iter(i) s[i] -= b[i];
  return s;
}

InSpec in_pow(in InSpec s, in InSpec p) {
  in_bin_iter(i) 
    if (p[i] > 1.f)
      s[i] = pow(s[i], p[i]);
  return s;
}

/* Component-wise logic comparators */

InMask in_eq(in InSpec a, in float b) {
  InMask m;
  in_bin_iter(i) m[i] = a[i] == b;
  return m;
}

InMask in_neq(in InSpec a, in float b) {
  InMask m;
  in_bin_iter(i) m[i] = a[i] != b;
  return m;
}

InMask in_gr(in InSpec a, in float b) {
  InMask m;
  in_bin_iter(i) m[i] = a[i] > b;
  return m;
}

InMask in_ge(in InSpec a, in float b) {
  InMask m;
  in_bin_iter(i) m[i] = a[i] >= b;
  return m;
}

InMask in_lr(in InSpec a, in float b) {
  InMask m;
  in_bin_iter(i) m[i] = a[i] < b;
  return m;
}

InMask in_le(in InSpec a, in float b) {
  InMask m;
  in_bin_iter(i)  m[i] = a[i] <= b;
  return m;
}

InSpec in_max(in InSpec s, in float b) {
  in_bin_iter(i) s[i] = max(s[i], b);
  return s;
}

InSpec in_min(in InSpec s, in float b) {
  in_bin_iter(i) s[i] = min(s[i], b);
  return s;
}

InSpec in_clamp(in InSpec s, in float minv, in float maxv) {
  in_bin_iter(i)  s[i] = clamp(s[i], minv, maxv);
  return s;
}

InSpec in_select(in InMask m, in float a, in float b) {
  InSpec s;
  in_bin_iter(i) s[i] = m[i] ? a : b;
  return s;
}

InSpec in_select(in InMask m, in float a, in InSpec s) {
  in_bin_iter(i) s[i] = m[i] ? a : s[i];
  return s;
}

InSpec in_select(in InMask m, in InSpec s, in float b) {
  in_bin_iter(i) s[i] = m[i] ? s[i] : b;
  return s;
}

/* Column-wise comparators */

InMask in_eq(in InSpec a, in InSpec b) {
  InMask m;
  in_bin_iter(i) m[i] = a[i] == b[i];
  return m;
}

InMask in_neq(in InSpec a, in InSpec b) {
  InMask m;
  in_bin_iter(i) m[i] = a[i] != b[i];
  return m;
}

InMask in_gr(in InSpec a, in InSpec b) {
  InMask m;
  in_bin_iter(i) m[i] = a[i] > b[i];
  return m;
}

InMask in_ge(in InSpec a, in InSpec b) {
  InMask m;
  in_bin_iter(i)  m[i] = a[i] >= b[i];
  return m;
}

InMask in_lr(in InSpec a, in InSpec b) {
  InMask m;
  in_bin_iter(i) m[i] = a[i] < b[i];
  return m;
}

InMask in_le(in InSpec a, in InSpec b) {
  InMask m;
  in_bin_iter(i) m[i] = a[i] <= b[i];
  return m;
}

InSpec in_max(in InSpec s, in InSpec o) {
  in_bin_iter(i) s[i] = max(s[i], o[i]);
  return s;
}

InSpec in_min(in InSpec s, in InSpec o) {
  in_bin_iter(i) s[i] = min(s[i], o[i]);
  return s;
}

InSpec in_clamp(in InSpec s, in InSpec minv, in InSpec maxv) {
  in_bin_iter(i) s[i] = clamp(s[i], minv[i], maxv[i]);
  return s;
}

InSpec in_select(in InMask m, in InSpec s, in InSpec o) {
  in_bin_iter(i) s[i] = m[i] ? s[i] : o[i];
  return s;
}

/* Reductions */

float in_hsum(in InSpec s) {
  float f = 0.f;
  in_bin_iter(i) f += s[i];
  return f;
}

float in_hmean(in InSpec s) {
  return wavelength_samples_inv * in_hsum(s);
}

float in_dot(in InSpec a, in InSpec b) {
  return in_hsum(in_add(a, b));
}

float in_dot(in InSpec a) {
  return in_hsum(in_add(a, a));
}

float in_max_value(in InSpec a) {
  float f = FLT_MIN;
  in_bin_iter(i) f = max(f, a[i]);
  return f;
}

float in_min_value(in InSpec a) {
  float f = FLT_MAX;
  in_bin_iter(i) f = min(f, a[i]);
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