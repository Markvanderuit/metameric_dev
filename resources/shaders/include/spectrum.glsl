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

/* Base Spectrum objects */

#define Spec float[wavelength_samples]
#define CMFS float[3][wavelength_samples]

/* Oft-used mapping object */

struct MappingType {
  CMFS cmfs;
  Spec illuminant;
  uint n_scatterings;
};

Spec constr_spec(in float constant) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = constant;
  return s;
}

Spec add(in Spec a, in Spec b) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = a[i] + b[i];
  return s;
}

Spec mul(in Spec a, in Spec b) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = a[i] * b[i];
  return s;
}

Spec mul(in Spec a, in float f) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = a[i] * f;
  return s;
}

Spec div(in Spec a, in Spec b) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = a[i] / b[i];
  return s;
}

Spec maxv(in Spec a, in Spec b) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = max(a[i], b[i]);
  return s;
}

Spec maxv(in Spec a, in float b) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = max(a[i], b);
  return s;
}

Spec minv(in Spec a, in Spec b) {
  Spec s;
  for (uint i = 0; i < wavelength_samples; ++i)
    s[i] = min(a[i], b[i]);
  return s;
}

Spec minv(in Spec a, in float b) {
  Spec s;
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

float vsum(vec3 v) {
  return v.x + v.y + v.z;
}

float ssum(in Spec s) {
  float f = s[0];
  for (uint i = 1; i < wavelength_samples; ++i)
    f += s[i];
  return f;
}

float sdot(in Spec a, in Spec b) {
  float f = 0.f;
  for (uint i = 0; i < wavelength_samples; ++i)
    f += a[i] * b[i];
  return f;
}

float vmean(vec3 v) {
  return vsum(v) / float(3);
}

float smean(in Spec s) {
  return ssum(s) / float(wavelength_samples);
}

vec3 mul(in CMFS cmfs, in Spec s) {
  return vec3(
    ssum(mul(cmfs[0], s)),
    ssum(mul(cmfs[1], s)),
    ssum(mul(cmfs[2], s))
  );
}

float wavelength_at_index(uint i) {
  return wavelength_min + wavelength_ssize * (float(i) + .5f);
}

uint index_at_wavelength(float wvl) {
  uint i = uint((wvl - wavelength_min) * wavelength_ssinv);
  return min(i, wavelength_samples - 1);
}

#endif // SPECTRUM_GLSL_GUARD