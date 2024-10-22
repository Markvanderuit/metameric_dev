#ifndef BRDF_PBR_GLSL_GUARD
#define BRDF_PBR_GLSL_GUARD

#include <render/warp.glsl>
#include <render/record.glsl>

vec2 sincos_phi(in vec3 v) {
  float sin_theta_2   = fma(v.x, v.x, v.y * v.y);
  float inv_sin_theta = 1.f / sqrt(sin_theta_2);

  vec2 res = v.xy * inv_sin_theta;

  res = (abs(sin_theta_2) <= 4.f * M_EPS)
      ? vec2(1, 0)
      : clamp(res, -1, 1);
      
  return res.yx;
}

vec2 sample_visible_11(in float cos_theta_i, vec2 sample_2d) {
  vec2 p = square_to_unif_disk_concentric(sample_2d);
  float s = 0.5f * (1.f + cos_theta_i);
  p.y = mix(sqrt(max(0.0001f, 1.f - sdot(p.x))), p.y, s);

  float x = p.x,
        y = p.y,
        z = sqrt(max(0.0001f, 1.f - sdot(p)));
  
  float sin_theta_i = sqrt(max(0.0001f, 1.f - sdot(cos_theta_i)));
  float norm        = 1.f / fma(sin_theta_i, y, cos_theta_i * z);
  return vec2(cos_theta_i * y - (sin_theta_i * z), x) * norm;
}

float eval_(in float alpha, in vec3 m) {
  float result = 1.f / (
    M_PI * sdot(alpha) * 
      sdot(
        sdot(m.x / alpha) + sdot(m.y / alpha) + sdot(m.z)
      )
  );
  return mix(result, 0.f, result * cos_theta(m) > 1e20f);
}

float smith_g1(in float alpha, in vec3 v, in vec3 m) {
  float xy_alpha_2 = sdot(alpha * v.x) + sdot(alpha * v.y),
        tan_theta_alpha_2 = xy_alpha_2 / sdot(v.z);
  
  float result = 2.f / (1.f + sqrt(1.f + tan_theta_alpha_2));

  if (xy_alpha_2 == 0.f)
    result = 1;

  if (dot(v, m) * cos_theta(v) <= 0.f)
    result = 0;

  return result;
}

float schlick_fresnel(float cos_theta, float f0) {
    return clamp(f0 + (1.f - f0) * pow(1.f - cos_theta, 5), 0.f, 1.f);
}

vec4 schlick_fresnel(float cos_theta, vec4 f0) {
    return clamp(f0 + (vec4(1.f) - f0) * pow(1.f - cos_theta, 5), vec4(0), vec4(1));
}

/* vec3 sample_ggx(in float alpha, in vec3 wi, in vec2 sample_2d) {
  vec3 wi_p = normalize(vec3(
    alpha * wi.x,
    alpha * wi.y,
    wi.z
  ));

  vec2 sin_cos_phi = sincos_phi(wi_p);
  float ctheta    = cos_theta(wi_p);

  vec2 slope = sample_visible_11(ctheta, sample_2d);
  slope = vec2(
    (sin_cos_phi.y * slope.x - (sin_cos_phi.x * slope.y)) * alpha,
    (sin_cos_phi.x * slope.x - (sin_cos_phi.y * slope.y)) * alpha,
  );

  vec3 m = normalize(vec3(-slope.x, -slope.y, 1));
} */

void init_brdf_pbr(inout BRDFInfo brdf, in SurfaceInfo si, vec4 wvls) {
  brdf.r = scene_sample_reflectance(record_get_object(si.data), si.tx, wvls);
  brdf.roughness = 0.1f;
  brdf.metallic  = 0.f;
}

BRDFSample sample_brdf_pbr(in BRDFInfo brdf, in vec2 sample_2d, in SurfaceInfo si) {
  BRDFSample bs;

  if (cos_theta(si.wi) <= 0.f) {
    bs.pdf = 0.f;
    return bs;
  }

  bs.is_delta = false;

  float sel   = 1.f - brdf.metallic;
  float alpha = max(0.001f, sdot(brdf.roughness))

  // Sample normal from specular lobe
  vec3 m_spec;
  {
    vec3 wi_p = normalize(vec3(
      alpha * si.wi.x,
      alpha * si.wi.y,
      si.wi.z
    ));

    vec2 sin_cos_phi = sincos_phi(wi_p);
    float ctheta    = cos_theta(wi_p);

    vec2 slope = sample_visible_11(ctheta, sample_2d);
    slope = vec2(
      (sin_cos_phi.y * slope.x - (sin_cos_phi.x * slope.y)) * alpha,
      (sin_cos_phi.x * slope.x - (sin_cos_phi.y * slope.y)) * alpha
    );


    m_spec = normalize(vec3(-slope.x, -slope.y, 1));
    vec3 F0 = mix(vec3(0.1), vec3(1), brdf.metallic);
    vec3 F = schlick_fresnel(dot(si.wi, m_spec), F0);

    
    float prob_spec_refl = 1.f - sel * (1.f - sdot(F));

    
    // bs.pdf = eval_(alpha, bs.wo) 
    //        * smith_g1(alpha, si.wi, bs.wo)
    //        * abs(dot(si.wi, bs.wo))
    //        / cos_theta(si.wi);
  }
  
  // Sample reflected specular
  bs.wo = reflect(si.wi, m_spec);

  // bs.f        = brdf.r * M_PI_INV;
  // bs.wo       = square_to_cos_hemisphere(sample_2d);
  // bs.pdf      = square_to_cos_hemisphere_pdf(bs.wo);

  return bs;
}

vec4 eval_brdf_pbr(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  float cos_theta_i = cos_theta(si.wi), 
        cos_theta_o = cos_theta(wo);

  if (cos_theta_i <= 0.f || cos_theta_o <= 0.f)
    return vec4(0.f);
    
  return brdf.r * M_PI_INV;
}

float pdf_brdf_pbr(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  float cos_theta_i = cos_theta(si.wi), 
        cos_theta_o = cos_theta(wo);

  if (cos_theta_i <= 0.f || cos_theta_o <= 0.f)
    return 0.f;
  
  return square_to_cos_hemisphere_pdf(wo);
}

#endif // BRDF_PBR_GLSL_GUARD