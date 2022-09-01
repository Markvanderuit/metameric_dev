#ifndef CMFS_CLUSTER_GLSL_GUARD
#define CMFS_CLUSTER_GLSL_GUARD

#include <spectrum_cluster.glsl>

#define ClCMFS mat3x4

#define cl_cmfs_scatter(dst, src) \
  { cl_spec_scatter(dst[0], src[0]) \
    cl_spec_scatter(dst[1], src[1]) \
    cl_spec_scatter(dst[2], src[2]) }

#define cl_cmfs_gather(dst, src) \
  { cl_spec_gather(dst[0], src[0]) \
    cl_spec_gather(dst[1], src[1]) \
    cl_spec_gather(dst[2], src[2]) }

/* Constructors */

ClCMFS cl_cmfs(in float f) {
  return ClCMFS(ClSpec(f), ClSpec(f), ClSpec(f));
}

#define cl_cmfs_colw_comp(mat, op) ClCMFS(mat[0] op, mat[1] op, mat[2] op)
#define cl_cmfs_colw_mat(mat, op)  ClCMFS(mat[0] op[0], mat[1] op[1], mat[2] op[2])

/* Component-wise math operators */

ClCMFS cl_add(in ClCMFS s, in float f) { return cl_cmfs_colw_comp(s, + f); }
ClCMFS cl_sub(in ClCMFS s, in float f) { return cl_cmfs_colw_comp(s, - f); }
ClCMFS cl_mul(in ClCMFS s, in float f) { return cl_cmfs_colw_comp(s, * f); }
ClCMFS cl_div(in ClCMFS s, in float f) { return cl_cmfs_colw_comp(s, / f); }

/* Column-wise math operators */

ClCMFS cl_add(in ClCMFS s, in ClCMFS o) { return cl_cmfs_colw_mat(s, + o); }
ClCMFS cl_sub(in ClCMFS s, in ClCMFS o) { return cl_cmfs_colw_mat(s, - o); }
ClCMFS cl_mul(in ClCMFS s, in ClCMFS o) { return cl_cmfs_colw_mat(s, * o); }
ClCMFS cl_div(in ClCMFS s, in ClCMFS o) { return cl_cmfs_colw_mat(s, / o); }

ClCMFS cl_add(in ClCMFS s, in ClSpec o) { return cl_cmfs_colw_comp(s, + o);}
ClCMFS cl_sub(in ClCMFS s, in ClSpec o) { return cl_cmfs_colw_comp(s, - o);}
ClCMFS cl_mul(in ClCMFS s, in ClSpec o) { return cl_cmfs_colw_comp(s, * o);}
ClCMFS cl_div(in ClCMFS s, in ClSpec o) { return cl_cmfs_colw_comp(s, / o);}

/* Special matrix math operators */

vec3 cl_mmul(in ClCMFS s, in ClSpec o) {
  return subgroupClusteredAdd(transpose(s) * o, cl_spectrum_invc_n);
}

ClSpec cl_mmul(in vec3 v, in ClCMFS s) {
  return v.x * s[0] + v.y * s[1] + v.z * s[2];
}

#endif // CMFS_CLUSTER_GLSL_GUARD