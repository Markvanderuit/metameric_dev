#ifndef EMITTER_CONSTANT_GLSL_GUARD
#define EMITTER_CONSTANT_GLSL_GUARD

PositionSample sample_emitter_constant(in EmitterInfo em, in SurfaceInfo si, in vec2 sample_2d) {
  PositionSample ps;

  ps.pdf = 1.f;
  ps.is_delta = true;

  return ps;
}

vec4 eval_emitter_constant(in EmitterInfo em, in PositionSample ps, in vec4 wvls) {
  return vec4(0);
}

float pdf_emitter_constant(in EmitterInfo em, in PositionSample ps) {
  return 0.f;
}

#endif // EMITTER_CONSTANT_GLSL_GUARD