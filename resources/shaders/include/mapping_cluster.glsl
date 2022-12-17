#ifndef MAPPING_CLUSTER_GLSL_GUARD
#define MAPPING_CLUSTER_GLSL_GUARD

#include <cmfs_cluster.glsl>

struct ClMapp {
  ClCMFS cmfs;
  ClSpec illuminant;
};

#define cl_mapp_scatter(dst, src)                   \
  { cl_cmfs_scatter(dst.cmfs, src.cmfs)             \
    cl_spec_scatter(dst.illuminant, src.illuminant) }

#define cl_mapp_gather(dst, src)                    \
  { cl_cmfs_gather(dst.cmfs, src.cmfs)              \
    cl_spec_gather(dst.illuminant, src.illuminant)  }

ClCMFS finalize_mapp(in ClMapp m) {
  // Normalization factor is applied over the illuminant
  float k = 1.f / cl_hsum(m.cmfs[1] * m.illuminant);

  // return k * cmfs * illum
  return cl_mul(cl_mul(m.cmfs, m.illuminant), k);
}

#endif // MAPPING_CLUSTER_GLSL_GUARD
