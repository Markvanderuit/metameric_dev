#ifndef MAPPING_SUBGROUP_GLSL_GUARD
#define MAPPING_SUBGROUP_GLSL_GUARD

#include <cmfs_subgroup.glsl>

/* Per-subgroup mapping object */

struct SgMapp {
  SgCMFS cmfs;
  SgSpec illuminant;
};

// Scatter Mapping to SgMapp
#define sg_mapp_scatter(dst, src)                  \
 { sg_cmfs_scatter(dst.cmfs, src.cmfs)             \
   sg_spec_scatter(dst.illuminant, src.illuminant) }

/* Mapping functions */

SgCMFS finalize_mapp(in SgMapp m) {
  // Normalization factor is applied over the illuminant
  // TODO extract and precompute
  float k = 1.f / sg_hsum(sg_mul(m.cmfs[1], m.illuminant));

  // return k * cmfs * illum
  return sg_mul(sg_mul(m.cmfs, m.illuminant), k);
}

#endif // MAPPING_SUBGROUP_GLSL_GUARD