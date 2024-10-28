#ifndef RENDER_EMITTER_RECTANGLE_GLSL_GUARD
#define RENDER_EMITTER_RECTANGLE_GLSL_GUARD

EmitterSample sample_emitter_rectangle(in EmitterInfo em, in SurfaceInfo si, in vec4 wvls, in vec2 sample_2d) {
  // Sample position on rectangle, with (0, 0) at its center
  vec3 p = (em.trf * vec4(sample_2d - .5f, 0, 1)).xyz;

  // Return value
  EmitterSample es;

  // Generate ray from surface towards position
  es.ray = ray_towards_point(si, p);

  // Set pdf to 0 if we approach a backface
  vec3  n  = normalize(em.trf[2].xyz);
  float dp = dot(-es.ray.d, n);
  if (dp < 0) {
    es.pdf = 0;
    return es;
  }

  es.L        = scene_illuminant(em.illuminant_i, wvls) * em.illuminant_scale;
  es.pdf      = sdot(es.ray.t) / (length(em.trf[0]) * length(em.trf[1]) * dp);
  es.is_delta = false;

  return es;
}

vec4 eval_emitter_rectangle(in EmitterInfo em, in SurfaceInfo si, in vec4 wvls) {
  // If normal is not inclined along the ray, return nothing
  if (cos_theta(si.wi) <= 0)
    return vec4(0);
  return scene_illuminant(em.illuminant_i, wvls) * em.illuminant_scale;
}

float pdf_emitter_rectangle(in EmitterInfo em, in SurfaceInfo si) {
  return sdot(si.t) / (length(em.trf[0]) * length(em.trf[1]) * cos_theta(si.wi));
}

#endif // RENDER_EMITTER_RECTANGLE_GLSL_GUARD