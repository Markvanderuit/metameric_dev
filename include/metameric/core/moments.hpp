#pragma once

#include <metameric/core/spectrum.hpp>

namespace met {
  // Output moment coefficients
  constexpr static uint moment_samples = 11;
  using Moments = eig::Array<float, moment_samples + 1, 1>;

  // Compute trigonometric moments representing a given discrete spectral reflectance
  Moments spectrum_to_moments(const Spec &s);

  // Compute a discrete spectral reflectance given trigonometric moments
  Spec moments_to_spectrum(const Moments &m);
} // namespace met