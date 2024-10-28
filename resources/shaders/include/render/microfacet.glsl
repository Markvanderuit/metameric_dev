#ifndef RENDER_MICROFACET_GLSL_GUARD
#define RENDER_MICROFACET_GLSL_GUARD

#include <render/record.glsl>
#include <render/warp.glsl>
#include <render/sample.glsl>

// Mostly a carbon copy of the implementation in mitsuba 3
vec2 sincos_phi(in vec3 v) {
  float sin_theta_2   = fma(v.x, v.x, v.y * v.y);
  float inv_sin_theta = 1.f / sqrt(sin_theta_2);
  vec2 res = (abs(sin_theta_2) <= 4.f * M_EPS)
           ? vec2(1, 0)
           : clamp(v.xy * inv_sin_theta, -1, 1);
  return res.yx;
}

float _Lambda(in vec3 w, in float alpha) {
  float tan_2_theta = max(0.f, 1.f - sdot(w)) / sdot(w);
  if (isinf(tan_2_theta))
    return 0.f;

  vec2 sc = sincos_phi(w);
  float alpha2 = sdot(sc.y * alpha + sdot(sc.x * alpha));

  return (-1.f + sqrt(1.f + alpha2 * tan_2_theta)) / 2.f;
}

float _D(in vec3 n, in float alpha) {
  float tan_2_theta = max(0.f, 1.f - sdot(n)) / sdot(n);
  if (isinf(tan_2_theta))
    return 0.f;

  float cos4theta = sdot(sdot(n));
  if (cos4theta < 1e-16f)
    return 0.f;
  
  vec2 sc = sincos_phi(n);
  float e = tan_2_theta 
          * (sdot(sc.y / alpha) + sdot(sc.x / alpha));
  
  return 1.f / (M_PI * alpha * alpha * cos4theta * sdot(1.f + e));
}

float _G1(in vec3 w, in float alpha) {
  return 1.f / (1.f + _Lambda(w, alpha));
}

float _G(in vec3 wi, in vec3 wo, in float alpha) {
  return 1.f / (1.f + _Lambda(wi, alpha) + _Lambda(wo, alpha));
}

// Mostly a carbon copy of the implementation in mitsuba 3
vec2 sample_visible_11(in float cos_theta_i, in vec2 sample_2d) {
  vec2 p  = square_to_unif_disk_concentric(sample_2d);
  float s = 0.5f * (1.f + cos_theta_i);
  p.y = mix(safe_sqrt(1.f - sdot(p.x)), p.y, s);

  float x = p.x, y = p.y, z = safe_sqrt(1.f - sdot(p));
  
  float sin_theta_i = safe_sqrt(1.f - sdot(cos_theta_i));
  float inv_norm    = 1.f / fma(sin_theta_i, y, cos_theta_i * z);

  return vec2(cos_theta_i * y - (sin_theta_i * z), x) * inv_norm;
}

// Mostly a carbon copy of the implementation of ggx with visible normal sampling in mitsuba 3
float eval_microfacet(in vec3 wi, in vec3 wh, in vec3 wo, in float alpha) {
  float f = (_G(wi, wo, alpha) * _D(wh, alpha))
          / (4.f * cos_theta(wi) * cos_theta(wo));
  return f;
}

// Mostly a carbon copy of the implementation of ggx with visible normal sampling in mitsuba 3
float pdf_microfacet(in vec3 wi, in vec3 n, in float alpha) {
  float pdf = _D(n, alpha)
            * _G1(wi, alpha)
            * max(0.f, dot(n, wi))
            / cos_theta(wi);
  return pdf;
}

// Mostly a carbon copy of the implementation of ggx with visible normal sampling in mitsuba 3
MicrofacetSample sample_microfacet(in vec3  wi,
                                    in float alpha,
                                    in vec2  sample_2d) {
  // Step 1; stretch wi
  vec3 wi_p = normalize(vec3(alpha * wi.xy, wi.z));
  if (wi_p.z < 0.f)
    wi_p = -wi_p;
  
  // Step 2; simulate P22_{wi}(slope.x, slope.y, 1, 1)
  vec2 slope = sample_visible_11(cos_theta(wi_p), sample_2d);

  // Step 3; rotate & unstretch
  vec2 sin_cos_phi = sincos_phi(wi_p);
  slope = alpha * vec2(
    fma(sin_cos_phi.y, slope.x, -(sin_cos_phi.x * slope.y)),
    fma(sin_cos_phi.x, slope.x,  (sin_cos_phi.y * slope.y))
  );

  // Step 4; assemble normal & PDF
  MicrofacetSample ms;
  ms.n   = normalize(vec3(-slope.xy, 1));
  ms.pdf = _G1(wi, alpha);
  return ms;
}

#endif // RENDER_MICROFACET_GLSL_GUARD