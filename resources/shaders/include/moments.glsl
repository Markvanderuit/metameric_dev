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

float wvl_to_phase(in float wvl) { // wvls is [0, 1]-bound already during rendering
  return M_PI * wvl - M_PI;
}

vec4 wvls_to_phase(in vec4 wvls) { // wvls is [0, 1]-bound already during rendering
  return M_PI * wvls - vec4(M_PI);
}

/* float moment_to_spectrum(in float wvl, in float[moment_coeffs] bm) {
  vec2[moment_coeffs] em;
  vec2[moment_coeffs] pm;
  prepare_reflectance(bm, em, pm);
  return evaluate_reflectance(wvl_to_phase(wvl), em, pm);
} */

vec4 moment_to_spectrum(in vec4 wvls, in float[moment_coeffs] bm) {
  vec2[moment_coeffs] em;
  vec2[moment_coeffs] pm;
  prepare_reflectance(bm, em, pm);
  return evaluate_reflectance(wvls_to_phase(wvls), em, pm);
}

float[moment_coeffs] unpack_moments_12x10(in ivec4 p) {
  float[moment_coeffs] m;

  m[0]  = (float(bitfieldExtract(p[0], 0,  10)) / 512.f);
  m[1]  = (float(bitfieldExtract(p[0], 10, 10)) / 512.f);
  m[2]  = (float(bitfieldExtract(p[0], 20, 10)) / 512.f);
  m[3]  = (float(bitfieldExtract(p[1], 0,  10)) / 512.f);
  m[4]  = (float(bitfieldExtract(p[1], 10, 10)) / 512.f);
  m[5]  = (float(bitfieldExtract(p[1], 20, 10)) / 512.f);
  m[6]  = (float(bitfieldExtract(p[2], 0,  10)) / 512.f);
  m[7]  = (float(bitfieldExtract(p[2], 10, 10)) / 512.f);
  m[8]  = (float(bitfieldExtract(p[2], 20, 10)) / 512.f);
  m[9]  = (float(bitfieldExtract(p[3], 0,  10)) / 512.f);
  m[10] = (float(bitfieldExtract(p[3], 10, 10)) / 512.f);
  m[11] = (float(bitfieldExtract(p[3], 20, 10)) / 512.f);

  return m;
}

#endif // MOMENTS_GLSL_GUARD
