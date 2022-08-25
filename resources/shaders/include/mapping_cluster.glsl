#ifndef MAPPING_CLUSTER_GLSL_GUARD
#define MAPPING_CLUSTER_GLSL_GUARD

#include <cmfs_cluster.glsl>

struct SgMapp {
  SgCMFS cmfs;
  SgSpec illuminant;
  uint   n_scatters;
};

#define sg_mapp_scatter(dst, src)                   \
  { sg_cmfs_scatter(dst.cmfs, src.cmfs)             \
    sg_spec_scatter(dst.illuminant, src.illuminant) \
    dst.n_scatters = src.n_scatters;                }

#define sg_mapp_gather(dst, src)                    \
  { sg_cmfs_gather(dst.cmfs, src.cmfs)              \
    sg_spec_gather(dst.illuminant, src.illuminant)  \
    dst.n_scatters = src.n_scatters;                }

SgCMFS finalize_mapping(in SgMapp m) {
  // Normalization factor is applied over the illuminant
  float k = 1.f / sg_hsum(m.cmfs.y * m.illuminant);

  // return k * cmfs * illum
  return sg_mul(sg_mul(m.cmfs, m.illuminant), k);
}

SgCMFS finalize_mapping(in SgMapp m, in SgSpec sd) {
  SgSpec refl_mul = m.n_scatters == 0
                  ? SgSpec(1.f)
                  : pow(sd, SgSpec(m.n_scatters));
  SgSpec illuminant = refl_mul * m.illuminant;

  // Normalization factor is applied over the unscattered illuminant
  // TODO extract and precompute
  float k = 1.f / sg_hsum(m.cmfs.y * m.illuminant);
  
  // return k * cmfs * illum
  return sg_mul(sg_mul(m.cmfs, illuminant), k);
}

#endif // MAPPING_CLUSTER_GLSL_GUARD
