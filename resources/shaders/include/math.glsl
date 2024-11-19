#ifndef MATH_GLSL_GUARD
#define MATH_GLSL_GUARD

// Define max/min single precision float constants
#define UINT_MAX 4294967295u
#define UINT_MIN 0
#define FLT_MAX  3.402823466e+38
#define FLT_MIN  1.175494351e-38

// Define math constants
#define M_PI      3.14159265358979323846
#define M_PI_INV  0.31830988618379067154
#define M_EPS     5.9604645e-8
#define M_RAY_EPS 5e3 * M_EPS

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

uint   hsum(in uvec2 v) { return HSUM_2(v); }
uint   hsum(in uvec3 v) { return HSUM_3(v); }
uint   hsum(in uvec4 v) { return HSUM_4(v); }
int    hsum(in ivec2 v) { return HSUM_2(v); }
int    hsum(in ivec3 v) { return HSUM_3(v); }
int    hsum(in ivec4 v) { return HSUM_4(v); }
float  hsum(in vec2 v)  { return HSUM_2(v); }
float  hsum(in vec3 v)  { return HSUM_3(v); }
float  hsum(in vec4 v)  { return HSUM_4(v); }
double hsum(in dvec2 v) { return HSUM_2(v); }
double hsum(in dvec3 v) { return HSUM_3(v); }
double hsum(in dvec4 v) { return HSUM_4(v); }

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

// Horizontal component self-dot functions

float sdot(in float f) { return f * f;     }
float sdot(in vec2  v) { return dot(v, v); }
float sdot(in vec3  v) { return dot(v, v); }
float sdot(in vec4  v) { return dot(v, v); }

// rcp(...) for reciprocal
#define RCP(v, type) ( type (1.f) / v )

float rcp(in float v) { return RCP(v, float); }
vec2  rcp(in vec2  v) { return RCP(v,  vec2); }
vec3  rcp(in vec3  v) { return RCP(v,  vec3); }
vec4  rcp(in vec4  v) { return RCP(v,  vec4); }

// safe_rcp(...) for epsilon-checked reciprocal
#define SAFE_RCP(v, type) ( type (1.f) / max(v, M_EPS) )

float safe_rcp(in float v) { return SAFE_RCP(v, float); }
vec2  safe_rcp(in vec2  v) { return SAFE_RCP(v,  vec2); }
vec3  safe_rcp(in vec3  v) { return SAFE_RCP(v,  vec3); }
vec4  safe_rcp(in vec4  v) { return SAFE_RCP(v,  vec4); }

// safe_sqrt(...) for epsilon-checked sqrt
#define SAFE_SQRT(v) sqrt(max(v, 0.f))

float safe_sqrt(in float v) { return SAFE_SQRT(v); }
vec2  safe_sqrt(in vec2  v) { return SAFE_SQRT(v); }
vec3  safe_sqrt(in vec3  v) { return SAFE_SQRT(v); }
vec4  safe_sqrt(in vec4  v) { return SAFE_SQRT(v); }

// Swapping of vector objects

#define SWAP_T(Ty)                        \
  void swap(inout Ty a, inout Ty b) {     \
    Ty t = a;                             \
    a    = b;                             \
    b    = t;                             \
  }

SWAP_T(float);
SWAP_T(vec2);
SWAP_T(vec3);
SWAP_T(vec4);

// Multiple importance sampling heuristics

float mis_balance(in float pdf_a, in float pdf_b) {
  return pdf_a / (pdf_a + pdf_b);
}

float mis_power(in float pdf_a, in float pdf_b) {
  pdf_a *= pdf_a;
  pdf_b *= pdf_b;
  return pdf_a / (pdf_a + pdf_b);
}

vec4 schlick_fresnel(in vec4 f0, in vec4 f90, in float cos_theta) {
  float c1 = 1.f - cos_theta;
  float c2 = c1 * c1;
  float c5 = c2 * c2 * c1;
  return clamp(f0 + (f90 - f0) * c5, vec4(0), vec4(1));
}

// Fresnel according to schlick's model
vec4 schlick_fresnel(in vec4 f0, in float cos_theta) {
  return schlick_fresnel(f0, vec4(1), cos_theta);
}

float fresnel_dielectric(float eta, float cos_theta) {
  float c = abs(cos_theta);
  float g = eta * eta - 1.0 + c * c;
  if (g > 0.0) {
    g = sqrt(g);
    float A = (g - c) / (g + c);
    float B = (c * (g + c) - 1.0) / (c * (g - c) + 1.0);
    return 0.5 * A * A * (1.0 + B * B);
  }
  return 1.0;
}

/* Fresnel color blend base on fresnel factor */
// vec3 F_color_blend(float eta, float fresnel, vec4 F0_refl) { // F)_refl should be white
//   float F0 = F0_from_ior(eta);
//   float fac = clamp((fresnel - F0) / (1.0 - F0), 0.f, 1.f);
//   return mix(F0_refl, vec3(1.0), fac);
// }

// Convert between eta and principled specular
float eta_to_specular(in float eta) {
  float div = (eta - 1.f) / (eta + 1.f);
  return div * div / .08f;
}

// Convert between eta and principled specular
float specular_to_eta(in float spec) {
  float div = sqrt(spec * 0.08f);
  return 2.f / (1.f - div) - 1.f;
}

// Implementation of unpolarized complex fresnel reflection coefficient;
// yarr-de-harred from Mitsuba 1.3
/* vec4 fresnel_conductor(in float cos_theta_i, in vec2 eta) {
  float cos_theta_i_2 = cos_theta_i * cos_theta_i,
        sin_theta_i_2 = 1.f - cos_theta_i_2,
        sin_theta_i_4 = sin_theta_i_2 * sin_theta_i_2;
  
  float temp_1   = eta.x * eta.x - eta.y * eta.y - sin_theta_i_2,
        a_2_pb_2 = dr::safe_sqrt(temp_1*temp_1 + 4.f * eta.y * eta.y * eta.x * eta.x),
        a        = dr::safe_sqrt(.5f * (a_2_pb_2 + temp_1));
        
  float term_1 = a_2_pb_2 + cos_theta_i_2,
        term_2 = 2.f * cos_theta_i * a;

  float r_s = (term_1 - term_2) / (term_1 + term_2);

  float term_3 = a_2_pb_2 * cos_theta_i_2 + sin_theta_i_4,
        term_4 = term_2 * sin_theta_i_2;

  float r_p = r_s * (term_3 - term_4) / (term_3 + term_4);

  return 0.5f * (r_s + r_p);
}
 */
#endif // MATH_GLSL_GUARD