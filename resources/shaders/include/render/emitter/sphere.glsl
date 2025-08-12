#ifndef RENDER_EMITTER_SPHERE_GLSL_GUARD
#define RENDER_EMITTER_SPHERE_GLSL_GUARD

vec4 eval_emitter_sphere(in EmitterInfo em, in SurfaceInfo si, in vec4 wvls) {
  // If normal is not inclined along the ray, return nothing
  if (cos_theta(si.wi) <= 0)
    return vec4(0);
  return scene_illuminant(em.illuminant_i, wvls) * em.illuminant_scale;
}

float pdf_emitter_sphere(in EmitterInfo em, in SurfaceInfo si) {
  return 2.f * M_PI_INV * sdot(si.t) / (sdot(em.trf[0].xyz) * cos_theta(si.wi));
}

EmitterSample sample_emitter_sphere(in EmitterInfo em, in SurfaceInfo si, in vec4 wvls, in vec2 sample_2d) {
  // Sample position on hemisphere facing surface, point may not be nearest
  Frame frm = get_frame(normalize(si.p - em.trf[3].xyz));
  vec3  p   = em.trf[3].xyz 
            + (.5f * length(em.trf[0].xyz)) 
            * to_world(frm, square_to_unif_hemisphere(sample_2d));

  // Return value
  EmitterSample es;

  // Generate ray from surface towards position
  es.ray = ray_towards_point(si, p);
  
  // Set pdf to zero if we are approaching from a back-face
  vec3  n  = normalize(p - em.trf[3].xyz);
  float dp = dot(-es.ray.d, n);
  if (dp < 0) {
    es.pdf = 0;
    return es;
  }
  
  es.L        = scene_illuminant(em.illuminant_i, wvls) * em.illuminant_scale;
  es.pdf      = 2.f * M_PI_INV * sdot(es.ray.t) / (sdot(em.trf[0].xyz) * dp);
  es.is_delta = false;

  return es;
}

#endif // RENDER_EMITTER_SPHERE_GLSL_GUARD