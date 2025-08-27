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
#define M_RAY_EPS 1e4 * M_EPS

// Guard functions, syntactic sugar

#define guard(expr)          if (!(expr)) { return;   }
#define guard_continue(expr) if (!(expr)) { continue; }
#define guard_break(expr)    if (!(expr)) { break;    }

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

// mulsign(a, b), multiply a by the (non-zero) sign of b
#define MULSIGN_F(a, b, type) a * mix(-1.f, 1.f, b >= 0)
#define MULSIGN_V(a, b, type) a * mix(type(-1), type(1), greaterThanEqual(b, type(0)))

float mulsign(in float a, in float b) { return MULSIGN_F(a, b, float); }
vec2  mulsign(in  vec2 a, in float b) { return MULSIGN_F(a, b, float); }
vec3  mulsign(in  vec3 a, in float b) { return MULSIGN_F(a, b, float); }
vec4  mulsign(in  vec4 a, in float b) { return MULSIGN_F(a, b, float); }
vec2  mulsign(in  vec2 a, in  vec2 b) { return MULSIGN_V(a, b,  vec2); }
vec3  mulsign(in  vec3 a, in  vec3 b) { return MULSIGN_V(a, b,  vec3); }
vec4  mulsign(in  vec4 a, in  vec4 b) { return MULSIGN_V(a, b,  vec4); }

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

// Miscellaneous

float mis_balance(in float pdf_a, in float pdf_b) {
  return pdf_a / (pdf_a + pdf_b);
}

vec4 mis_balance(in vec4 pdf_a, in vec4 pdf_b) {
  return pdf_a / (pdf_a + pdf_b);
}

float mis_power(in float pdf_a, in float pdf_b) {
  pdf_a *= pdf_a;
  pdf_b *= pdf_b;
  return pdf_a / (pdf_a + pdf_b);
}

vec4 mis_power(in vec4 pdf_a, in vec4 pdf_b) {
  pdf_a *= pdf_a;
  pdf_b *= pdf_b;
  return pdf_a / (pdf_a + pdf_b);
}

// Convert between eta and principled specular
float eta_to_specular(in float eta) {
  float div = (eta - 1.f) / (eta + 1.f);
  return div * div / 0.08f;
}

// Convert between eta and principled specular
float specular_to_eta(in float spec) {
  float div = sqrt(spec * 0.08f);
  return 2.f / (1.f - div) - 1.f;
}

#endif // MATH_GLSL_GUARD