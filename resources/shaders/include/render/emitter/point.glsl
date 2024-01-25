#ifndef RENDER_EMITTER_POINT_GLSL_GUARD
#define RENDER_EMITTER_POINT_GLSL_GUARD

PositionSample sample_emitter_point(in EmitterInfo em, in SurfaceInfo si, in vec2 sample_2d) {
  PositionSample ps;

  ps.p = (em.trf * vec4(0, 0, 0, 1)).xyz;
  ps.n = vec3(0, 0, 1); // Indeterminate?
  
  ps.d = ps.p - si.p;
  ps.t = length(ps.d);
  ps.d /= ps.t;
  
  ps.pdf = 1.f;
  ps.is_delta = true;

  return ps;
}

vec4 eval_emitter_point(in EmitterInfo em, in PositionSample ps, in vec4 wvls) {
  vec4 v = vec4(0);
  for (uint i = 0; i < 4; ++i)
    v[i] = texture(b_illm_1f, vec2(wvls[i], em.illuminant_i)).x;

  // Attenuate point light
  v /= sdot(ps.t);
    
  return v * em.illuminant_scale;
}

float pdf_emitter_point(in EmitterInfo em, in PositionSample ps) {
  return 1.f;
}

#endif // RENDER_EMITTER_POINT_GLSL_GUARD