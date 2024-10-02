#ifndef RENDER_OBJECT_GLSL_GUARD
#define RENDER_OBJECT_GLSL_GUARD

bool ray_intersect_object(inout Ray ray, uint object_i) {
  ObjectInfo object_info = scene_object_info(object_i);
  
  if (!object_info.is_active)
    return false;

  // Generate local ray
  Ray ray_local = ray_transform(ray, object_info.trf_mesh_inv);

  // Run intersection; on a hit, recover world-space distance,
  // and store intersection data in ray
  if (ray_intersect_bvh(ray_local, object_info.mesh_i)) {
    ray_transform_inplace(ray, ray_local, object_info.trf_mesh);
    record_set_object(ray.data, object_i);
    return true;
  }

  return false;
}

bool ray_intersect_object_any(in Ray ray, uint object_i) {
  ObjectInfo object_info = scene_object_info(object_i);
  
  if (!object_info.is_active)
    return false;
  
  // Generate object space ray
  Ray ray_local = ray_transform(ray, object_info.trf_mesh_inv);
  
  // Run intersection and return result
  return ray_intersect_bvh_any(ray_local, object_info.mesh_i);
}

#endif // RENDER_OBJECT_GLSL_GUARD