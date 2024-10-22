#ifndef RENDER_EMITTER_POINT_GLSL_GUARD
#define RENDER_EMITTER_POINT_GLSL_GUARD

PositionSample sample_emitter_point(in EmitterInfo em, in SurfaceInfo si, in vec2 sample_2d) {
  PositionSample ps;

  ps.p = (em.trf * vec4(0, 0, 0, 1)).xyz;
  ps.n = vec3(0, 0, 1); // Indeterminate?
  
  ps.d = ps.p - si.p;
  ps.t = length(ps.d);
  ps.d /= ps.t;
  
  ps.pdf      = 1.f;
  ps.is_delta = true;

  return ps;
}

vec4 eval_emitter_point(in EmitterInfo em, in PositionSample ps, in vec4 wvls) {
  #ifdef TEMP_BASIS_AVAILABLE
  return s_bucket_illm[bucket_id][em.illuminant_i] / sdot(ps.t);
  #else
  // Attenuate point light by one over squared distance
  vec4 v = scene_illuminant(em.illuminant_i, wvls);
  return v * em.illuminant_scale / sdot(ps.t);
  #endif
}

float pdf_emitter_point(in EmitterInfo em, in PositionSample ps) {
  return 1.f;
}

#endif // RENDER_EMITTER_POINT_GLSL_GUARD