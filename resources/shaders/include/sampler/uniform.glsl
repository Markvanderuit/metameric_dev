#ifndef SAMPLER_UNIFORM_GLSL_GUARD
#define SAMPLER_UNIFORM_GLSL_GUARD

#include <sampler/detail/pcg.glsl>

#define SamplerState uint

float uniform_distr_1d(in uint u) {
  return float(u) / 4294967295.f;
}

float next_1d(inout SamplerState state) {
  return uniform_distr_1d(pcg_hash(state));
}

#define next_nd(n)\
  vec##n next_##n##d(inout SamplerState state) { \
    vec##n v;                                    \
    for (uint i = 0; i < n; ++i)                 \
      v[i] = next_1d(state);                     \
    return v;                                    \
  }

next_nd(2)
next_nd(3)
next_nd(4)

#endif // SAMPLER_UNIFORM_GLSL_GUARD