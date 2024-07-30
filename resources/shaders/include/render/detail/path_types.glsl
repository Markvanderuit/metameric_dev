#ifndef RENDER_DETAIL_PATH_TYPES_GLSL_GUARD
#define RENDER_DETAIL_PATH_TYPES_GLSL_GUARD

#define path_max_depth 6

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

  // Actual length of path before termination
  uint path_depth; /* padd to 16 bytes */ uint p0[3];

  // Path vertex information, up to maximum depth
  PathVertex data[path_max_depth];
};

#define declare_path_data(head, paths)                           \
  uint get_next_path_id()        { return atomicAdd(head, 1);  } \
  void set_path(Path pt, uint i) { paths[i] = pt;              } \
  Path get_path(uint i)          { return paths[i];            }   

#endif // RENDER_DETAIL_PATH_TYPES_GLSL_GUARD