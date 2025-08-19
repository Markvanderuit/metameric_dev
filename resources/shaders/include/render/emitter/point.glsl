#ifndef RENDER_EMITTER_POINT_GLSL_GUARD
#define RENDER_EMITTER_POINT_GLSL_GUARD

vec4 eval_emitter_point(in Emitter em, in Interaction si, in vec4 wvls, in vec2 sample_2d) {
  // Sample either uplifted texture data, or a specified illuminant
  vec4 L = em.spec_type == EmitterSpectrumTypeColor
         ? texture_illuminant(record_get_emitter(si.data), si.tx, wvls, sample_2d)
         : scene_illuminant(em.illuminant_i, wvls);
  
  return L * em.illuminant_scale / sdot(si.t);
}

float pdf_emitter_point(in Emitter em, in Interaction si) {
  return 1.f;
}

EmitterSample sample_emitter_point(in Emitter em, in Interaction si, in vec4 wvls, in vec2 sample_2d) {
  EmitterSample es;
  es.ray      = ray_towards_point(si, em.trf[3].xyz);
  es.pdf      = 1.f;
  es.is_delta = true;
  return es;
}

#endif // RENDER_EMITTER_POINT_GLSL_GUARD