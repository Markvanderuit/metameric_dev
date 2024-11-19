#ifndef RENDER_MICROFACET_GLSL_GUARD
#define RENDER_MICROFACET_GLSL_GUARD

#include <render/record.glsl>
#include <render/warp.glsl>
#include <render/sample.glsl>

// Microfacet brdf without fresnel; normal times shadowing times masking over 4 cos wo cos wi
float eval_microfacet(in SurfaceInfo si, in vec3 wh, in vec3 wo, in float alpha) {
  float alpha_2 = alpha * alpha;

  // GGX normal distribution function
  float ggx = alpha_2 
            / sdot((alpha_2 * cos_theta(wh) - cos_theta(wh)) * cos_theta(wh) + 1.f);

  // G1/G2 shadowing-masking
  float lambda_i = -.5f + .5f * sqrt(1.f + alpha_2 / (si.wi.z * si.wi.z) - alpha_2);
  float lambda_o = -.5f + .5f * sqrt(1.f + alpha_2 / (wo.z * wo.z) - alpha_2);
  float g1_i = 1.f / (1.f + lambda_i);
  float g1_o = 1.f / (1.f + lambda_o);
  float g2 = g1_i * g1_o;

  // Combination, fresnel excluded and implemented in the overlying brdf
  return ggx * g2 / (4.f * cos_theta(wo) * cos_theta(si.wi));
}

// Sampling density for a given normal, drawn from a ggx microfacet distribution
// Follows Equation 2 in this paper: https://doi.org/10.1111/cgf.14867
// This is mostly christoph's rewrite at this point. Seems cleaner.
float pdf_microfacet(in SurfaceInfo si, in vec3 wh, in float alpha) {
	if (cos_theta(wh) < 0.0)
		return 0.0;
  float alpha_2 = alpha * alpha;
	float len_m_wo_2_inv = alpha_2 + (1.f - alpha_2) * sdot(cos_theta(si.wi));
	float len_m_wh_2     = 1.f     - (1.f - alpha_2) * sdot(cos_theta(wh));
	float D = max(0.0, dot(si.wi, wh)) 
          * M_PI_INV
					/ (cos_theta(si.wi) + sqrt(len_m_wo_2_inv));
	return D * alpha_2 / (4.f * sdot(len_m_wh_2) * dot(si.wi, wh));
}

// Sample a ggx microfacet distribution and obtain a normal with some probability
MicrofacetSample sample_microfacet(in SurfaceInfo si,
                                   in float       alpha,
                                   in vec2        sample_2d) {
  // Step 1; warp wi to hemisphere as wi_p
  vec3 wi_p = normalize(vec3(si.wi.xy * alpha, si.wi.z));
  if (wi_p.z < 0)
    wi_p = -wi_p;
  
  // Step 2; sample visible hemisphere;  
  // listing 3 of Dupuy et al.
  float phi  = 2.f * M_PI * sample_2d.x;
  float z    = fma(1.f - sample_2d.y, 1.f + wi_p.z, -wi_p.z);
  float sth  = sqrt(clamp(1.f - z * z, 0.f, 1.f));
  vec3  c    = { sth * cos(phi), sth * sin(phi), z };
  vec3  wh_p = c + wi_p;

  // Step 2; sample visible hemisphere
  // christoph's rewrite; gives some clipped artifacts for me :(
  /* float azimuth = 2.f * M_PI * sample_2d.x - M_PI;
  float z       = 1.f - sample_2d.y * (1.f + wi_p.z);
  float sth     = sqrt(max(0.f, 1.f - z * z));
  vec3  c       = { sth * cos(azimuth), sth * sin(azimuth), z };
  vec3  wh_p    = c + wi_p;  */

  // Step 3; warp wh_p back to ellipsoid as wh
  vec3 wh = normalize(vec3(wh_p.xy * alpha, wh_p.z));
  if (wh.z < 0)
    wh = -wh;

  // Step 4; assemble sample object
  MicrofacetSample ms;
  ms.n   = normalize(reflect(-si.wi, wh) + si.wi);
  ms.pdf = pdf_microfacet(si, ms.n, alpha);
  return ms;
}

#endif // RENDER_MICROFACET_GLSL_GUARD