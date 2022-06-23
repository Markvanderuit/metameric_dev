#extension GL_KHR_shader_subgroup_basic      : require
#extension GL_KHR_shader_subgroup_arithmetic : require

/* Metameric's spectral range layout */
#define wavelength_samples 31
#define wavelength_min     400.f
#define wavelength_max     710.f

/* Derived variables from Metameric's spectral layout */
#define wavelength_range   (wavelength_max - wavelength_min)
#define wavelength_ssize   (wavelength_range / float(wavelength_samples))
#define wavelength_ssinv   (1.f / wavelength_ssize)

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

vec3 test_fun(in vec3 v) {
  return 4 * v;
}