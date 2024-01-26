#ifndef FRAME_GLSL_GUARD
#define FRAME_GLSL_GUARD

// Local vector frame
struct Frame {
  vec3 n, s, t;
};

Frame get_frame(in vec3 n) {
  Frame fr;

  float s = n.z >= 0.f ? 1.f : -1.f;
  float a = -1.f / (s + n.z);
  float b = n.x * n.y * a; 

  fr.n = n;
  fr.s = vec3(n.x * n.x * a *  s + 1,
              b             *  s,
              n.x           * -s);
  fr.t = vec3(b,
              n.y * n.y * a + s,
             -n.y);

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

#endif // FRAME_GLSL_GUARD
