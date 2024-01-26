#ifndef RENDER_DETAIL_PATH_TYPES_GLSL_GUARD
#define RENDER_DETAIL_PATH_TYPES_GLSL_GUARD

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

  // Energy over probability density
  vec4 L; 

  // Total length of path before termination
  uint path_depth;

  // Path vertex information, up to path_depth
  PathVertex data[max_depth];
};

#endif // RENDER_DETAIL_PATH_TYPES_GLSL_GUARD