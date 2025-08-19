#ifndef RENDER_EMITTER_RECTANGLE_GLSL_GUARD
#define RENDER_EMITTER_RECTANGLE_GLSL_GUARD

vec4 eval_emitter_rectangle(in Emitter em, in Interaction si, in vec4 wvls, in vec2 sample_2d) {
  // If normal is not inclined along the ray, return nothing
  if (cos_theta(si.wi) <= 0)
    return vec4(0);

  // Sample either uplifted texture data, or a specified illuminant
  vec4 L = em.spec_type == EmitterSpectrumTypeColor
         ? texture_illuminant(record_get_emitter(si.data), si.tx, wvls, sample_2d)
         : scene_illuminant(em.illuminant_i, wvls);
  
  return L * em.illuminant_scale;
}

float pdf_emitter_rectangle(in Emitter em, in Interaction si) {
  return sdot(si.t) / (length(em.trf[0]) * length(em.trf[1]) * cos_theta(si.wi));
}

EmitterSample sample_emitter_rectangle(in Emitter em, in Interaction si, in vec4 wvls, in vec2 sample_2d) {
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

  es.pdf      = sdot(es.ray.t) / (length(em.trf[0]) * length(em.trf[1]) * dp);
  es.is_delta = false;

  return es;
}

#endif // RENDER_EMITTER_RECTANGLE_GLSL_GUARD