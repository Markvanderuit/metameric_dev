#ifndef CMFS_GLSL_GUARD
#define CMFS_GLSL_GUARD

#include <spectrum.glsl>

/* Base CMFS object */

#define CMFS float[3][wavelength_samples]

/* Constructors */

CMFS constr_cmfs(in float f) {
  Spec s = constr_spec(f);
  return CMFS(s, s, s);
}

/* Component-wise math operators */

CMFS mul(in CMFS cmfs, in float f) {
  return CMFS(
    mul(cmfs[0], f),
    mul(cmfs[1], f),
    mul(cmfs[2], f)
  );
}

CMFS div(in CMFS cmfs, in float f) {
  return CMFS(
    div(cmfs[0], f),
    div(cmfs[1], f),
    div(cmfs[2], f)
  );
}

CMFS add(in CMFS cmfs, in float f) {
  return CMFS(
    add(cmfs[0], f),
    add(cmfs[1], f),
    add(cmfs[2], f)
  );
}

CMFS sub(in CMFS cmfs, in float f) {
  return CMFS(
    sub(cmfs[0], f),
    sub(cmfs[1], f),
    sub(cmfs[2], f)
  );
}

/* Column-wise math operators */

CMFS mul(in CMFS cmfs, in Spec s) {
  return CMFS(
    mul(cmfs[0], s),
    mul(cmfs[1], s),
    mul(cmfs[2], s)
  );
}

CMFS div(in CMFS cmfs, in Spec s) {
  return CMFS(
    div(cmfs[0], s),
    div(cmfs[1], s),
    div(cmfs[2], s)
  );
}

CMFS add(in CMFS cmfs, in Spec s) {
  return CMFS(
    add(cmfs[0], s),
    add(cmfs[1], s),
    add(cmfs[2], s)
  );
}

CMFS sub(in CMFS cmfs, in Spec s) {
  return CMFS(
    sub(cmfs[0], s),
    sub(cmfs[1], s),
    sub(cmfs[2], s)
  );
}

/* Matrix math operators */

vec3 matmul(in CMFS cmfs, in Spec s) {
  return vec3(
    ssum(mul(cmfs[0], s)),
    ssum(mul(cmfs[1], s)),
    ssum(mul(cmfs[2], s))
  );
}

#endif // CMFS_GLSL_GUARD