#ifndef PATH_GLSL_GUARD
#define PATH_GLSL_GUARD

// Helper struct to cache path vertex information
struct PathVertex {
  // World position
  vec3 p;

  // Record storing object/emitter/primitive id, see record.glsl
  uint data;
};

// Helper struct to cache full path information
struct Path {
  // Sampled path wavelengths
  vec4 wvls;

  // Energy times geometric attenuation over probability density, 
  // without reflectances which are separated out
  vec4 L; 

  // Total length of path before termination
  uint path_depth;

  // Path vertex information, up to path_depth
  PathVertex data[max_depth];
};

#endif // PATH_GLSL_GUARD