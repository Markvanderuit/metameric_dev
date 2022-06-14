#pragma once

#include <metameric/core/detail/eigen.hpp>

namespace met {
  /* Define metameric's spectral range layout */
  constexpr static float  wavelength_min         = 360.f;  
  constexpr static float  wavelength_max         = 830.f;  
  constexpr static float  wavelength_range       = wavelength_max - wavelength_min;  
  constexpr static size_t wavelength_samples     = 31;
  constexpr static float  wavelength_sample_size = wavelength_range 
                                                 / static_cast<float>(wavelength_samples);  

  /* Define program's underlying spectrum/color/cmfs classes as just eigen objects */
  using Spectrum = eig::Array<float, wavelength_samples, 1>;
  using Color    = eig::Array<float, 3, 1>;
  using CMFS     = eig::Matrix<float, wavelength_samples, 3>;

  // Given a spectral bin, obtain the relevant central wavelength
  constexpr 
  float wavelength_at_index(size_t i) {
    return (static_cast<float>(i) + .5f) * wavelength_sample_size + wavelength_min;
  }

  // Given a wavelength, obtain the relevant spectral bin's index
  constexpr 
  size_t index_at_wavelength(float f) {
    return static_cast<size_t>((f - wavelength_min) / wavelength_sample_size);
  }
  
  extern CMFS cmfs_ciexyz;
  Color spectrum_to_xyz(const Spectrum &s) {

  }
  
  Color xyz_to_srgb(const Color &c) {
    const eig::Matrix3f m {{ 3.240479f, -1.537150f, -0.498535f },
                           {-0.969256f,  1.875991f,  0.041556f },
                           { 0.055648f, -0.204043f,  1.057311f }};
    return (m * c.matrix());
  }

  Color srgb_to_xyz(const Color &c) {
    const eig::Matrix3f m {{ 0.412453f, 0.357580f, 0.180423f },
                           { 0.212671f, 0.715160f, 0.072169f },
                           { 0.019334f, 0.119193f, 0.950227f }};
    return (m * c.matrix());
  }
} // namespace met