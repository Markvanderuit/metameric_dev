#ifndef COMPLEX_GLSL_GUARD
#define COMPLEX_GLSL_GUARD

#include <math.glsl>

// Note; mostly copied from src:
// https://momentsingraphics.de/Siggraph2019.html, ComplexAlgebra.hlsl

vec2 complex_conj(vec2 v) {
  return vec2(v.x, -v.y);
}

vec2 complex_mult(vec2 lhs, vec2 rhs) {
  return vec2(lhs.x * rhs.x - lhs.y * rhs.y,
              lhs.x * rhs.y + lhs.y * rhs.x);
}

// Note; denom must be non-zero
vec2 complex_divd(vec2 num, vec2 denom) {
	return vec2(num.x * denom.x + num.y * denom.y,
             -num.x * denom.y + num.y * denom.x) / sdot(denom);
}

// Note; must be non-zero
vec2 complex_rcp(vec2 v) {
  return vec2(v.x, -v.y) / sdot(v);
}

// Lacks precision, yuck
vec2 complex_exp(vec2 v) {
  float e = exp(v.x);
  return vec2(e * cos(v.y), e * sin(v.y));
}

#endif // COMPLEX_GLSL_GUARD