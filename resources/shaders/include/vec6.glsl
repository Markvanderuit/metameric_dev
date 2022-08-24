#ifndef VEC6_GLSL_GUARD
#define VEC6_GLSL_GUARD

struct vec6 { float v[6]; };

float dot(in vec6 a, in vec6 b) {
  return a.v[0] * b.v[0] + a.v[1] * b.v[1] + a.v[2] * b.v[2] +
         a.v[3] * b.v[3] + a.v[4] * b.v[4] + a.v[5] * b.v[5];
}

float length(in vec6 v) {
  return sqrt(dot(v, v));
} 

vec6 normalize(in vec6 v) {
  float r = length(v);
  for (uint i = 0; i < 6; ++i)
    v.v[i] /= r;
  return v;
}

void store_first(inout vec6 v6, in vec3 v) {
  v6.v[0] = v.x;
  v6.v[1] = v.y;
  v6.v[2] = v.z;
}

void store_second(inout vec6 v6, in vec3 v) {
  v6.v[3] = v.x;
  v6.v[4] = v.y;
  v6.v[5] = v.z;
}

#endif // VEC6_GLSL_GUARD