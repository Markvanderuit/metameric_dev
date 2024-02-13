#ifndef WARP_GLSL_GUARD
#define WARP_GLSL_GUARD

#include <math.glsl>

vec2 square_to_unif_disk_concentric(in vec2 sample_2d) {
  sample_2d = 2.f * sample_2d - 1.f;

  bool quad_1_or_3 = abs(sample_2d.x) < abs(sample_2d.y);
  float r  = quad_1_or_3 ? sample_2d.y : sample_2d.x,
        rp = quad_1_or_3 ? sample_2d.x : sample_2d.y;
  
  float phi = 0.25f * M_PI * rp / r;
  if (quad_1_or_3)
    phi = .5f * M_PI - phi;
  if (all(is_zero(sample_2d)))
    phi = 0.f;

  return vec2(r * cos(phi), r * sin(phi));
}

vec3 square_to_unif_hemisphere(in vec2 sample_2d) {
  vec2 p = square_to_unif_disk_concentric(sample_2d);
  float z = 1.f - sdot(p);
  p *= sqrt(z + 1.f);
  return vec3(p, z);
}

float square_to_unif_hemisphere_pdf(in vec3 v) {
  return 1.f / (2.f * M_PI);
}

vec3 square_to_unif_sphere(in vec2 sample_2d) {
  float z = 1.f - 2.f * sample_2d.y;
  float r = sqrt(max(0.000001f, 1.f - z * z));

  float sin_phi = sin(2.f * M_PI * sample_2d.x);
  float cos_phi = cos(2.f * M_PI * sample_2d.x);
  
  return vec3(r * cos_phi, r * sin_phi, z);
}

vec3 square_to_cos_hemisphere(in vec2 sample_2d) {
  vec2 p = square_to_unif_disk_concentric(sample_2d);
  return vec3(p, sqrt(max(1.f - sdot(p), 0.f)));
}

float square_to_cos_hemisphere_pdf(in vec3 v) {
  return M_PI_INV * v.z;
}

vec3 square_to_unif_cone(in vec2 sample_2d, in float cos_cutoff) {
  float one_minus_cos_cutoff = 1.f - cos_cutoff;

  vec2 p   = square_to_unif_disk_concentric(sample_2d);
  float pn = sdot(p);
  float z  = cos_cutoff + one_minus_cos_cutoff * (1.f - pn);

  p *= sqrt(max(0.00001f, one_minus_cos_cutoff * (2.f - one_minus_cos_cutoff * pn)));

  return vec3(p, z);

  // float cos_theta = (1.f - sample_2d.x) + sample_2d.x * cos_cutoff;
  // float sin_theta = sqrt(max(0.0001f, 1.f - cos_theta * cos_theta));

  // float sin_phi = sin(2.f * M_PI * sample_2d.y);
  // float cos_phi = cos(2.f * M_PI * sample_2d.y);

  // return vec3(cos_phi * sin_theta, sin_phi * sin_theta, cos_theta);
}

float square_to_unif_cone_pdf(in vec2 sample_2d, in float cos_cutoff) {
  return .5f * M_PI_INV / (1.f - cos_cutoff);
}

#endif // WARP_GLSL_GUARD