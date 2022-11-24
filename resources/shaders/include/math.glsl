#ifndef MATH_GLSL_GUARD
#define MATH_GLSL_GUARD

// Define max/min single precision float constants
#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1.175494351e-38

// Define math constants
#define M_PI  3.1415926538f
#define M_EPS 0.000001f

// Rounded-up division functions

#define CEIL_DIV(a, b) ((a + (b - 1)) / b)

int ceil_div(in int a, in int b) {return CEIL_DIV(a, b); }
uint ceil_div(in uint a, in uint b) {return CEIL_DIV(a, b); }

// Horizontal component product functions

#define HPROD_2(v) (v.x * v.y)
#define HPROD_3(v) (v.x * v.y * v.z)
#define HPROD_4(v) (v.x * v.y * v.z * v.w)

uint  hprod(in uvec2 v) { return HPROD_2(v); }
uint  hprod(in uvec3 v) { return HPROD_3(v); }
uint  hprod(in uvec4 v) { return HPROD_4(v); }
int   hprod(in ivec2 v) { return HPROD_2(v); }
int   hprod(in ivec3 v) { return HPROD_3(v); }
int   hprod(in ivec4 v) { return HPROD_4(v); }
float hprod(in vec2 v)  { return HPROD_2(v); }
float hprod(in vec3 v)  { return HPROD_3(v); }
float hprod(in vec4 v)  { return HPROD_4(v); }

// Horizontal component sum functions

#define HSUM_2(v) (v.x + v.y)
#define HSUM_3(v) (v.x + v.y + v.z)
#define HSUM_4(v) (v.x + v.y + v.z + v.w)

uint  hsum(in uvec2 v) { return HSUM_2(v); }
uint  hsum(in uvec3 v) { return HSUM_3(v); }
uint  hsum(in uvec4 v) { return HSUM_4(v); }
int   hsum(in ivec2 v) { return HSUM_2(v); }
int   hsum(in ivec3 v) { return HSUM_3(v); }
int   hsum(in ivec4 v) { return HSUM_4(v); }
float hsum(in vec2 v)  { return HSUM_2(v); }
float hsum(in vec3 v)  { return HSUM_3(v); }
float hsum(in vec4 v)  { return HSUM_4(v); }

// Horizontal component min functions

#define HMIN_2(v) min(v.x, v.y)
#define HMIN_3(v) min(min(v.x, v.y), v.z)
#define HMIN_4(v) min(min(min(v.x, v.y), v.z), v.w)

uint  hmin(in uvec2 v) { return HMIN_2(v); }
uint  hmin(in uvec3 v) { return HMIN_3(v); }
uint  hmin(in uvec4 v) { return HMIN_4(v); }
int   hmin(in ivec2 v) { return HMIN_2(v); }
int   hmin(in ivec3 v) { return HMIN_3(v); }
int   hmin(in ivec4 v) { return HMIN_4(v); }
float hmin(in vec2 v)  { return HMIN_2(v); }
float hmin(in vec3 v)  { return HMIN_3(v); }
float hmin(in vec4 v)  { return HMIN_4(v); }

// Horizontal component max functions

#define HMAX_2(v) max(v.x, v.y)
#define HMAX_3(v) max(max(v.x, v.y), v.z)
#define HMAX_4(v) max(max(max(v.x, v.y), v.z), v.w)

uint  hmax(in uvec2 v) { return HMAX_2(v); }
uint  hmax(in uvec3 v) { return HMAX_3(v); }
uint  hmax(in uvec4 v) { return HMAX_4(v); }
int   hmax(in ivec2 v) { return HMAX_2(v); }
int   hmax(in ivec3 v) { return HMAX_3(v); }
int   hmax(in ivec4 v) { return HMAX_4(v); }
float hmax(in vec2 v)  { return HMAX_2(v); }
float hmax(in vec3 v)  { return HMAX_3(v); }
float hmax(in vec4 v)  { return HMAX_4(v); }

#endif // MATH_GLSL_GUARD