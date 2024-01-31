#ifndef RENDER_EMITTER_RECTANGLE_GLSL_GUARD
#define RENDER_EMITTER_RECTANGLE_GLSL_GUARD

PositionSample sample_emitter_rectangle(in EmitterInfo em, in SurfaceInfo si, in vec2 sample_2d) {
  PositionSample ps;
  
  // Sample point on rectangle, with (0, 0) at its center
  ps.p = (em.trf * vec4(sample_2d - .5f, 0, 1)).xyz;
  ps.n = em.rect_n;

  // Store direction to point, normalize, and keep distance
  ps.d = ps.p - si.p;
  ps.t = length(ps.d);
  ps.d /= ps.t;

  // Set pdf to non-zero if we are not approaching from a back-face
  float dp = max(0.f, dot(ps.d, -ps.n));
  ps.pdf = dp > 0.f ? em.srfc_area_inv * sdot(ps.t) / dp : 0.f;
  ps.is_delta = false;

  return ps;
}

vec4 eval_emitter_rectangle(in EmitterInfo em, in PositionSample ps, in vec4 wvls) {
  // If normal is not inclined along the ray, return nothing
  if (dot(ps.d, ps.n) >= 0)
    return vec4(0);
  
  vec4 v = scene_illuminant(em.illuminant_i, wvls);
  return v * em.illuminant_scale;
}

float pdf_emitter_rectangle(in EmitterInfo em, in PositionSample ps) {
  float dp = max(0.f, dot(ps.d, -ps.n));
  return dp > 0.f ? em.srfc_area_inv * sdot(ps.t) / dp : 0.f;
}

#endif // RENDER_EMITTER_RECTANGLE_GLSL_GUARD