#ifndef PATH_GLSL_GUARD
#define PATH_GLSL_GUARD

// Helper struct to cache path vertex information
struct PathVertex {
  // World position
  vec3 p;

  // Record storing object/emitter/primitive id, see record.glsl
  uint data;
};

// Helper struct to cache path information,
// but without surface reflectances, which are ignored
struct IncompletePath {
  // Sampled path wavelengths
  vec4 wvls;

  // Energy over probability density, 
  // not decreased by surface reflectances
  vec4 L; 

  // Total length of path before termination
  uint path_depth;

  // Path vertex information, up to path_depth
  PathVertex data[max_depth];
}

// Helper struct to cache full path information
struct FullPath {
  // Sampled path wavelengths
  vec4 wvls;

  // Energy over probability density
  vec4 L; 

  // Total length of path before termination
  uint path_depth;

  // Path vertex information, up to path_depth
  PathVertex data[max_depth];
};

#endif // PATH_GLSL_GUARD