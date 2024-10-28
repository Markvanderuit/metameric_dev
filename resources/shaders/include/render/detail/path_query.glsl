#ifndef RENDER_DETAIL_PATH_QUERY_GLSL_GUARD
#define RENDER_DETAIL_PATH_QUERY_GLSL_GUARD

// Macros for enabling/disabling path querying; 
// helper code optionally inserted into path.glsl to capture
// path vertices and path throughput
#ifdef ENABLE_PATH_QUERY
  // Instantiate a path object
  #define path_query_initialize(pt) Path pt; { pt.path_depth = 0; }

  // Extend the path object, recording an additional vertex
  void path_query_extend(inout Path pt, in Ray r) {
    pt.data[pt.path_depth++] = PathVertex(ray_get_position(r), r.data);
  }

  // Finalize the path object, as if an emitter was directly hit by brdf sampling,
  // and store to buffer
  void path_query_finalize_direct(in Path pt, in vec4 L, in vec4 wvls) {
    pt.wvls = wvls;
    pt.L    = L;
    set_path(pt, get_next_path_id());
  }


  // Finalize the path object, as if an emitter was hit by illuminant sampling,
  // and store to buffer
  void path_query_finalize_emitter(in Path pt, in EmitterSample r, in vec4 L, in vec4 wvls) {
    pt.data[pt.path_depth++] = PathVertex(ray_get_position(r.ray), r.ray.data);
    pt.wvls = wvls;
    pt.L    = L;
    set_path(pt, get_next_path_id());
  }

  // Finalize the path object, as if there was no intersection, and instead
  // environment emission was captured
  void path_query_finalize_envmap(in Path pt, in vec4 L, in vec4 wvls) {
    // pt.data[pt.path_depth++] = PathVertex(r.p, r.data);
    pt.wvls = wvls;
    pt.L    = L;
    set_path(pt, get_next_path_id());
  }
#else  
  // Defaults that do nothing
  #define path_query_initialize(pt)                     {}
  #define path_query_extend(path, vt)                   {}
  #define path_query_finalize_direct(path, L, wvls)     {}
  #define path_query_finalize_emitter(path, r, L, wvls) {}
  #define path_query_finalize_envmap(path, L, wvls)     {}
#endif

#endif // RENDER_DETAIL_PATH_QUERY_GLSL_GUARD