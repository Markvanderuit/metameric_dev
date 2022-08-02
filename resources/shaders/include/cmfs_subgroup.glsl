#ifndef CMFS_SUBGROUP_GLSL_GUARD
#define CMFS_SUBGROUP_GLSL_GUARD

#include <spectrum_subgroup.glsl>

/* Base CMFS object */

#define CMFS float[3][sg_iters];

/* Constructors */

CMFS c_constr(in float f) {
  Spec s = s_constr(f);
  return CMFS(s, s, s);
}

/* Component-wise math operators */

CMFS c_mul(in CMFS cmfs, in float f) {
  return CMFS(s_mul(cmfs[0], f),
              s_mul(cmfs[1], f),
              s_mul(cmfs[2], f));
}

CMFS c_div(in CMFS cmfs, in float f) {
  return CMFS(s_div(cmfs[0], f),
              s_div(cmfs[1], f),
              s_div(cmfs[2], f));
}

CMFS c_add(in CMFS cmfs, in float f) {
  return CMFS(s_add(cmfs[0], f),
              s_add(cmfs[1], f),
              s_add(cmfs[2], f));
}

CMFS c_sub(in CMFS cmfs, in float f) {
  return CMFS(s_sub(cmfs[0], f),
              s_sub(cmfs[1], f),
              s_sub(cmfs[2], f));
}

/* Column-wise math operators */

CMFS c_mul(in CMFS cmfs, in Spec s) {
  return CMFS(s_mul(cmfs[0], s),
              s_mul(cmfs[1], s),
              s_mul(cmfs[2], s));
}

CMFS c_div(in CMFS cmfs, in Spec s) {
  return CMFS(s_div(cmfs[0], s),
              s_div(cmfs[1], s),
              s_div(cmfs[2], s));
}

CMFS c_add(in CMFS cmfs, in Spec s) {
  return CMFS(s_add(cmfs[0], s),
              s_add(cmfs[1], s),
              s_add(cmfs[2], s));
}

CMFS c_sub(in CMFS cmfs, in Spec s) {
  return CMFS(s_sub(cmfs[0], s),
              s_sub(cmfs[1], s),
              s_sub(cmfs[2], s));
}

/* Matrix math operators */

vec3 c_matmul(in CMFS cmfs, in Spec s) {
  return vec3(s_hsum(s_mul(cmfs[0], s)),
              s_hsum(s_mul(cmfs[1], s)),
              s_hsum(s_mul(cmfs[2], s)));
}

#endif // CMFS_SUBGROUP_GLSL_GUARD