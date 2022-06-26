#ifndef CMFS_GLSL_GUARD
#define CMFS_GLSL_GUARD

#include <spectrum.glsl>

struct CMFS {
  float values[3][wavelength_samples];
};

vec3 mul(in CMFS cmfs, in Spectrum s) {
  return vec3(mean(mul(Spectrum(cmfs[0]), s)),
              mean(mul(Spectrum(cmfs[1]), s)),
              mean(mul(Spectrum(cmfs[2]), s)));
};

#endif // CMFS_GLSL_GUARD