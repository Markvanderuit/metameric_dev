#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/eigen.hpp>

namespace met {
  /* Define metameric's spectral range layout */
  constexpr static float wavelength_min     = MET_WAVELENGTH_MIN;  
  constexpr static float wavelength_max     = MET_WAVELENGTH_MAX;  
  constexpr static uint  wavelength_samples = MET_WAVELENGTH_SAMPLES;

  /* Define derived variables from metameric's spectral range layout */
  constexpr static float wavelength_range = wavelength_max - wavelength_min;  
  constexpr static float wavelength_ssize = wavelength_range / static_cast<float>(wavelength_samples);  
  constexpr static float wavelength_ssinv = static_cast<float>(wavelength_samples) / wavelength_range;

  /* Define program's underlying spectrum/color/cmfs classes as just renamed Eigen objects */
  using CMFS  = eig::Matrix<float, 3, wavelength_samples>;
  using Spec  = eig::Array<float, wavelength_samples, 1>;
  using Color = eig::Array<float, 3, 1>;

  /* Define color matching functions, SPD models, and other models */
  namespace models {
    // Linear color space transformations
    extern eig::Matrix3f xyz_to_srgb_transform;
    extern eig::Matrix3f srgb_to_xyz_transform;

    // Color matching functions
    extern CMFS cmfs_cie_xyz;
    extern CMFS cmfs_srgb;

    // Illuminant spectra
    extern Spec emitter_cie_d65;
    extern Spec emitter_cie_e;
  } // namespace models

  // Given a spectral bin, obtain the relevant central wavelength
  constexpr inline
  float wavelength_at_index(size_t i) {
    debug::check_expr(i >= 0 && i < wavelength_samples, fmt::format("index {} out of range", i));
    return wavelength_min + wavelength_ssize * (static_cast<float>(i) + .5f);
  }

  // Given a wavelength, obtain the relevant spectral bin's index
  constexpr inline
  size_t index_at_wavelength(float wvl) {
    debug::check_expr(wvl >= wavelength_min && wvl <= wavelength_max, 
                      fmt::format("wavelength {} out of range", wvl));
    const uint i = static_cast<uint>((wvl - wavelength_min) * wavelength_ssinv);
    return std::min(i, wavelength_samples - 1);
  }

  // Load a discrete spectral distribution from sequentially increasing wvl/value data
  Spec spectrum_from_data(std::span<const float> wvls, 
                          std::span<const float> values);

  // Load a discrete trio of color matching functions from sequentially increasing wvl/value data
  CMFS cmfs_from_data(std::span<const float> wvls, 
                      std::span<const float> values_x,
                      std::span<const float> values_y,
                      std::span<const float> values_z);
  
  // Convert a spectral emission distr. to cie XYZ
  inline
  Color emission_to_xyz(const Spec &sd) {
    const float k = 1.f / (models::cmfs_cie_xyz.row(1).array().transpose() * sd).sum();
    return k * models::cmfs_cie_xyz * sd.matrix();
  }

  // Convert a spectral reflectance distr. to cie XYZ under a given illuminant whitepoint
  inline
  Color reflectance_to_xyz(const Spec &sd, const Spec &illum = models::emitter_cie_d65) {
    const float k = 1.f / (models::cmfs_cie_xyz.row(1).array().transpose() * illum).sum();
    return k * models::cmfs_cie_xyz * (illum * sd).matrix();
  }
  
  // Convert a color in cie XYZ to linear sRGB
  inline
  Color xyz_to_srgb(const Color &c) {
    return models::xyz_to_srgb_transform * c.matrix();
  }

  // Convert a color in linear sRGB to cie XYZ
  inline
  Color srgb_to_xyz(const Color &c) {
    return models::srgb_to_xyz_transform * c.matrix();
  }

  // Convert a gamma-corrected sRGB value to linear sRGB
  template <typename Float>
  constexpr inline
  Float gamma_srgb_to_linear_srgb(Float f) {
    return f <= 0.003130 ? f * 12.92 : std::pow<Float>(f, 1.0 / 2.4) * 1.055 - 0.055;
  }

  // Convert a linear sRGB value to gamma-corrected sRGB
  template <typename Float>
  constexpr inline
  Float linear_srgb_to_gamma_srgb(Float f) {
    return f <= 0.04045 ? f / 12.92 : std::pow<Float>((f + 0.055) / 1.055, 2.4);
  }

  // Convert a gamma-corrected sRGB value to linear sRGB
  inline
  Color gamma_srgb_to_linear_srgb(Color c) {
    std::ranges::transform(c, c.begin(), gamma_srgb_to_linear_srgb<float>);
    return c;
  }

  // Convert a linear sRGB value to gamma-corrected sRGB
  inline
  Color linear_srgb_to_gamma_srgb(Color c) {
    std::ranges::transform(c, c.begin(), linear_srgb_to_gamma_srgb<float>);
    return c;
  }
} // namespace met