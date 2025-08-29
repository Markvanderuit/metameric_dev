#ifndef FRAME_GLSL_GUARD
#define FRAME_GLSL_GUARD

// Local vector frame
struct Frame {
  vec3 n, s, t;
};

// Src: https://people.compute.dtu.dk/jerf/code/hairy/HairAndDirections.pdf
Frame get_frame(in vec3 n) {
  Frame fr;

  const float a = -1.f / (1.f + n.z);
  const float b = n.x * n.y * a;
  
  fr.n = n;
  fr.s = vec3(fma(n.x * n.x, a, 1.f), b, -n.x);
  fr.t = vec3(b, fma(n.y * n.y, a, 1.f), -n.y);

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

vec3 local_refract(in vec3 wi, float cos_theta_i, float inv_eta) {
  float cos_theta_t_2 = 1.f - (1.f - sdot(cos_theta_i)) * sdot(inv_eta);
  float cos_theta_t   = mulsign(sqrt(cos_theta_t_2), -cos_theta_i);
  return vec3(-wi.xy * inv_eta, cos_theta_t);
}

vec3 to_upper_hemisphere(in vec3 v) {
  return mulsign(v, cos_theta(v));
}

#endif // FRAME_GLSL_GUARD
