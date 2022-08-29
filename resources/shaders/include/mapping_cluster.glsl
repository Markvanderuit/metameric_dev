#ifndef MAPPING_CLUSTER_GLSL_GUARD
#define MAPPING_CLUSTER_GLSL_GUARD

#include <cmfs_cluster.glsl>

struct ClMapp {
  ClCMFS cmfs;
  ClSpec illuminant;
  uint   n_scatters;
};

#define cl_mapp_scatter(dst, src)                   \
  { cl_cmfs_scatter(dst.cmfs, src.cmfs)             \
    cl_spec_scatter(dst.illuminant, src.illuminant) \
    dst.n_scatters = src.n_scatters;                }

#define cl_mapp_gather(dst, src)                    \
  { cl_cmfs_gather(dst.cmfs, src.cmfs)              \
    cl_spec_gather(dst.illuminant, src.illuminant)  \
    dst.n_scatters = src.n_scatters;                }

ClCMFS finalize_mapp(in ClMapp m) {
  // Normalization factor is applied over the illuminant
  float k = 1.f / cl_hsum(m.cmfs[1] * m.illuminant);

  // return k * cmfs * illum
  return cl_mul(cl_mul(m.cmfs, m.illuminant), k);
}

ClCMFS finalize_mapp(in ClMapp m, in ClSpec sd) {
  ClSpec reflp = m.n_scatters == 0 ? ClSpec(1.f) : pow(sd, ClSpec(m.n_scatters));
  ClSpec illum = reflp * m.illuminant;

  // Normalization factor is applied over the unscattered illuminant
  float k = 1.f / cl_hsum(m.cmfs[1] * m.illuminant);
  
  // return k * cmfs * illum
  return cl_mul(cl_mul(m.cmfs, illum), k);
}

#endif // MAPPING_CLUSTER_GLSL_GUARD
