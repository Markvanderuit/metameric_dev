#ifndef RENDER_EMITTER_CONSTANT_GLSL_GUARD
#define RENDER_EMITTER_CONSTANT_GLSL_GUARD

PositionSample sample_emitter_constant(in EmitterInfo em, in SurfaceInfo si, in vec2 sample_2d) {
  PositionSample ps;

  vec3 d = square_to_cos_hemisphere(sample_2d);

  // Point at far-away
  // TODO find bounding sphere to put point on
  ps.n = vec3(0, 0, 1);

  ps.t = 10000.f;
  ps.d = to_world(si, d);
  ps.p = si.p + ps.t * ps.d;
  
  ps.pdf      = square_to_cos_hemisphere_pdf(d);
  ps.is_delta = false;

  return ps;
}

vec4 eval_emitter_constant(in EmitterInfo em, in vec4 wvls) {
  return scene_illuminant(em.illuminant_i, wvls) * em.illuminant_scale;
}

float pdf_emitter_constant(in EmitterInfo em, in vec3 d_local) {
  return square_to_cos_hemisphere_pdf(d_local);
}

#endif // RENDER_EMITTER_CONSTANT_GLSL_GUARD