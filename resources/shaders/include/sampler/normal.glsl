#ifndef SAMPLER_NORMAL_GLSL_GUARD
#define SAMPLER_NORMAL_GLSL_GUARD

#include <sampler/uniform.glsl>

const float PI                 = 3.1415926538f;
const float GAUSSIAN_EPSILON   = .0001f;
const float GAUSSIAN_ALPHA     = 1.f;
const float GAUSSIAN_INV_ALPHA = 1.f / GAUSSIAN_ALPHA;
const float GAUSSIAN_K         = 2.0 / (PI * GAUSSIAN_ALPHA);

float inv_gaussian_cdf(in float x) {
	float y = log(max(1.f - x * x, GAUSSIAN_EPSILON));
	float z = GAUSSIAN_K + .5f * y;
	return sqrt(sqrt(z * z - y * GAUSSIAN_INV_ALPHA) - z) * sign(x);
}

vec2 inv_gaussian_cdf(in vec2 x) {
	vec2 y = log(max(1.f - x * x, GAUSSIAN_EPSILON));
	vec2 z = GAUSSIAN_K + .5f * y;
	return sqrt(sqrt(z * z - y * GAUSSIAN_INV_ALPHA) - z) * sign(x);
}

vec3 inv_gaussian_cdf(in vec3 x) {
	vec3 y = log(max(1.f - x * x, GAUSSIAN_EPSILON));
	vec3 z = GAUSSIAN_K + .5f * y;
	return sqrt(sqrt(z * z - y * GAUSSIAN_INV_ALPHA) - z) * sign(x);
}

vec4 inv_gaussian_cdf(in vec4 x) {
	vec4 y = log(max(1.f - x * x, GAUSSIAN_EPSILON));
	vec4 z = GAUSSIAN_K + .5f * y;
	return sqrt(sqrt(z * z - y * GAUSSIAN_INV_ALPHA) - z) * sign(x);
}

float next_1d_normal(inout SamplerState state) {
  return inv_gaussian_cdf(next_1d(state) * 2.f - 1.f);
}

vec2 next_2d_normal(inout SamplerState state) {
  return inv_gaussian_cdf(next_2d(state) * 2.f - 1.f);
}

vec3 next_3d_normal(inout SamplerState state) {
  return inv_gaussian_cdf(next_3d(state) * 2.f - 1.f);
}

vec4 next_4d_normal(inout SamplerState state) {
  return inv_gaussian_cdf(next_4d(state) * 2.f - 1.f);
}

#endif // SAMPLER_NORMAL_GLSL_GUARD