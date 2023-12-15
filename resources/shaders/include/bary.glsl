#ifndef BARY_GLSL_GUARD
#define BARY_GLSL_GUARD

const uint generalized_weights   = MET_GENERALIZED_WEIGHTS;
const uint generalized_weights_4 = MET_GENERALIZED_WEIGHTS / 4;

#define Bary4 vec4[generalized_weights_4]
#define BaryP Bary4

#endif // BARY_GLSL_GUARD