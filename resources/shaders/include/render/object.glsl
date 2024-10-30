#ifndef RENDER_OBJECT_GLSL_GUARD
#define RENDER_OBJECT_GLSL_GUARD

void ray_intersect_object(inout Ray ray, uint object_i) {
  ObjectInfo object_info = scene_object_info(object_i);
  guard(object_info.is_active);

  // Generate local ray
  Ray ray_local = ray_transform(ray, inverse(object_info.trf));

  // Run intersection, return if not a closest hit
  guard(ray_intersect_bvh(ray_local, object_info.mesh_i)); 

  // Recover world-space distance from local ray
  ray_transform_inplace(ray, ray_local, scene_object_info(object_i).trf);

  // Store object id in ray data
  record_set_object(ray.data, object_i);
}

bool ray_intersect_object_any(in Ray ray, uint object_i) {
  ObjectInfo object_info = scene_object_info(object_i);
  if (!object_info.is_active)
    return false;
  
  // Generate local ray
  Ray ray_local = ray_transform(ray, inverse(object_info.trf));
  
  // Run intersection and return result
  return ray_intersect_bvh_any(ray_local, object_info.mesh_i);
}

#endif // RENDER_OBJECT_GLSL_GUARD