#ifndef RENDER_OBJECT_GLSL_GUARD
#define RENDER_OBJECT_GLSL_GUARD

void ray_intersect_object(inout Ray ray, uint object_i) {
  ObjectInfo object_info = scene_object_info(object_i);
  
  if (!object_info.is_active)
    return;

  // Generate local ray
  Ray ray_local;
  ray_local.o = (object_info.trf_mesh_inv * vec4(ray.o, 1)).xyz;
  ray_local.d = (object_info.trf_mesh_inv * vec4(ray.d, 0)).xyz;
  
  // Get length and normalize direction
  // Reuse length to adjust ray_local.t if ray.t is not at infty
  float dt = length(ray_local.d);
  ray_local.d /= dt;
  ray_local.t = (ray.t == FLT_MAX) ? FLT_MAX : dt * ray.t;

  // Run intersection; on a hit, recover world-space distance,
  // and store intersection data in ray
  if (ray_intersect_bvh(ray_local, object_info.mesh_i)) {
    ray.t    = length((object_info.trf_mesh * vec4(ray_local.d * ray_local.t, 0)).xyz);
    ray.data = ray_local.data;
    record_set_object(ray.data, object_i);
  }
}

bool ray_intersect_object_any(in Ray ray, uint object_i) {
  ObjectInfo object_info = scene_object_info(object_i);
  
  if (!object_info.is_active)
    return false;
  
  // Generate object space ray
  Ray ray_local;
  ray_local.o = (object_info.trf_mesh_inv * vec4(ray.o, 1)).xyz;
  ray_local.d = (object_info.trf_mesh_inv * vec4(ray.d, 0)).xyz;
  
  // Get length and normalize direction
  // Reuse length to adjust ray_local.t if ray.t is not at infty
  float dt = length(ray_local.d);
  ray_local.d /= dt;
  ray_local.t = (ray.t == FLT_MAX) ? FLT_MAX : dt * ray.t;
  
  return ray_intersect_bvh_any(ray_local, object_info.mesh_i);
}

#endif // RENDER_OBJECT_GLSL_GUARD