#ifndef RENDER_EMITTER_CONSTANT_GLSL_GUARD
#define RENDER_EMITTER_CONSTANT_GLSL_GUARD

vec4 eval_emitter_constant(in EmitterInfo em, in SurfaceInfo si, in vec4 wvls, in vec2 sample_2d) {
  // Sample either uplifted texture data, or a specified illuminant
  vec4 L = em.spec_type == EmitterSpectrumTypeColor
         ? texture_illuminant(record_get_emitter(si.data), si.tx, wvls, sample_2d)
         : scene_illuminant(em.illuminant_i, wvls);
  return L * em.illuminant_scale;
}

float pdf_emitter_constant(in EmitterInfo em, in SurfaceInfo si) {
  return square_to_unif_hemisphere_pdf(si.wi);
}

EmitterSample sample_emitter_constant(in EmitterInfo em, in SurfaceInfo si, in vec4 wvls, in vec2 sample_2d) {
  EmitterSample es;
  
  es.is_delta = false;
  es.ray      = ray_towards_direction(si, to_world(si, square_to_unif_hemisphere(sample_2d)));
  es.pdf      = square_to_unif_hemisphere_pdf(es.ray.d);

  return es;
}

#endif // RENDER_EMITTER_CONSTANT_GLSL_GUARD