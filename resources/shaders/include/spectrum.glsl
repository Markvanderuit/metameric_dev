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
struct Spectrum {
  float values[wavelength_samples];
};

Spectrum add(in Spectrum a, in Spectrum b, in uint i) {
  Spectrum s;
  s.values[i] = a.values[i] + b.values[i];
  return s;
}

Spectrum sub(in Spectrum a, in Spectrum b, in uint i) {
  Spectrum s;
  s.values[i] = a.values[i] - b.values[i];
  return s;
}

Spectrum mul(in Spectrum a, in Spectrum b, in uint i) {
  Spectrum s;
  s.values[i] = a.values[i] * b.values[i];
  return s;
}

Spectrum div(in Spectrum a, in Spectrum b, in uint i) {
  Spectrum s;
  s.values[i] = a.values[i] / b.values[i];
  return s;
}

float mean(in Spectrum s) {
  float f = 0.f;
  for (uint i = 0; i < wavelength_samples; ++i)
    f += s.values[i];
  return f;
}

vec3 test_fun(in vec3 v) {
  return 4 * v;
}

#endif // SPECTRUM_GLSL_GUARD