#ifndef MAPPING_INVOC_GLSL_GUARD
#define MAPPING_INVOC_GLSL_GUARD

#include <cmfs_invoc.glsl>

/* Per-invocation mapping object */

struct InMapp {
  InCMFS cmfs;
  InSpec illuminant;
  uint n_scatters;
};

/* Mapping functions */

InCMFS finalize_mapp(in InMapp m) {
  // Normalization factor is applied over the illuminant
  // TODO extract and precompute
  float k = 1.f / in_hsum(in_mul(m.cmfs[1], m.illuminant));

  // return k * cmfs * illum
  return in_mul(in_mul(m.cmfs, m.illuminant), k);
}

InCMFS finalize_mapp(in InMapp m, in InSpec sd) {
  // If n_scatters > 0, multiply illuminant by reflectance to simulate
  // repeated scattering of indirect lighting
  InSpec e = in_mul(m.illuminant, 
                  m.n_scatters == 0 ? in_spectrum(1.f) : in_pow(sd, float(m.n_scatters)));

  // Normalization factor is applied over the unscattered illuminant
  float k = 1.f / in_hsum(in_mul(m.cmfs[1], m.illuminant)); // TODO extract and precompute

  // return k * cmfs * illum
  return in_mul(in_mul(m.cmfs, e), k);
}

#endif // MAPPING_INVOC_GLSL_GUARD