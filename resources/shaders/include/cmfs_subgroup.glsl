#ifndef CMFS_SUBGROUP_GLSL_GUARD
#define CMFS_SUBGROUP_GLSL_GUARD

#include <spectrum_subgroup.glsl>

/* Subgroup color matching functions object */

#define SgCMFS float[3][sg_wavelength_samples]

/* Constructors */

SgCMFS sg_cmfs(in float f) {
  SgSpec s = sg_spectrum(f);
  return SgCMFS(s, s, s);
}

// Scatter CMFS to SgCMFS
#define sg_scatter_cmfs(dst, src)     \
  { sg_scatter_spec(dst[0], src[0])   \
    sg_scatter_spec(dst[1], src[1])   \
    sg_scatter_spec(dst[2], src[2]) } \

/* Component-wise math operators */

SgCMFS sg_mul(in SgCMFS cmfs, in float f) {
  return SgCMFS(sg_mul(cmfs[0], f),
                sg_mul(cmfs[1], f),
                sg_mul(cmfs[2], f));
}

SgCMFS sg_div(in SgCMFS cmfs, in float f) {
  return SgCMFS(sg_div(cmfs[0], f),
                sg_div(cmfs[1], f),
                sg_div(cmfs[2], f));
}

SgCMFS sg_add(in SgCMFS cmfs, in float f) {
  return SgCMFS(sg_add(cmfs[0], f),
                sg_add(cmfs[1], f),
                sg_add(cmfs[2], f));
}

SgCMFS sg_sub(in SgCMFS cmfs, in float f) {
  return SgCMFS(sg_sub(cmfs[0], f),
                sg_sub(cmfs[1], f),
                sg_sub(cmfs[2], f));
}

/* Column-wise math operators */

SgCMFS sg_mul(in SgCMFS cmfs, in SgSpec s) {
  return SgCMFS(sg_mul(cmfs[0], s),
                sg_mul(cmfs[1], s),
                sg_mul(cmfs[2], s));
}

SgCMFS sg_div(in SgCMFS cmfs, in SgSpec s) {
  return SgCMFS(sg_div(cmfs[0], s),
                sg_div(cmfs[1], s),
                sg_div(cmfs[2], s));
}

SgCMFS sg_add(in SgCMFS cmfs, in SgSpec s) {
  return SgCMFS(sg_add(cmfs[0], s),
                sg_add(cmfs[1], s),
                sg_add(cmfs[2], s));
}

SgCMFS sg_sub(in SgCMFS cmfs, in SgSpec s) {
  return SgCMFS(sg_sub(cmfs[0], s),
                sg_sub(cmfs[1], s),
                sg_sub(cmfs[2], s));
}

/* Matrix math operators */

vec3 sg_matmul(in SgCMFS cmfs, in SgSpec s) {
  return vec3(sg_hsum(sg_mul(cmfs[0], s)),
              sg_hsum(sg_mul(cmfs[1], s)),
              sg_hsum(sg_mul(cmfs[2], s)));
}

#endif // CMFS_SUBGROUP_GLSL_GUARD