#ifndef CMFS_INVOC_GLSL_GUARD
#define CMFS_INVOC_GLSL_GUARD

#include <spectrum_invoc.glsl>

/* Per-invocation CMFS object */

#define InCMFS float[3][wavelength_samples]

/* Constructors */

InCMFS in_cmfs(in float f) {
  InSpec s = in_spectrum(f);
  return InCMFS(s, s, s);
}

/* Component-wise math operators */

InCMFS in_mul(in InCMFS cmfs, in float f) {
  return InCMFS(in_mul(cmfs[0], f), in_mul(cmfs[1], f), in_mul(cmfs[2], f));
}

InCMFS in_div(in InCMFS cmfs, in float f) {
  return InCMFS(in_div(cmfs[0], f), in_div(cmfs[1], f), in_div(cmfs[2], f));
}

InCMFS in_add(in InCMFS cmfs, in float f) {
  return InCMFS(in_add(cmfs[0], f), in_add(cmfs[1], f), in_add(cmfs[2], f));
}

InCMFS in_sub(in InCMFS cmfs, in float f) {
  return InCMFS(in_sub(cmfs[0], f), in_sub(cmfs[1], f), in_sub(cmfs[2], f));
}

/* Column-wise math operators */

InCMFS in_mul(in InCMFS cmfs, in InSpec s) {
  return InCMFS(in_mul(cmfs[0], s), in_mul(cmfs[1], s), in_mul(cmfs[2], s));
}

InCMFS in_div(in InCMFS cmfs, in InSpec s) {
  return InCMFS(in_div(cmfs[0], s), in_div(cmfs[1], s), in_div(cmfs[2], s));
}

InCMFS in_add(in InCMFS cmfs, in InSpec s) {
  return InCMFS(in_add(cmfs[0], s), in_add(cmfs[1], s), in_add(cmfs[2], s));
}

InCMFS in_sub(in InCMFS cmfs, in InSpec s) {
  return InCMFS(in_sub(cmfs[0], s), in_sub(cmfs[1], s), in_sub(cmfs[2], s));
}

/* Matrix math operators */

vec3 in_mmul(in InCMFS cmfs, in InSpec s) {
  return vec3(in_hsum(in_mul(cmfs[0], s)),
              in_hsum(in_mul(cmfs[1], s)),
              in_hsum(in_mul(cmfs[2], s)));
}

InSpec in_mmul(in vec3 v, in InCMFS cmfs) {
  return in_add(in_add(in_mul(cmfs[0], v.x), 
                       in_mul(cmfs[1], v.y)), 
                       in_mul(cmfs[2], v.z));
}

#endif // CMFS_INVOC_GLSL_GUARD