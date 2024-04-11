#ifndef MOMENTS_GLSL_GUARD
#define MOMENTS_GLSL_GUARD

// Mostly copied from https://momentsingraphics.de/Siggraph2019.html -> MomentBasedSpectra.hlsl, 
// following the paper "Using Moments to Represent Bounded Signals for Spectral Rendering", 
// Peters et al., 2019.
// See also include/metameric/core/moments.hpp, where things are a little more readable

#include <complex.glsl>

#define moment_coeffs 12
#define Moments_Comp vec2[moment_coeffs]
#define Moments_Real float[moment_coeffs]

vec2[moment_coeffs] trigonometric_to_exponential_moments(in float[moment_coeffs] tm) {
  vec2[moment_coeffs] em;

  float zeroeth_phase = tm[0] * M_PI - 0.5f * M_PI;
  em[0] = 0.0795774715f * vec2(cos(zeroeth_phase), sin(zeroeth_phase));

  for (uint i = 1; i < moment_coeffs; ++i) {
    em[i] = vec2(0);
    for (uint j = 0; j < i; ++j) {
      em[i] += tm[i - j] 
             * complex_mult(em[j], 
                            vec2(0, 2.f * M_PI * float(i - j) / float(i)));
    } // for (uint j)
  } // for (uint i)

  em[0] = 2.f * em[0];

  return em;
}

vec2[moment_coeffs] levinsons_algorithm(in vec2[moment_coeffs] fc) {
  vec2[moment_coeffs] rm;
  rm[0] = vec2(1.f / fc[0].x, 0);

  vec2[moment_coeffs] flipped_solution;

  for (uint i = 1; i < moment_coeffs; ++i) {
    vec2 dp = rm[0].x * fc[i];
    for (uint j = 1; j < i; ++j)
      dp += complex_mult(rm[j], fc[i - j]);
    
    float factor = 1.f / (1.f - sdot(dp));

    for (uint j = 1; j < i; ++j)
      flipped_solution[j] = complex_conj(rm[i - j]);
    flipped_solution[i] = vec2(rm[0].x, 0);

    rm[0] = vec2(factor * rm[0].x, 0);
    for (uint j = 1; j < i; ++j)
      rm[j] = factor * (rm[j] - complex_mult(flipped_solution[j], dp));
    rm[i] = factor * (-flipped_solution[i].x * dp);
  } // for (uint i)

  return rm;
}

vec2 fast_herglotz_trf(in vec2 circle_point, in vec2[moment_coeffs] em, in vec2[moment_coeffs] pm) {
  vec2 conj_circle_point = complex_conj(circle_point);

  pm[0] = vec2(pm[0].x, 0);
  for (uint j = 1; j < moment_coeffs; ++j)
    pm[j] = pm[j] + complex_mult(conj_circle_point, pm[j - 1]);
  
  vec2 dp = vec2(0);
  for (uint j = 1; j < moment_coeffs; ++j)
    dp += complex_mult(pm[moment_coeffs - j - 1], em[j]);
  
  return em[0] + 2.f * complex_divd(dp, pm[moment_coeffs - 1]);
}

void prepare_reflectance(in float[moment_coeffs] bm, out vec2[moment_coeffs] em, out vec2[moment_coeffs] pm) {
  em = trigonometric_to_exponential_moments(bm);
  pm = levinsons_algorithm(em);
  for (uint i = 0; i < moment_coeffs; ++i)
    pm[i] = 2.f * M_PI * pm[i];
}

float evaluate_reflectance(in float phase, in vec2[moment_coeffs] em, in vec2[moment_coeffs] pm) {
  vec2 circle_point = vec2(cos(phase), sin(phase));
  vec2 trf = fast_herglotz_trf(circle_point, em, pm);
  return atan(trf.y, trf.x) * M_PI_INV + .5f;
}

vec4 evaluate_reflectance(in vec4 phase, in vec2[moment_coeffs] em, in vec2[moment_coeffs] pm) {
  vec4 rv;
  for (uint i = 0; i < 4; ++i)
    rv[i] = evaluate_reflectance(phase[i], em, pm);
  return rv;
}

// Wavelengths are [0, 1] already during rendering
float wvl_to_phase(in float wvl) { return M_PI * wvl - M_PI;       }
vec4  wvl_to_phase(in vec4 wvl)  { return M_PI * wvl - vec4(M_PI); }

float moments_to_reflectance(in float wvl, in float[moment_coeffs] bm) {
  vec2[moment_coeffs] em, pm;
  prepare_reflectance(bm, em, pm);
  return evaluate_reflectance(wvl_to_phase(wvl), em, pm);
}

vec4 moments_to_reflectance(in vec4 wvls, in float[moment_coeffs] bm) {
  vec2[moment_coeffs] em, pm;
  prepare_reflectance(bm, em, pm);
  return evaluate_reflectance(wvl_to_phase(wvls), em, pm);
}

float[moment_coeffs] unpack_moments_12x10(in uvec4 p) {
  float[moment_coeffs] m;
  for (int i = 0; i < moment_coeffs; ++i) {
    uint j = bitfieldExtract(p[i / 3],              // 0,  0,  0,  1,  1,  1,  ...
                             i % 3 * 11,            // 0,  11, 22, 0,  11, 22, ...
                             i % 3 == 2 ? 10 : 11); // 11, 11, 10, 11, 11, 10, ...
    float scale = i % 3 == 2 ? 0.0009765625f : 0.0004882813f;
    m[i] = (float(j) * scale) * 2.f - 1.f;
  }
  return m;
}

uvec4 pack_moments_12x10(in float[moment_coeffs] m) {
  uvec4 p;
  for (int i = 0; i < moment_coeffs; ++i) {
    float scale = i % 3 == 2 ? 512.f : 1024.f;
    uint j = uint(round((m[i] + 1.f) * .5f * scale));
    p[i / 3] = bitfieldInsert(p[i / 3],
                              j,
                              i % 3 * 11,
                              i % 3 == 2 ? 10 : 11);
  }
  return p;
}

#endif // MOMENTS_GLSL_GUARD
