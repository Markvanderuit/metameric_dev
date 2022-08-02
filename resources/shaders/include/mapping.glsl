#ifndef MAPPING_GLSL_GUARD
#define MAPPING_GLSL_GUARD

#include <cmfs.glsl>

/* Base mapping object */

struct MappingType {
  CMFS cmfs;
  Spec illuminant;
  uint n_scatters;
};

/* Mapping functions */

CMFS finalize_mapping(in MappingType m) {
  // Normalization factor is applied over the illuminant
  // TODO extract and precompute
  float k = 1.f / ssum(mul(m.cmfs[1], m.illuminant));

  // return k * cmfs * illum
  return mul(mul(m.cmfs, m.illuminant), k);
}

CMFS finalize_mapping(in MappingType m, in Spec sd) {
  // If n_scatters > 0, multiply illuminant by reflectance to simulate
  // repeated scattering of indirect lighting
  Spec refl_mul = m.n_scatters == 0
                ? constr_spec(1.f)
                : powv(sd, m.n_scatters);
  Spec illuminant = mul(refl_mul, m.illuminant);

  // Normalization factor is applied over the unscattered illuminant
  // TODO extract and precompute
  float k = 1.f / ssum(mul(m.cmfs[1], m.illuminant));

  // return k * cmfs * illum
  return mul(mul(m.cmfs, illuminant), k);
}

#endif // MAPPING_GLSL_GUARD