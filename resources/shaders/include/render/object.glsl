#ifndef RENDER_OBJECT_GLSL_GUARD
#define RENDER_OBJECT_GLSL_GUARD

bool ray_intersect_object(inout Ray ray, uint object_i) {
  Object object = scene_object_info(object_i);
  if (!object_is_active(object))
    return false;

  // Generate local ray
  Ray ray_local = ray_transform(ray, inverse(object.trf));

  // Run intersection, return if not a closest hit
  if (!ray_intersect_blas(ray_local, object_mesh_i(object)))
    return false;

  // Recover world-space distance from local ray
  ray_transform_inplace(ray, ray_local, scene_object_info(object_i).trf);

  // Store object id in ray data
  record_set_object(ray.data, object_i);

  return true;
}

bool ray_intersect_object_any(in Ray ray, uint object_i) {
  Object object = scene_object_info(object_i);
  if (!object_is_active(object))
    return false;
  
  // Generate local ray
  Ray ray_local = ray_transform(ray, inverse(object.trf));
  
  // Run intersection and return result
  return ray_intersect_blas_any(ray_local, object_mesh_i(object));
}

#endif // RENDER_OBJECT_GLSL_GUARD