#ifndef RENDER_MICROFACET_GLSL_GUARD
#define RENDER_MICROFACET_GLSL_GUARD

#include <render/record.glsl>
#include <render/warp.glsl>
#include <render/sample.glsl>

vec3 sample_ggx_vndf(vec3 out_dir, vec2 roughness, vec2 randoms) {
	// This implementation is based on:
	// https://gist.github.com/jdupuy/4c6e782b62c92b9cb3d13fbb0a5bd7a0
	// It is described in detail here:
	// https://doi.org/10.1111/cgf.14867

	// Warp to the hemisphere configuration
	vec3 out_dir_std = normalize(vec3(out_dir.xy * roughness, out_dir.z));
  if (out_dir_std.z < 0)
    out_dir_std = -out_dir_std;
  
	// Sample a spherical cap in (-out_dir_std.z, 1]
	float azimuth = (2.0 * M_PI) * randoms[0] - M_PI;
	float z = 1.0 - randoms[1] * (1.0 + out_dir_std.z);
	float sine = sqrt(max(0.0, 1.0 - z * z));
	vec3 cap = vec3(sine * cos(azimuth), sine * sin(azimuth), z);
	// Compute the half vector in the hemisphere configuration
	vec3 half_dir_std = cap + out_dir_std;
	// Warp back to the ellipsoid configuration
	
  vec3 in_dir = normalize(vec3(half_dir_std.xy * roughness, half_dir_std.z));
  if (in_dir.z < 0)
    in_dir = -in_dir;

  return in_dir;
}

vec3 sample_ggx_in_dir(vec3 out_dir, float roughness, vec2 randoms) {
	vec3 half_dir = sample_ggx_vndf(out_dir, vec2(roughness), randoms);
	return -reflect(out_dir, half_dir);
}

// https://auzaiffe.wordpress.com/2024/04/15/vndf-importance-sampling-an-isotropic-distribution/
/* float pdf_vndf_isotropic(vec3 wo, vec3 n, vec3 wi, float alpha) {
  float alpha_2 = alpha * alpha;
  vec3  wm = normalize(wo + wi);
  float zm = dot(wm, n);
  float zi = dot(wi, n);
  float nrm = 1.f / sqrt((zi * zi) * (1.0f - alpha_2) + alpha_2);
  float sigmaStd = (zi * nrm) * 0.5f + 0.5f;
  float sigmaI = sigmaStd / nrm;
  float nrmN = (zm * zm) * (alpha_2 - 1.0f) + 1.0f;
  return alpha_2 / (M_PI * 4.0f * nrmN * nrmN * sigmaI);
} */

float get_ggx_vndf_density(float lambert_out, float half_dot_normal, float half_dot_out, float roughness) {
	// Based on Equation 2 in this paper: https://doi.org/10.1111/cgf.14867
	// A few factors have been cancelled to optimize evaluation.
	if (half_dot_normal < 0.0)
		return 0.0;

	float roughness_2 		 = roughness * roughness;
	float flip_roughness_2 = 1.0 - roughness * roughness;

	float length_M_inv_out_2 = roughness_2 + flip_roughness_2 * lambert_out * lambert_out;
	float length_M_half_2    = 1.0 				 - flip_roughness_2 * half_dot_normal * half_dot_normal;

	float D_vis_std = max(0.0, half_dot_out) 
									* (1.0 / M_PI) 
									/ (lambert_out + sqrt(length_M_inv_out_2));

	return D_vis_std * roughness_2 / (length_M_half_2 * length_M_half_2);
}


// Microfacet brdf without fresnel; normal times shadowing times masking over 4 cos wo cos wi
float eval_microfacet(in SurfaceInfo si, in vec3 wh, in vec3 wo, in float alpha) {
  float alpha_2 = alpha * alpha;

  // GGX normal distribution function
  // float ggx = (alpha_2 * wh.z - wh.z) * wh.z + 1.f;
  // ggx = alpha_2 / (ggx * ggx);
  float ggx = alpha_2 / sdot((alpha_2 * cos_theta(wh) - cos_theta(wh)) * cos_theta(wh) + 1.f);

  // G1/G2 shadowing-masking
  float lambda_i = -.5f + .5f * sqrt(1.f + alpha_2 / (si.wi.z * si.wi.z) - alpha_2);
  float lambda_o = -.5f + .5f * sqrt(1.f + alpha_2 / (wo.z * wo.z) - alpha_2);
  float g1_i = 1.f / (1.f + lambda_i);
  float g1_o = 1.f / (1.f + lambda_o);
  float g2 = g1_i * g1_o;

  // Combination, fresnel excluded and kept for the brdf
  return ggx * g2 / (4.f * cos_theta(wo) * cos_theta(si.wi));
  
//   // GGX normal distribution function
//   float alpha_2 = alpha * alpha;
//   float ggx = (alpha_2 * wh.z - wh.z) * wh.z + 1.f;
//   ggx = alpha_2 / (ggx * ggx);
  
// //  /*  float lambda_i = -.5f + .5f * sqrt(1.f + alpha_2 / (si.wi.z * si.wi.z) - alpha_2);
// //   float lambda_o = -.5f + .5f * sqrt(1.f + alpha_2 / (wo.z * wo.z) - alpha_2);
// //   float g1_i = 1.f / (1.f + lambda_i);
// //   float g1_o = 1.f / (1.f + lambda_o);
// //   float smith = g1_i * g1_o; */
// //   // smith /= 4.f * cos_theta(wo) * cos_theta(si.wi);
// //   // smith /= 4.f * cos_theta(wo) * dot(si.wi, wh);

// 	// ... Smith masking-shadowing...
// 	float masking   = cos_theta(wo)    * sqrt((cos_theta(si.wi) - alpha_2 * cos_theta(si.wi)) * cos_theta(si.wi) + alpha_2);
// 	float shadowing = cos_theta(si.wi) * sqrt((cos_theta(wo)    - alpha_2 * cos_theta(wo))    * cos_theta(wo) + alpha_2);
// 	float smith = 0.5 / (masking + shadowing);

  // return ggx * smith;
}

// Sampling density for a given normal, drawn from a ggx microfacet distribution
float pdf_microfacet(in SurfaceInfo si, in vec3 wh, in float alpha) {
  float density = get_ggx_vndf_density(cos_theta(si.wi),
                                       cos_theta(wh),
                                       dot(si.wi, wh),
                                       alpha);
	density /= 4.0 * dot(si.wi, wh);
  return density;

  // return pdf_vndf_isotropic(si.wi, wh, reflect(-si.wi, wh), alpha);

  /* if (cos_theta(wh) < 0.f)
    return 0.f;

  float alpha_2       = alpha * alpha;
  float len_mwi_2_inv = alpha_2 + (1.f - alpha_2) * sdot(cos_theta(si.wi));
  float len_mwh_2     = 1.f     - (1.f - alpha_2) * sdot(cos_theta(wh));

  float D_vis_std = max(0.f, dot(wh, si.wi)) 
                  * (2.f * M_PI_INV) 
                  / (cos_theta(si.wi) + sqrt(len_mwi_2_inv));

  float pdf = D_vis_std * alpha_2 / (len_mwh_2 * len_mwh_2);
  pdf /= 4.f * dot(wh, si.wi);

  return pdf; */
}

// Sample a ggx microfacet distribution and obtain a normal with some probability
MicrofacetSample sample_microfacet(in SurfaceInfo si,
                                   in float       alpha,
                                   in vec2        sample_2d) {
  /* {
    MicrofacetSample ms;
    // vec3 wo = sample_ggx_in_dir(si.wi, alpha, sample_2d);
    // ms.n   = normalize(si.wi + wo);
    vec3 wo = reflect(-si.wi, sample_ggx_vndf(si.wi, vec2(alpha), sample_2d));
    ms.n   = normalize(si.wi + wo);
    ms.pdf = pdf_microfacet(si, ms.n, alpha);
    return ms;
  }      */           

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
  // christoph's rewrite
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