#ifndef RANDOM_UNIFORM_GLSL_GUARD
#define RANDOM_UNIFORM_GLSL_GUARD

#include <pcg.glsl>

#define RAND_DIV 4294967295.f

float uniform_distr_1d(in uint u) {
  return float(u) / RAND_DIV;
}

vec2 uniform_distr_2d(in uvec2 u) {
  return vec2(u) / RAND_DIV;
}

vec3 uniform_distr_3d(in uvec3 u) {
  return vec3(u) / RAND_DIV;
}

vec4 uniform_distr_4d(in uvec4 u) {
  return vec4(u) / RAND_DIV;
}

float next_1d(inout uint state) {
  return uniform_distr_1d(pcg_hash(state));
}

/* float next_1d(inout uvec2 state) {
  state.x += pcg_hash(state.y);
  return uniform_distr_1d(pcg_hash(state.x));
}

float next_1d(inout uvec3 state) {
  state.y += pcg_hash(state.z);
  state.x += pcg_hash(state.y);
  return uniform_distr_1d(pcg_hash(state.x));
}

float next_1d(inout uvec4 state) {
  state.z += pcg_hash(state.w);
  state.y += pcg_hash(state.z);
  state.x += pcg_hash(state.y);
  return uniform_distr_1d(pcg_hash(state.x));
} */

/* vec2 next_2d(inout uint state) {
  return vec2(next_1d(state), next_1d(state));
} */

/* vec2 next_2d(inout uvec2 state) {
  return uniform_distr_2d(pcg_hash_2(state));
}

vec2 next_2d(inout uvec3 state) {
  state.y += pcg_hash(state.z);
  return uniform_distr_2d(pcg_hash_2(state.xy));
}

vec2 next_2d(inout uvec4 state) {
  state.z += pcg_hash(state.w);
  state.y += pcg_hash(state.z);
  return uniform_distr_2d(pcg_hash_2(state.xy));
} */

/* vec3 next_3d(inout uint state) {
  return vec3(next_1d(state), next_1d(state), next_1d(state));
} */

/* vec3 next_3d(inout uvec2 state) {
  return vec3(next_2d(state), next_1d(state.x));
}

vec3 next_3d(inout uvec3 state) {
  return uniform_distr_3d(pcg_hash_3(state));
}

vec3 next_3d(inout uvec4 state) {
  state.z += pcg_hash(state.w);
  return uniform_distr_3d(pcg_hash_3(state.xyz));
} */

#define next_nd(n)\
  vec##n next_##n##d(inout uint state) {\
    vec##n v;                           \
    for (uint i = 0; i < n; ++i)        \
      v[i] = next_1d(state);            \
    return v;                           \
  }

next_nd(2)
next_nd(3)
next_nd(4)

/* vec4 next_4d(inout uint state) {
  vec4 v;
  for (uint i = 0; i < 4; ++i)
  return vec4(next_1d(state), next_1d(state), next_1d(state), next_1d(state));
} */

/* vec4 next_4d(inout uvec2 state) {
  return vec4(next_2d(state), next_2d(state));
}

vec4 next_4d(inout uvec3 state) {
  return vec4(next_3d(state), next_1d(state.x));
}

vec4 next_4d(inout uvec4 state) {
  return uniform_distr_4d(pcg_hash_4(state));
} */

#endif // RANDOM_UNIFORM_GLSL_GUARD