#ifndef RENDER_MICROFACET_GLSL_GUARD
#define RENDER_MICROFACET_GLSL_GUARD

#include <render/record.glsl>
#include <render/warp.glsl>
#include <render/sample.glsl>


float eval_microfacet_partial(in vec3 wi, in vec3 wh, in vec3 wo, in float alpha) {
  float alpha_2 = alpha * alpha;

  // GGX normal distribution function
  float ggx_D = alpha_2 
              / sdot((alpha_2 * cos_theta(wh) - cos_theta(wh)) * cos_theta(wh) + 1.f);

  // Smith shadowing-masking function
  float G1_o   = cos_theta(wo) 
               * sqrt((cos_theta(wi) - alpha_2 * cos_theta(wi)) * cos_theta(wi) + alpha_2);
  float G1_i   = cos_theta(wi) 
               * sqrt((cos_theta(wo) - alpha_2 * cos_theta(wo)) * cos_theta(wo) + alpha_2);
  float ggx_G2 = 0.5 / (G1_o + G1_i);

  // Fresnel and 1/pi excluded and implemented in the surrounding brdf
  return ggx_D * ggx_G2;
}

vec4 eval_microfacet(in vec3 wi, in vec3 wh, in vec3 wo, in float alpha, in vec4 F0) {
  float alpha_2 = alpha * alpha;

  // GGX normal distribution function
  float ggx_D = alpha_2 
              / sdot((alpha_2 * cos_theta(wh) - cos_theta(wh)) * cos_theta(wh) + 1.f);

  // Smith shadowing-masking function
  float G1_o  = cos_theta(wo) 
              * sqrt((cos_theta(wi) - alpha_2 * cos_theta(wi)) * cos_theta(wi) + alpha_2);
  float G1_i  = cos_theta(wi) 
              * sqrt((cos_theta(wo) - alpha_2 * cos_theta(wo)) * cos_theta(wo) + alpha_2);
  float ggx_G = 0.5 / (G1_o + G1_i);

  // Schlick's fresnel approximation
  vec4 ggx_F = schlick_fresnel(F0, vec4(1), cos_theta(wi));

  return ggx_D * ggx_G * ggx_F * M_PI_INV;
}

// Follows Equation 2 in this paper: https://doi.org/10.1111/cgf.14867
// This is mostly christoph's rewrite at this point. Seems cleaner.
float pdf_microfacet_hemisphere(in vec3 wi, in vec3 wh, in float alpha) {
	if (cos_theta(wh) < 0.0)
		return 0.0;

  float alpha_2      = alpha * alpha;
  float alpha_2_flip = 1.f - alpha_2;

  float len_m_wi_2_inv = alpha_2 + alpha_2_flip * sdot(cos_theta(wi));
  float len_m_wh_2     = 1.f - alpha_2_flip * sdot(cos_theta(wh));
  float D_vis_std = max(0.f, dot(wi, wh))
                  * (2.f * M_PI_INV)
                  / (cos_theta(wi) + sqrt(len_m_wi_2_inv));
  
  return D_vis_std * alpha_2 / sdot(len_m_wh_2);
}

// Sampling density for a given normal, drawn from a ggx microfacet distribution
float pdf_microfacet(in vec3 wi, in vec3 wh, in float alpha) {
	if (cos_theta(wh) < 0.0)
		return 0.0;
  return pdf_microfacet_hemisphere(wi, wh, alpha) / (4.f * dot(wi, wh));
}

// listing 3 of Dupuy et al. for sampling the visible hemisphere
vec3 sample_microfacet_hemisphere(in vec2 sample_2d, in vec3 wi) {
  float phi = 2.f * M_PI * sample_2d.x;
  float z   = fma((1.f - sample_2d.y), (1.f + wi.z), -wi.z);
  float sth = sqrt(clamp(1.f - z * z, 0.f, 1.f));
  vec3  c   = { sth * cos(phi), sth * sin(phi), z }; 
  vec3  h   = c + wi;
  return h;
}

// Sample a ggx microfacet distribution and obtain a normal with some probability
MicrofacetSample sample_microfacet(in vec3  wi,
                                   in float alpha,
                                   in vec2  sample_2d) {
  // Step 1; warp wi to hemisphere as wi_p
  vec3 wi_p = normalize(vec3(wi.xy * alpha, wi.z));
  
  // Step 2; sample visible hemisphere;  
  vec3 wh_p = sample_microfacet_hemisphere(sample_2d, wi_p);

  // Step 3; warp wh_p back to ellipsoid as wh
  vec3 wh = normalize(vec3(wh_p.xy * alpha, wh_p.z));

  // Step 4; assemble sample object
  MicrofacetSample ms;
  ms.n   = normalize(reflect(-wi, wh) + wi);
  ms.pdf = pdf_microfacet(wi, ms.n, alpha);
  return ms;
}

#endif // RENDER_MICROFACET_GLSL_GUARD