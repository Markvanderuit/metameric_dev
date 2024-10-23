#ifndef RENDER_EMITTER_SPHERE_GLSL_GUARD
#define RENDER_EMITTER_SPHERE_GLSL_GUARD

PositionSample sample_emitter_sphere(in EmitterInfo em, in SurfaceInfo si, in vec2 sample_2d) {
  PositionSample ps;

  // Sample position on hemisphere facing surface, point may not be nearest
  Frame frm = get_frame(normalize(si.p - em.center));
  ps.p = em.center 
       + em.sphere_r * to_world(frm, square_to_unif_hemisphere(sample_2d));

  // Generate direction to point
  ps.d = ps.p - si.p;
  ps.t = length(ps.d);
  ps.d /= ps.t;
  
  ps.n   = normalize(ps.p - em.center);
  ps.pdf = (2.f * em.srfc_area_inv) * sdot(ps.t) / abs(dot(ps.d, ps.n));
  ps.is_delta = em.sphere_r == 0.f;

  return ps;
}

vec4 eval_emitter_sphere(in EmitterInfo em, in PositionSample ps, in vec4 wvls) {
  // If normal is not inclined along the ray, return nothing
  if (dot(ps.d, ps.n) >= 0)
    return vec4(0);
  
  return scene_illuminant(em.illuminant_i, wvls) * em.illuminant_scale;
}

float pdf_emitter_sphere(in EmitterInfo em, in PositionSample ps) {
  return (2.f * em.srfc_area_inv) * sdot(ps.t) / abs(dot(ps.d, ps.n));
}

#endif // RENDER_EMITTER_SPHERE_GLSL_GUARD