#ifndef MAPPING_SUBGROUP_GLSL_GUARD
#define MAPPING_SUBGROUP_GLSL_GUARD

#include <cmfs_subgroup.glsl>

/* Per-subgroup mapping object */

struct SgMapping {
  SgCMFS cmfs;
  SgSpec illuminant;
  uint   n_scatters;
};

/* Mapping functions */

SgCMFS finalize_mapping(in SgMapping m) {
  // Normalization factor is applied over the illuminant
  // TODO extract and precompute
  float k = 1.f / sg_hsum(sg_mul(m.cmfs[1], m.illuminant));

  // return k * cmfs * illum
  return sg_mul(sg_mul(m.cmfs, m.illuminant), k);
}

SgCMFS finalize_mapping(in SgMapping m, in SgSpec sd) {
  SgSpec refl_mul = m.n_scatters == 0
                  ? sg_spectrum(1.f)
                  : sg_pow(sd, float(m.n_scatters));
  SgSpec illuminant = sg_mul(refl_mul, m.illuminant);

  // Normalization factor is applied over the unscattered illuminant
  // TODO extract and precompute
  float k = 1.f / sg_hsum(sg_mul(m.cmfs[1], m.illuminant));
  
  // return k * cmfs * illum
  return sg_mul(sg_mul(m.cmfs, illuminant), k);
}

#endif // MAPPING_SUBGROUP_GLSL_GUARD