#ifndef BRDF_GLSL_GUARD
#define BRDF_GLSL_GUARD

#include <render/record.glsl>
#include <render/sample.glsl>
#include <render/interaction.glsl>
#include <render/texture.glsl>
#include <render/ggx.glsl>
#include <render/fresnel.glsl>

// Indices of different lobe values
#define LOBE_SPEC_REFLECT 0
#define LOBE_SPEC_REFRACT 1
#define LOBE_DIFF_REFLECT 2
#define LOBE_COAT_REFLECT 3

vec4 detail_get_lobe_pdf(in BRDF brdf, in Interaction si, in vec4 F) {
  vec4 v;

  // Average used for metals,  same as F for dielectrics.
  float F_avg = hsum(F) * .25f;

  if (is_upper_hemisphere(si.wi)) {
    // From upper; we skip computation of coat fresnel and simply use 1/4th like mitsuba
    v[LOBE_SPEC_REFLECT] = F_avg * (1.f - brdf_metallic(brdf)) + brdf_metallic(brdf);
    v[LOBE_SPEC_REFRACT] = (1.f - F_avg) * (1.f - brdf_metallic(brdf)) * brdf_transmission(brdf);
    v[LOBE_DIFF_REFLECT] = (1.f - F_avg) * (1.f - brdf_metallic(brdf)) * (1.f - brdf_transmission(brdf));
    v[LOBE_COAT_REFLECT] = .25 * brdf_clearcoat(brdf);
  } else {
    // From lower, we don't apply transmission twice, metallic doesn't even get here, nor does coat
    v[LOBE_SPEC_REFLECT] = F_avg;
    v[LOBE_SPEC_REFRACT] = 1.f - F_avg;
    v[LOBE_DIFF_REFLECT] = 0.f;
    v[LOBE_COAT_REFLECT] = 0.f;
  }

  // Return normalized
  return v / hsum(v);
}

vec4 detail_get_lobe_cdf(in BRDF brdf, in Interaction si, in vec4 F) {
  vec4 v = detail_get_lobe_pdf(brdf, si, F);
  v.y += v.x;
  v.z += v.y;
  v.w += v.z;
  return v;
}

// Load BRDF data from underlying textures
BRDF get_brdf(inout Interaction si, vec4 wvls, in vec2 sample_2d) {
  BRDF brdf;
  
  // Query packed reflectance and brdf texture data
  brdf.r    = texture_reflectance(si, wvls, sample_2d);
  brdf.data = texture_brdf(si, sample_2d);
  
  // Compute cauchy coefficients b and c, then compute actual wavelength-dependent eta
  float eta_min = float(((brdf.data[1]     ) & 0x00FFu)) / 255.f * 3.f + 1.f; // 8b unorm
  float eta_max = float(((brdf.data[1] >> 8) & 0x00FFu)) / 255.f * 3.f + 1.f; // 8b unorm
  if (eta_max > eta_min) {
    float lambda_min_2 = sdot(wavelength_min), lambda_max_2 = sdot(wavelength_max);
    float cauchy_b = (lambda_min_2 * eta_max - lambda_max_2 * eta_min) / (lambda_min_2 - lambda_max_2);
    float cauchy_c = lambda_min_2 * (eta_max - cauchy_b);
    brdf.eta         = cauchy_b + cauchy_c / sdot(sample_to_wavelength(wvls.x));
    brdf.is_spectral = true;
  } else {
    brdf.eta         = eta_min;
    brdf.is_spectral = false;
  }

  // Unpack normalmap data;
  // Now that we've queried the underlying textures, we can adjust the 
  // local shading frame. This wasn't in use before this point.
  vec3 n = brdf_normalmap(brdf);
  si.n   = to_world(si, n);
  si.wi  = to_local(si, to_world(si, si.wi));

  return brdf;
}

vec4 eval_brdf(in BRDF brdf, in Interaction si, in vec3 wo) {
  bool is_upper     = is_upper_hemisphere(si.wi);
  bool is_reflected = cos_theta(si.wi) * cos_theta(wo) >= 0.f;

  // Get relative index of refraction along ray
  float     eta = is_upper_hemisphere(si.wi) ? brdf.eta : 1.f / brdf.eta;
  float inv_eta = is_upper_hemisphere(si.wi) ? 1.f / brdf.eta : brdf.eta;
  
  // Get the half-vector in the positive hemisphere direction
  vec3 m = to_upper_hemisphere(normalize(si.wi + wo * (is_reflected ? 1.f : eta)));

  // Evaluate fresnel, microfacet distribution; we assume coat isn't present
  // and ignore its index of refraction for now
  vec4  F0 = mix(vec4(schlick_F0(1.f, brdf.eta)), brdf.r, brdf_metallic(brdf));
  vec4  F  = schlick_fresnel(F0, dot(to_upper_hemisphere(si.wi), m)); 
  float GD = eval_ggx(si.wi, m, wo, brdf_alpha(brdf));

  // Output value
  vec4 f = vec4(0);

  // Coat component
  float coat = 0.f;
  if (is_upper && is_reflected && brdf_clearcoat(brdf) > 0) {
    float F0_coat = schlick_F0(1.f, 1.5);
    float F_coat  = schlick_fresnel(F0_coat, cos_theta(to_upper_hemisphere(si.wi)));
    float GD_coat = eval_ggx(si.wi, m, wo, brdf_clearcoat_alpha(brdf));
    float weight = 1.f / (4.f * cos_theta(si.wi) * cos_theta(wo));
    coat = F_coat * brdf_clearcoat(brdf);
    f += coat * GD_coat * weight;
  }

  // Diffuse component; lambert scaled by metallic
  if (is_upper && is_reflected) {
    f += (1.f - coat) * (1.f - F) * (1.f - brdf_metallic(brdf)) * (1.f - brdf_transmission(brdf)) * brdf.r * M_PI_INV;
  }

  // Specular reflect; evaluate ggx and multiply by fresnel, jacobian
  if (is_reflected) {
    float weight = 1.f / (4.f * cos_theta(si.wi) * cos_theta(wo));
    f += (1.f - coat) * F * GD * abs(weight);
  }
  
  // Specular refract; evaluate ggx and multiply by fresnel, jacobian
  if (!is_reflected) {
    // Apply Beer's law on exit
    vec4 r = is_upper 
           ? vec4(1) 
           : brdf.r * exp(-si.t * brdf_absorption(brdf));

    float weight = sdot(inv_eta) * dot(si.wi, m) * dot(wo, m) 
                 / abs(sdot(dot(wo, m) + dot(si.wi, m) * inv_eta) * cos_theta(si.wi) * cos_theta(wo));

    f += (1.f - coat) * (1.f - F) * (1.f - brdf_metallic(brdf)) * brdf_transmission(brdf) * r * GD * abs(weight);
  }

  return f;
}

float pdf_brdf(in BRDF brdf, in Interaction si, in vec3 wo) {
  bool is_reflected = cos_theta(si.wi) * cos_theta(wo) >= 0.f;
  bool is_upper     = is_upper_hemisphere(si.wi);

  // Get relative index of refraction for bsdf
  float     eta = is_upper_hemisphere(si.wi) ? brdf.eta : 1.f / brdf.eta;
  float inv_eta = is_upper_hemisphere(si.wi) ? 1.f / brdf.eta : brdf.eta;

  // Get the half-vector in the positive hemisphere direction as acting normal
  vec3 m = to_upper_hemisphere(normalize(si.wi + mix(eta, 1.f, is_reflected) * wo));

  // Evaluate fresnel, microfacet distribution; we assume coat isn't present
  // and ignore its index of refraction for now
  vec4  F0 = mix(vec4(schlick_F0(1.f, brdf.eta)), brdf.r, brdf_metallic(brdf));
  vec4  F  = schlick_fresnel(F0, dot(to_upper_hemisphere(si.wi), m)); 
  float GD = pdf_ggx(si.wi, m, brdf_alpha(brdf));

  // Compute lobe densities
  vec4 lobe_pdf = detail_get_lobe_pdf(brdf, si, F);

  // Output value
  float pdf = 0.f;

  // Diffuse lobe sample density
  if (is_reflected && is_upper) {
    pdf += lobe_pdf[LOBE_DIFF_REFLECT] * square_to_cos_hemisphere_pdf(wo);
  }

  // Reflect lobe sample density
  if (is_reflected) {
    float weight = 1.f / (4.f * dot(si.wi, m));
    pdf += lobe_pdf[LOBE_SPEC_REFLECT] * GD * abs(weight);
  }

  // Refract lobe sample density
  if (!is_reflected) {
    float weight = sdot(inv_eta) * dot(wo, m) 
                 / sdot(dot(wo, m) + dot(si.wi, m) * inv_eta);
    pdf += lobe_pdf[LOBE_SPEC_REFRACT] * GD * abs(weight);
  }
  
  // Coat sample density
  if (is_reflected && is_upper) {
    float weight = 1.f / (4.f * cos_theta(si.wi));
    float GD_coat = pdf_ggx(si.wi, m, brdf_clearcoat_alpha(brdf));
    pdf += lobe_pdf[LOBE_COAT_REFLECT] * GD_coat * abs(weight);
  }

  return pdf;
}

BRDFSample sample_brdf(in BRDF brdf, in vec3 sample_3d, in Interaction si) {
  // Sample a microfacet normal to operate on
  vec3 m = sample_ggx(si.wi, brdf_alpha(brdf), sample_3d.yz);
  
  // Get relative index of refraction
  float     eta = is_upper_hemisphere(si.wi) ? brdf.eta : 1.f / brdf.eta;
  float inv_eta = is_upper_hemisphere(si.wi) ? 1.f / brdf.eta : brdf.eta;

  // Move into microfacet normal's frame
  Frame local_fr = get_frame(m);
  vec3  local_wi = to_local(local_fr, si.wi);

  // Return object
  BRDFSample bs;
  bs.is_delta    = false;
  bs.is_spectral = false;
  bs.eta         = 1.f;

  // Compute fresnel and angle of transmission; F is set to 1 on total internal reflection;
  // we assume coat isn't present and ignore its index of refraction for now
  float cos_theta_t;
  vec4  F0 = mix(vec4(schlick_F0(1.f, brdf.eta)), brdf.r, brdf_metallic(brdf));
  vec4  F  = schlick_fresnel(F0, dot((si.wi), m), cos_theta_t, brdf.eta); 

  // Compute lobe densities
  vec4 lobe_cdf = detail_get_lobe_cdf(brdf, si, F);

  if (sample_3d.x < lobe_cdf[LOBE_SPEC_REFLECT]) { // Sample 1st specular reflect lobe
    // Reflect on microfacet normal
    bs.wo          = to_world(local_fr, local_reflect(local_wi));
    bs.is_spectral = false;
    
    if (cos_theta(bs.wo) * cos_theta(si.wi) <= 0)
      return brdf_sample_zero();
  } else if (sample_3d.x < lobe_cdf[LOBE_SPEC_REFRACT]) { // Sample specular refract lobe
    // Refract on microfacet normal
    bs.wo          = to_world(local_fr, local_refract(local_wi, cos_theta_t, inv_eta));
    bs.is_spectral = brdf.is_spectral;
    bs.eta         = inv_eta; //cos_theta(si.i) > 0 ?  cos_theta_t < 0.f ? eta : inv_eta;

    if (cos_theta(bs.wo) * cos_theta(si.wi) >= 0)
      return brdf_sample_zero();
  } else if (sample_3d.x < lobe_cdf[LOBE_DIFF_REFLECT]) { // Sample diffuse lobe
    bs.wo          = square_to_cos_hemisphere(sample_3d.yz);
    bs.is_spectral = false;

    if (cos_theta(bs.wo) * cos_theta(si.wi) <= 0)
      return brdf_sample_zero();
  } else if (sample_3d.x < lobe_cdf[LOBE_COAT_REFLECT]) { // Sample 2nd specular reflect lobe for clearcoat
    // Sample a microfacet normal to operate on
    vec3 m = sample_ggx(si.wi, brdf_clearcoat_alpha(brdf), sample_3d.yz);

    // Move into microfacet normal's frame
    Frame local_fr = get_frame(m);
    vec3  local_wi = to_local(local_fr, si.wi);

    // Reflect on coat microfacet normal
    bs.wo          = to_world(local_fr, local_reflect(local_wi));
    bs.is_spectral = false;

    if (cos_theta(bs.wo) * cos_theta(si.wi) <= 0)
      return brdf_sample_zero();
  }

  bs.pdf = pdf_brdf(brdf, si, bs.wo);

  return bs;
}

#endif // BRDF_GLSL_GUARD