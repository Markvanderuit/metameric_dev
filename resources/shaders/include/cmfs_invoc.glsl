#ifndef CMFS_INVOC_GLSL_GUARD
#define CMFS_INVOC_GLSL_GUARD

#include <spectrum_invoc.glsl>

/* Per-invocation CMFS object */

#define CMFS float[3][wavelength_samples]

/* Constructors */

CMFS in_cmfs(in float f) {
  Spec s = in_spectrum(f);
  return CMFS(s, s, s);
}

/* Component-wise math operators */

CMFS in_mul(in CMFS cmfs, in float f) {
  return CMFS(in_mul(cmfs[0], f),
              in_mul(cmfs[1], f),
              in_mul(cmfs[2], f));
}

CMFS in_div(in CMFS cmfs, in float f) {
  return CMFS(in_div(cmfs[0], f),
              in_div(cmfs[1], f),
              in_div(cmfs[2], f));
}

CMFS in_add(in CMFS cmfs, in float f) {
  return CMFS(in_add(cmfs[0], f),
              in_add(cmfs[1], f),
              in_add(cmfs[2], f));
}

CMFS in_sub(in CMFS cmfs, in float f) {
  return CMFS(in_sub(cmfs[0], f),
              in_sub(cmfs[1], f),
              in_sub(cmfs[2], f));
}

/* Column-wise math operators */

CMFS in_mul(in CMFS cmfs, in Spec s) {
  return CMFS(in_mul(cmfs[0], s),
              in_mul(cmfs[1], s),
              in_mul(cmfs[2], s));
}

CMFS in_div(in CMFS cmfs, in Spec s) {
  return CMFS(in_div(cmfs[0], s),
              in_div(cmfs[1], s),
              in_div(cmfs[2], s));
}

CMFS in_add(in CMFS cmfs, in Spec s) {
  return CMFS(in_add(cmfs[0], s),
              in_add(cmfs[1], s),
              in_add(cmfs[2], s));
}

CMFS in_sub(in CMFS cmfs, in Spec s) {
  return CMFS(in_sub(cmfs[0], s),
              in_sub(cmfs[1], s),
              in_sub(cmfs[2], s));
}

/* Matrix math operators */

vec3 in_matmul(in CMFS cmfs, in Spec s) {
  return vec3(in_hsum(in_mul(cmfs[0], s)),
              in_hsum(in_mul(cmfs[1], s)),
              in_hsum(in_mul(cmfs[2], s)));
}

#endif // CMFS_INVOC_GLSL_GUARD