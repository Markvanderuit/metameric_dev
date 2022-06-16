#pragma once

#include <metameric/core/detail/eigen.hpp>
#include <metameric/core/utility.hpp>

namespace met {
  /* Define metameric's spectral range layout */
  constexpr static float  wavelength_min     = 400.f;  
  constexpr static float  wavelength_max     = 710.f;  
  constexpr static size_t wavelength_samples = 31;

  /* Define derived variables from metameric's spectral range layout */
  constexpr static float wavelength_range           = wavelength_max - wavelength_min;  
  constexpr static float wavelength_sample_size     = wavelength_range 
                                                    / static_cast<float>(wavelength_samples);  
  constexpr static float wavelength_sample_size_inv = static_cast<float>(wavelength_samples) 
                                                    / wavelength_range;

  /* Define program's underlying spectrum/color/cmfs classes as just eigen objects */
  using Spectrum = eig::Array<float, wavelength_samples, 1>;
  using Color    = eig::Array<float, 3, 1>;
  using CMFS     = eig::Matrix<float, 3, wavelength_samples>;

  /* Define color matching function and SPD models */
  namespace models {
    extern CMFS     cmfs_cie_xyz;
    extern Spectrum emitter_cie_d65;
    extern Spectrum emitter_cie_e;
  } // namespace models

  // Given a spectral bin, obtain the relevant central wavelength
  constexpr inline
  float wavelength_at_index(size_t i) {
    debug::check_expr(i >= 0 && i < wavelength_samples, 
                      fmt::format("index {} out of range", i));
    return wavelength_min + wavelength_sample_size * (static_cast<float>(i) + .5f);
  }

  // Given a wavelength, obtain the relevant spectral bin's index
  constexpr inline
  size_t index_at_wavelength(float wvl) {
    debug::check_expr(wvl >= wavelength_min && wvl <= wavelength_max, 
                      fmt::format("wavelength {} out of range", wvl));
    size_t i = static_cast<size_t>((wvl - wavelength_min) * wavelength_sample_size_inv);
    return std::min(i, wavelength_samples - 1);
  }

  // Load a discrete trio of color matching functions from arbitrary wvl/value data
  CMFS cmfs_from_data(std::span<const float> wvls, 
                      std::span<const float> values_x,
                      std::span<const float> values_y,
                      std::span<const float> values_z);

  // Load a discrete spectral distribution from arbitrary wvl/value data
  Spectrum spectrum_from_data(std::span<const float> wvls, 
                              std::span<const float> values);
  
  // Convert a spectral emission distr. to cie XYZ
  inline
  Color emission_to_xyz(const Spectrum &sd) {
    const float k = 1.f / (models::cmfs_cie_xyz.row(1).array() * sd.transpose()).sum();
    return k * models::cmfs_cie_xyz * sd.matrix();
  }

  // Convert a spectral reflectance distr. to cie XYZ under a given illuminant whitepoint
  inline
  Color reflectance_to_xyz(const Spectrum &sd, const Spectrum &illum = models::emitter_cie_d65) {
    const float k = 1.f / (models::cmfs_cie_xyz.row(1).array() * illum.transpose()).sum();
    return k * models::cmfs_cie_xyz * (illum * sd).matrix();
  }
  
  // Convert a color in cie XYZ to linear sRGB
  inline
  Color xyz_to_srgb(const Color &c) {
    static const eig::Matrix3f m {{ 3.240479f, -1.537150f,-0.498535f },
                                  {-0.969256f,  1.875991f, 0.041556f },
                                  { 0.055648f, -0.204043f, 1.057311f }};
    return m * c.matrix();
  }

  // Convert a color in linear sRGB to cie XYZ
  inline
  Color srgb_to_xyz(const Color &c) {
    static const eig::Matrix3f m {{ 0.412453f, 0.357580f, 0.180423f },
                                  { 0.212671f, 0.715160f, 0.072169f },
                                  { 0.019334f, 0.119193f, 0.950227f }};
    return m * c.matrix();
  }

  // Convert a gamma-corrected sRGB value to linear sRGB
  template <typename Float>
  constexpr inline
  Float srgb_to_lrgb(Float f) {
    return f <= 0.003130 ? f * 12.92
                         : std::pow<Float>(f, 1.0 / 2.4) * 1.055 - 0.055;
  }

  // Convert a linear sRGB value to gamma-corrected sRGB
  template <typename Float>
  constexpr inline
  Float lrgb_to_srgb(Float f) {
    return f <= 0.04045 ? f / 12.92
                        : std::pow<Float>((f + 0.055) / 1.055, 2.4);
  }

  // Convert a gamma-corrected sRGB value to linear sRGB
  inline
  Color srgb_to_lrgb(Color c) {
    std::ranges::transform(c, c.begin(), srgb_to_lrgb<float>);
    return c;
  }

  // Convert a linear sRGB value to gamma-corrected sRGB
  inline
  Color lrgb_to_srgb(Color c) {
    std::ranges::transform(c, c.begin(), lrgb_to_srgb<float>);
    return c;
  }
} // namespace met