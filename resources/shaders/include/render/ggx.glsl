#ifndef RENDER_GGX_GLSL_GUARD
#define RENDER_GGX_GLSL_GUARD

float ggx_D(in vec3 n, in float alpha) {
  // Compute D per eq. 1 in https://jcgt.org/published/0007/04/01/paper.pdf
  float xyz = sdot(n.x / alpha) + sdot(n.y / alpha) + sdot(n.z);
  float D   = 1.f / (M_PI * sdot(alpha) * sdot(xyz));

  // Clamp to avoid numeric issues later on
  return cos_theta(n) > 1e-20f ? D : 0.f;
}

float ggx_smith_g1(in vec3 wi, in vec3 n, in float alpha) {
  // Compute g1 per eq. 2 in https://jcgt.org/published/0007/04/01/paper.pdf
  float xyz = (sdot(alpha * wi.x) + sdot(alpha * wi.y)) / sdot(wi.z);
  float g1  = 2.f / (1.f + sqrt(1.f + xyz));

  if (dot(wi, n) * cos_theta(wi) <= 0.f)
    g1 = 0.f;
  
  return g1;
}

float ggx_G(in vec3 wi, in vec3 wh, in vec3 wo, in float alpha) {
  return ggx_smith_g1(wi, wh, alpha) * ggx_smith_g1(wo, wh, alpha);
}

// listing 3 of Dupuy et al. for sampling the visible hemisphere
vec3 ggx_sample_hemisphere(in vec3 wi, in vec2 sample_2d) {
  float phi = 2.f * M_PI * sample_2d.x;
  float z   = fma((1.f - sample_2d.y), (1.f + wi.z), -wi.z);
  float sth = sqrt(clamp(1.f - z * z, 0.f, 1.f));
  vec3  c   = { sth * cos(phi), sth * sin(phi), z }; 
  vec3  h   = c + wi;
  return h;
}

float pdf_ggx(in vec3 wi, in vec3 wh, in float alpha) {
  return ggx_D(wh, alpha) 
       * ggx_smith_g1(wi, wh, alpha) 
       * abs(dot(wi, wh))
       / abs_cos_theta(wi);
}

float eval_ggx(in vec3 wi, in vec3 wh, in vec3 wo, in float alpha) {
  return ggx_D(wh, alpha) * ggx_G(wi, wh, wo, alpha);
}

MicrofacetSample sample_ggx(in vec3 wi, in float alpha, in vec2 sample_2d) {
  // Step 1; warp wi to hemisphere as wi_p
  vec3 wi_p = normalize(vec3(wi.xy * alpha, wi.z));

  // Step 2; sample visible hemisphere;  
  vec3 wh_p = ggx_sample_hemisphere(wi_p, sample_2d);

  // Step 3; warp wh_p back to ellipsoid as wh
  vec3 wh = normalize(vec3(wh_p.xy * alpha, wh_p.z));

  // Step 4; assemble sample object
  MicrofacetSample ms;
  ms.n   = normalize(reflect(-wi, wh) + wi);
  ms.pdf = pdf_ggx(wi, ms.n, alpha);
  return ms;
}

#endif // RENDER_GGX_GLSL_GUARD