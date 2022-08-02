#ifndef MAPPING_SUBGROUP_GLSL_GUARD
#define MAPPING_SUBGROUP_GLSL_GUARD

#include <cmfs_subgroup.glsl>

/* Base mapping object */

struct MappingLoadType {
  float[3][wavelength_samples] cmfs;
  float[wavelength_samples]    illuminant;
  uint                         n_scatters;
};

struct MappingType {
  CMFS cmfs;
  Spec illuminant;
  uint n_scatters;
};

/* Mapping functions */

CMFS finalize_mapping(in MappingType m) {
  // Normalization factor is applied over the illuminant
  // TODO extract and precompute
  float k = 1.f / s_hsum(s_mul(m.cmfs[1], m.illuminant));

  // return k * cmfs * illum
  return c_mul(c_mul(m.cmfs, m.illuminant), k);
}

CMFS finalize_mapping(in MappingType m, in Spec sd) {
  Spec refl_mul = m.n_scatters == 0
                ? s_constr(1.f)
                : s_pow(sd, float(m.n_scatters));
  Spec illuminant = s_mul(refl_mul, m.illuminant);

  // Normalization factor is applied over the unscattered illuminant
  // TODO extract and precompute
  float k = 1.f / s_hsum(s_mul(m.cmfs[1], m.illuminant));
  
  // return k * cmfs * illum
  return c_mul(c_mul(m.cmfs, illuminant), k);
}

#endif // MAPPING_SUBGROUP_GLSL_GUARD