#ifndef BARY_GLSL_GUARD
#define BARY_GLSL_GUARD

const uint barycentric_weights = MET_BARYCENTRIC_WEIGHTS;

#define Bary  float[barycentric_weights]
#define Bary4 vec4[barycentric_weights / 4]

#endif // BARY_GLSL_GUARD