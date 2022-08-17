#ifndef RANDOM_GLSL_GUARD
#define RANDOM_GLSL_GUARD

// Hash function from https://www.shadertoy.com/view/4djSRW
#ifndef RANDOM_SCALE3
#define RANDOM_SCALE3 vec3(.1031, .1030, .0973)
#endif

#ifndef FANDOM_SCALE4
#define FANDOM_SCALE4 vec4(1031, .1030, .0973, .1099)
#endif

// Random_* functions from https://github.com/patriciogonzalezvivo/lygia/blob/main/generative/random.glsl

float random_1d(float x) {
  return fract(sin(x) * 43758.5453);
}

float random_1d(vec2 xy) {
  return fract(sin(dot(xy, vec2(12.9898, 78.233))) * 43758.5453);
}

float random_1d(in vec3 xyz) {
  return fract(sin(dot(xyz, vec3(70.9898, 78.233, 32.4355))) * 43758.5453123);
}

float random_1d(in vec4 xyzw) {
  float f = dot(xyzw, vec4(12.9898, 78.233, 45.164, 94.673));
  return fract(sin(f) * 43758.5453);
}

vec2 random_2d(in float x) {
  vec3 xyz = fract(vec3(x) * RANDOM_SCALE3);
  xyz += dot(xyz, xyz.yzx + 19.19);
  return fract((xyz.xx + xyz.yz) * xyz.zy);
}

vec2 random_2d(in vec2 xy) {
  const vec2 k = vec2(.3183099, .3678794);
  xy = xy * k + k.yx;
  return -1. + 2. * 
    fract(16. * k * fract(xy.x * xy.y * (xy.x + xy.y)));
}

vec2 random_2d(in vec3 xyz) {
  xyz = fract(xyz * RANDOM_SCALE3);
  xyz += dot(xyz, xyz.yzx + 19.19);
  return fract((xyz.xx + xyz.yz) * xyz.zy);
}

vec3 random_3d(in float x) {
  vec3 xyz = fract(vec3(x) * RANDOM_SCALE3);
  xyz += dot(xyz, xyz.yzx + 19.19);
  return fract((xyz.xxy + xyz.yzz) * xyz.zyx); 
}

vec3 random_3d(in vec2 xy) {
  vec3 xyz = fract(vec3(xy.xyx) * RANDOM_SCALE3);
  xyz += dot(xyz, xyz.yzx + 19.19);
  return fract((xyz.xxy + xyz.yzz) * xyz.zyx);
}

vec3 random_3d(in vec3 xyz) {
  xyz = vec3(dot(xyz, vec3(127.1, 311.7, 74.7)),
             dot(xyz, vec3(269.5, 183.3, 246.1)),
             dot(xyz, vec3(113.5, 271.9, 124.6)));
  return -1. + 2. * fract(sin(xyz) * 43758.5453123);
}

vec4 random_4d(float p) {
  vec4 xyzw = fract(vec4(p) * FANDOM_SCALE4);
  xyzw += dot(xyzw, xyzw.wzxy + 19.19);
  return fract((xyzw.xxyz + xyzw.yzzw) * xyzw.zywx);   
}

vec4 random_4d(vec2 xy) {
  vec4 xyzw = fract(vec4(xy.xyxy) * FANDOM_SCALE4);
  xyzw += dot(xyzw, xyzw.wzxy + 19.19);
  return fract((xyzw.xxyz + xyzw.yzzw) * xyzw.zywx);   
}

vec4 random_4d(vec3 xyz) {
  vec4 xyzw = fract(vec4(xyz.xyzx) * FANDOM_SCALE4);
  xyzw += dot(xyzw, xyzw.wzxy + 19.19);
  return fract((xyzw.xxyz + xyzw.yzzw) * xyzw.zywx);
}

vec4 random_4d(vec4 xyzw) {
  xyzw = fract(xyzw  * FANDOM_SCALE4);
  xyzw += dot(xyzw, xyzw.wzxy + 19.19);
  return fract((xyzw.xxyz + xyzw.yzzw) * xyzw.zywx);
}

float next_1d(inout float x) {
  return random_1d(x++);
}

float next_1d(inout vec2 x) {
  return random_1d(x++);
}

float next_1d(inout vec3 x) {
  return random_1d(x++);
}

float next_1d(inout vec4 x) {
  return random_1d(x++);
}

vec2 next_2d(inout float x) {
  return random_2d(x++);
}

vec2 next_2d(inout vec2 x) {
  return random_2d(x++);
}

vec3 next_3d(inout float x) {
  return random_3d(x++);
}

vec3 next_3d(inout vec2 x) {
  return random_3d(x++);
}

vec3 next_3d(inout vec3 x) {
  return random_3d(x++);
}

vec4 next_4d(inout float x) {
  return random_4d(x++);
}

vec4 next_4d(inout vec2 x) {
  return random_4d(x++);
}

vec4 next_4d(inout vec3 x) {
  return random_4d(x++);
}

vec4 next_4d(inout vec4 x) {
  return random_4d(x++);
}

#endif // RANDOM_GLSL_GUARDj