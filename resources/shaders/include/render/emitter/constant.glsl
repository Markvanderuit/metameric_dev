#ifndef RENDER_EMITTER_CONSTANT_GLSL_GUARD
#define RENDER_EMITTER_CONSTANT_GLSL_GUARD

EmitterSample sample_emitter_constant(in EmitterInfo em, in SurfaceInfo si, in vec4 wvls, in vec2 sample_2d) {
  EmitterSample es;
  es.ray      = ray_towards_direction(si, square_to_cos_hemisphere(sample_2d));
  es.L        = scene_illuminant(em.illuminant_i, wvls) * em.illuminant_scale;
  es.pdf      = square_to_cos_hemisphere_pdf(es.ray.d);
  es.is_delta = false;
  return es;
}

vec4 eval_emitter_constant(in EmitterInfo em, in vec4 wvls) {
  return scene_illuminant(em.illuminant_i, wvls) * em.illuminant_scale;
}

float pdf_emitter_constant(in EmitterInfo em, in vec3 d_local) {
  return square_to_cos_hemisphere_pdf(d_local);
}

#endif // RENDER_EMITTER_CONSTANT_GLSL_GUARD