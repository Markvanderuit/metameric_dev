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
#define sg_cmfs_scatter(dst, src)     \
  { sg_spec_scatter(dst[0], src[0])   \
    sg_spec_scatter(dst[1], src[1])   \
    sg_spec_scatter(dst[2], src[2]) } \

/* Component-wise math operators */

SgCMFS sg_mul(in SgCMFS s, in float f) {
  return SgCMFS(sg_mul(s[0], f),
                sg_mul(s[1], f),
                sg_mul(s[2], f));
}

SgCMFS sg_div(in SgCMFS s, in float f) {
  return SgCMFS(sg_div(s[0], f),
                sg_div(s[1], f),
                sg_div(s[2], f));
}

SgCMFS sg_add(in SgCMFS s, in float f) {
  return SgCMFS(sg_add(s[0], f),
                sg_add(s[1], f),
                sg_add(s[2], f));
}

SgCMFS sg_sub(in SgCMFS s, in float f) {
  return SgCMFS(sg_sub(s[0], f),
                sg_sub(s[1], f),
                sg_sub(s[2], f));
}

/* Column-wise math operators */

SgCMFS sg_mul(in SgCMFS s, in SgSpec o) {
  return SgCMFS(sg_mul(s[0], o),
                sg_mul(s[1], o),
                sg_mul(s[2], o));
}

SgCMFS sg_div(in SgCMFS s, in SgSpec o) {
  return SgCMFS(sg_div(s[0], o),
                sg_div(s[1], o),
                sg_div(s[2], o));
}

SgCMFS sg_add(in SgCMFS s, in SgSpec o) {
  return SgCMFS(sg_add(s[0], o),
                sg_add(s[1], o),
                sg_add(s[2], o));
}

SgCMFS sg_sub(in SgCMFS s, in SgSpec o) {
  return SgCMFS(sg_sub(s[0], o),
                sg_sub(s[1], o),
                sg_sub(s[2], o));
}

/* Matrix math operators */

vec3 sg_matmul(in SgCMFS s, in SgSpec o) {
  return vec3(sg_hsum(sg_mul(s[0], o)),
              sg_hsum(sg_mul(s[1], o)),
              sg_hsum(sg_mul(s[2], o)));
}

#endif // CMFS_SUBGROUP_GLSL_GUARD