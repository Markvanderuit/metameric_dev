#ifndef CMFS_CLUSTER_GLSL_GUARD
#define CMFS_CLUSTER_GLSL_GUARD

struct SgCMFS {
  SgSpec x;
  SgSpec y;
  SgSpec z;
};

/* Constructors */

SgCMFS sg_cmfs(in float f) {
  SgSpec s = sg_spectrum(f);
  return SgCMFS(s, s, s);
}



#endif // CMFS_CLUSTER_GLSL_GUARD