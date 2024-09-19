#pragma once

#include <metameric/core/spectrum.hpp>

namespace met {
  // Output moment coefficients
  constexpr static uint moment_coeffs = MET_MOMENT_COEFFICIENTS;
  using Moments = eig::Array<float, moment_coeffs, 1>;

  // Compute trigonometric moments representing a given discrete spectral reflectance
  Moments spectrum_to_moments(const Spec &s);

  // Compute a discrete spectral reflectance given trigonometric moments
  Spec         moments_to_spectrum(const Moments &m);
  Spec         moments_to_spectrum_lagrange(const Moments &m);
  float        moments_to_reflectance(float wvl, const Moments &m);
  eig::Array4f moments_to_reflectance(const eig::Array4f &wvls, const Moments &m);
  
  Spec generate_uniform_phase();
  Spec generate_warped_phase();
} // namespace met