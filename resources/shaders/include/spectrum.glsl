#ifndef SPECTRUM_GLSL_GUARD
#define SPECTRUM_GLSL_GUARD

#extension GL_KHR_shader_subgroup_basic      : require
#extension GL_KHR_shader_subgroup_arithmetic : require

/* Define metameric's spectral range layout */
const float wavelength_min     = MET_WAVELENGTH_MIN;
const float wavelength_max     = MET_WAVELENGTH_MAX;
const uint  wavelength_samples = MET_WAVELENGTH_SAMPLES;

/* Define derived variables from metameric's spectral range layout */
const float wavelength_range = wavelength_max - wavelength_min;
const float wavelength_ssize = wavelength_range / float(wavelength_samples);
const float wavelength_ssinv = float(wavelength_samples) / wavelength_range;

/* Base Spectrum object */
#define Spectrum float[wavelength_samples]
// struct Spectrum {
//   float values[wavelength_samples];
// };

Spectrum constr_spectrum(in float constant) {
  Spectrum s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = constant;
  return s;
}

Spectrum add(in Spectrum a, in Spectrum b) {
  Spectrum s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = a[i] + b[i];
  return s;
}

Spectrum div(in Spectrum a, in Spectrum b) {
  Spectrum s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = a[i] / b[i];
  return s;
}

Spectrum maxv(in Spectrum a, in Spectrum b) {
  Spectrum s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = max(a[i], b[i]);
  return s;
}

Spectrum maxv(in Spectrum a, in float b) {
  Spectrum s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = max(a[i], b);
  return s;
}

Spectrum minv(in Spectrum a, in Spectrum b) {
  Spectrum s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = min(a[i], b[i]);
  return s;
}

Spectrum minv(in Spectrum a, in float b) {
  Spectrum s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = min(a[i], b);
  return s;
}

// Spectrum sub(in Spectrum a, in Spectrum b, in uint i) {
//   Spectrum s;
//   s.values[i] = a.values[i] - b.values[i];
//   return s;
// }

// Spectrum mul(in Spectrum a, in Spectrum b, in uint i) {
//   Spectrum s;
//   s.values[i] = a.values[i] * b.values[i];
//   return s;
// }

// Spectrum div(in Spectrum a, in Spectrum b, in uint i) {
//   Spectrum s;
//   s.values[i] = a.values[i] / b.values[i];
//   return s;
// }

// float mean(in Spectrum s) {
//   float f = 0.f;
//   for (uint i = 0; i < wavelength_samples; ++i)
//     f += s.values[i];
//   return f;
// }

float wavelength_at_index(uint i) {
  return wavelength_min + wavelength_ssize * (float(i) + .5f);
}

uint index_at_wavelength(float wvl) {
  uint i = uint((wvl - wavelength_min) * wavelength_ssinv);
  return min(i, wavelength_samples - 1);
}

vec3 test_fun(in vec3 v) {
  return 4 * v;
}

#endif // SPECTRUM_GLSL_GUARD