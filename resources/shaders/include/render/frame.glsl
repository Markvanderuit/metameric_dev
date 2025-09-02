#ifndef FRAME_GLSL_GUARD
#define FRAME_GLSL_GUARD

// Local vector frame
struct Frame {
  vec3 n, s, t;
};

// Src: https://people.compute.dtu.dk/jerf/code/hairy/HairAndDirections.pdf
Frame get_frame(in vec3 n) {
  /* Frame fr;

  const float a = -safe_rcp(1.f + n.z);
  const float b = n.x * n.y * a;
  
  fr.n = n;
  fr.s = vec3(fma(n.x * n.x, a, 1.f), b, -n.x);
  fr.t = vec3(b, fma(n.y * n.y, a, 1.f), -n.y);

  return fr; */

  /* Frame fr;

  float s = n.z >= 0.f ? 1.f : -1.f;
  float a = -1.f / (s + n.z);
  vec3  m = n.xyy * n.xyx * a;
  
  fr.n = n;
  fr.s = vec3(fma(m.x, s, 1), m.z * s, -n.x * s);
  fr.t = vec3(m.z, m.y + s, -n.y); */

  // Stupid, but at least it's stable for textures
  Frame fr;
  fr.n = n;
  fr.s = n == vec3(1, 0, 0) ? vec3(0, 1, 0) : normalize(cross(n, vec3(1, 0, 0)));
  fr.t = normalize(cross(fr.n, fr.s));
  return fr;
}

vec3 to_local(in Frame fr, in vec3 v) {
  return vec3(dot(v, fr.s), dot(v, fr.t), dot(v, fr.n));
}

vec3 to_world(in Frame fr, in vec3 v) {
  return fma(fr.n, vec3(v.z), fma(fr.t, vec3(v.y), fr.s * v.x));
}

float cos_theta(in vec3 v) {
  return v.z;
}

float abs_cos_theta(in vec3 v) {
  return abs(v.z);
}

vec3 local_reflect(in vec3 wi) {
  return vec3(-wi.xy, wi.z);
}

vec3 local_refract(in vec3 wi, float inv_eta) {
  float cos_theta_t_2 = 1.f - (1.f - sdot(cos_theta(wi))) * sdot(inv_eta);
  float cos_theta_t   = mulsign(sqrt(cos_theta_t_2), -cos_theta(wi));
  return vec3(-wi.xy * inv_eta, cos_theta_t);
}

vec3 local_refract(in vec3 wi, float cos_theta_t, float inv_eta) {
  return vec3(-wi.xy * inv_eta, cos_theta_t);
}

vec3 to_upper_hemisphere(in vec3 v) {
  return mulsign(v, cos_theta(v));
}

bool is_upper_hemisphere(in vec3 v) {
  return cos_theta(v) >= 0;
}

#endif // FRAME_GLSL_GUARD
