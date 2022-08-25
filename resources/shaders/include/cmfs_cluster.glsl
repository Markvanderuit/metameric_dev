#ifndef CMFS_CLUSTER_GLSL_GUARD
#define CMFS_CLUSTER_GLSL_GUARD

#include <spectrum_cluster.glsl>

struct SgCMFS {
  SgSpec x;
  SgSpec y;
  SgSpec z;
};

#define sg_cmfs_scatter(dst, src) \
  { sg_spec_scatter(dst.x, src[0]) \
    sg_spec_scatter(dst.y, src[1]) \
    sg_spec_scatter(dst.z, src[2]) }

#define sg_cmfs_gather(dst, src) \
  { sg_spec_gather(dst.x, src[0]) \
    sg_spec_gather(dst.y, src[1]) \
    sg_spec_gather(dst.z, src[2]) }

/* Constructors */

SgCMFS sg_cmfs(in float f) {
  SgSpec s = SgSpec(f);
  return SgCMFS(s, s, s);
}

/* Component-wise math operators */

SgCMFS sg_add(in SgCMFS s, in float f) {
  return SgCMFS(s.x + f, s.y + f, s.z + f);
}

SgCMFS sg_sub(in SgCMFS s, in float f) {
  return SgCMFS(s.x - f, s.y - f, s.z - f);
}

SgCMFS sg_mul(in SgCMFS s, in float f) {
  return SgCMFS(s.x * f, s.y * f, s.z * f);
}

SgCMFS sg_div(in SgCMFS s, in float f) {
  return SgCMFS(s.x / f, s.y / f, s.z / f);
}

/* Column-wise math operators */

SgCMFS sg_add(in SgCMFS s, in SgCMFS o) {
  return SgCMFS(s.x + o.x, s.y + o.y, s.z + o.z);
}

SgCMFS sg_sub(in SgCMFS s, in SgCMFS o) {
  return SgCMFS(s.x - o.x, s.y - o.y, s.z - o.z);
}

SgCMFS sg_mul(in SgCMFS s, in SgCMFS o) {
  return SgCMFS(s.x * o.x, s.y * o.y, s.z * o.z);
}

SgCMFS sg_div(in SgCMFS s, in SgCMFS o) {
  return SgCMFS(s.x / o.x, s.y / o.y, s.z / o.z);
}

SgCMFS sg_add(in SgCMFS s, in SgSpec o) {
  return SgCMFS(s.x + o, s.y + o, s.z + o);
}

SgCMFS sg_sub(in SgCMFS s, in SgSpec o) {
  return SgCMFS(s.x - o, s.y - o, s.z - o);
}

SgCMFS sg_mul(in SgCMFS s, in SgSpec o) {
  return SgCMFS(s.x * o, s.y * o, s.z * o);
}

SgCMFS sg_div(in SgCMFS s, in SgSpec o) {
  return SgCMFS(s.x / o, s.y / o, s.z / o);
}
/* Special matrix math operators */

vec3 sg_matmul(in SgCMFS s, in SgSpec o) {
  return vec3(sg_hsum(s.x * o), sg_hsum(s.y * o), sg_hsum(s.z * o));
}

SgSpec sg_matmul(in vec3 v, in SgCMFS s) {
  return v.x * s.x + v.y * s.y + v.z * s.z;
}

#endif // CMFS_CLUSTER_GLSL_GUARD