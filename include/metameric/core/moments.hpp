#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <numbers>

namespace met {
  constexpr static uint moment_samples = 7; // Resulting in e.g. 7+1 = 8 coefficients

  // Real moment coefficients
  using Moments = eig::Array<float, moment_samples + 1, 1>;

  // If normalize_wvl is set, maps [wvl_min, wvl_max] to [0, 1] first. Otherwise
  // treats wvl as a value in [0, 1]. Output is then mapped to [-pi, 0].
  inline
  float wavelength_to_phase(float wvl, bool normalize_wvl = false) {
    if (normalize_wvl)
      wvl = (wvl - static_cast<float>(wavelength_min)) / static_cast<float>(wavelength_range);
    return /* 2.f * */ std::numbers::pi_v<float> * wvl - std::numbers::pi_v<float>;
  }

  namespace peters {
    // Compute a discrete spectral reflectance given trigonometric moments
    Spec moments_to_spectrum(const Moments &m);
  } // namespace peters

  // Compute trigonometric moments representing a given discrete spectral reflectance
  Moments spectrum_to_moments(const Spec &s);

  // Compute a discrete spectral reflectance given trigonometric moments
  Spec moments_to_spectrum(const Moments &m);
} // namespace met