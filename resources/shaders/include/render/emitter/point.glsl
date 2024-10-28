#ifndef RENDER_EMITTER_POINT_GLSL_GUARD
#define RENDER_EMITTER_POINT_GLSL_GUARD

EmitterSample sample_emitter_point(in EmitterInfo em, in SurfaceInfo si, in vec4 wvls, in vec2 sample_2d) {
  EmitterSample es;
  es.ray      = ray_towards_point(si, em.trf[3].xyz);
  es.L        = scene_illuminant(em.illuminant_i, wvls) * em.illuminant_scale / sdot(es.ray.t);
  es.pdf      = 1.f;
  es.is_delta = true;
  return es;
}

vec4 eval_emitter_point(in EmitterInfo em, in SurfaceInfo si, in vec4 wvls) {
  return scene_illuminant(em.illuminant_i, wvls) * em.illuminant_scale / sdot(si.t);
}

float pdf_emitter_point(in EmitterInfo em, in SurfaceInfo si) {
  return 1.f;
}

#endif // RENDER_EMITTER_POINT_GLSL_GUARD