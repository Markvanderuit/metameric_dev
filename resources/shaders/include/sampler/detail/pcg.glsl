#ifndef PCG_GLSL_GUARD
#define PCG_GLSL_GUARD

// PCG integer hash, originally from https://www.pcg-random.org/, with 
// good sources at
// - https://jcgt.org/published/0009/03/02/ 
// - https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/

uint pcg_hash(inout uint state) {
  state = state * 747796405u + 2891336453u;

  uint v = state;
  v ^= v >> ((v >> 28u) + 4u);
  v *= 277803737u;
  v ^= v >> 22u;
  return v;
}

uvec2 pcg_hash_2(inout uvec2 state) {
  state = state * 1664525u + 1013904223u;

  uvec2 v = state;
  v += v.yx * 1664525u;
  v ^= v >> 16u;
  v += v.yx * 1664525u;
  v ^= v >> 16u;
  return v;
}

uvec3 pcg_hash_3(inout uvec3 state) {
  state = state * 1664525u + 1013904223u; 
  
  uvec3 v = state;
  v += v.yzx * v.zxy;
  v ^= v >> 16u; 
  v += v.yzx * v.zxy;
  return v;
}

uvec4 pcg_hash_4(inout uvec4 state) {
  state = state * 1664525u + 1013904223u;
  
  uvec4 v = state;
  v += v.yzxy * v.wxyz;
  v ^= v >> 16u;
  v += v.yzxy * v.wxyz;    
  return v;
}

#endif // PCG_GLSL_GUARD