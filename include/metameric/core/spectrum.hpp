#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>
#include <compare>

namespace met {
  /* Define metameric's spectral range layout */
  constexpr static float wavelength_min     = MET_WAVELENGTH_MIN;  
  constexpr static float wavelength_max     = MET_WAVELENGTH_MAX;  
  constexpr static uint  wavelength_samples = MET_WAVELENGTH_SAMPLES;

  /* Define derived variables from metameric's spectral range layout */
  constexpr static float wavelength_range = wavelength_max - wavelength_min;  
  constexpr static float wavelength_ssize = wavelength_range / static_cast<float>(wavelength_samples);  
  constexpr static float wavelength_ssinv = static_cast<float>(wavelength_samples) / wavelength_range;

  /* Define program's underlying spectrum/cmfs/color classes as just renamed Eigen objects */
  using CMFS = eig::Matrix<float, wavelength_samples, 3>;
  using Spec = eig::Array<float, wavelength_samples, 1>;
  using Colr = eig::Array<float, 3, 1>;

  /* Forcibly unaligned types */
  using UnalCMFS = eig::Matrix<float, wavelength_samples, 3, eig::DontAlign>;
  using UnalSpec = eig::Array<float, wavelength_samples, 1, eig::DontAlign>;
  using UnalColr = eig::Array<float, 3, 1, eig::DontAlign>;

  /* 16-byte aligned types */
  using AlSpec = eig::AlArray<float, wavelength_samples>;
  using AlColr = eig::AlArray<float, 3>;

  namespace io {
    // Load a discrete spectral distribution from sequentially increasing wvl/value data
    Spec spectrum_from_data(std::span<const float> wvls, 
                            std::span<const float> values);

    // Load a discrete trio of color matching functions from sequentially increasing wvl/value data
    CMFS cmfs_from_data(std::span<const float> wvls, 
                        std::span<const float> values_x,
                        std::span<const float> values_y,
                        std::span<const float> values_z);
  }

  /* Define color matching functions, SPD models, etc. */
  namespace models {
    // Linear color space transformations
    extern eig::Matrix3f xyz_to_srgb_transform;
    extern eig::Matrix3f srgb_to_xyz_transform;

    // Color matching functions
    extern CMFS cmfs_cie_xyz; // CIE 1931 2 deg. color matching functions
    extern CMFS cmfs_srgb;    // Shorthand for ((srgb_to_xyz_transform * cmfs_cie_xyz))

    // Illuminant spectra
    extern Spec emitter_cie_e;        // CIE standard illuminant E, equal energy
    extern Spec emitter_cie_d65;      // CIE standard illuminant D65, noon daylight
    extern Spec emitter_cie_fl2;      // CIE standard illuminant FL2
    extern Spec emitter_cie_fl11;     // CIE standard illuminant FL11
    extern Spec emitter_cie_ledb1;    // CIE standard illuminant LED-B1; blue LED
    extern Spec emitter_cie_ledrgb1;  // CIE standard illuminant LED-RGB1; R/G/B LEDs
  } // namespace models

  // Given a spectral bin, obtain the relevant central wavelength
  constexpr inline
  float wavelength_at_index(size_t i) {
    debug::check_expr_dbg(i >= 0 && i < wavelength_samples, fmt::format("index {} out of range", i));
    return wavelength_min + wavelength_ssize * (static_cast<float>(i) + .5f);
  }

  // Given a wavelength, obtain the relevant spectral bin's index
  constexpr inline
  size_t index_at_wavelength(float wvl) {
    debug::check_expr_dbg(wvl >= wavelength_min && wvl <= wavelength_max, fmt::format("wavelength {} out of range", wvl));
    const uint i = static_cast<uint>((wvl - wavelength_min) * wavelength_ssinv);
    return std::min(i, wavelength_samples - 1);
  }
  
  /* Spectral mapping object defines how a reflectance-to-color conversion is performed */
  struct SpectralMapping {
    /* Mapping components */

    UnalCMFS cmfs       = models::cmfs_cie_xyz;    // Color matching or sensor response functions
    UnalSpec illuminant = models::emitter_cie_d65; // Illuminant under which observation is performed
    uint n_scatters     = 0;                       // Nr. of indirect scatterings of reflectance

    /* Mapping functions */

    CMFS finalize() const {
      auto cmfs_col = cmfs.array().colwise();
      float k = 1.f / (cmfs_col * illuminant).col(1).sum();
      return k * (cmfs_col * illuminant);
    }

    // Given a known reflectance, simplify the CMFS/illuminant/indirections into a single object
    CMFS finalize(const Spec &sd) const {
      auto cmfs_col = cmfs.array().colwise();

      // If n_scatters > 0, multiply illuminant by reflectance to simulate
      // repeated scattering of indirect lighting
      Spec e = illuminant * (n_scatters == 0 ? 1.f : sd.pow(n_scatters).eval());

      // Normalization factor is applied over the unscattered illuminant
      float k = 1.f / (cmfs_col * illuminant).col(1).sum();

      return k * (cmfs_col * e);
    }

    // Obtain a color by applying this spectral mapping
    Colr apply_color(const Spec &sd) const {
      return finalize(sd).transpose() * sd.matrix();
    }

    // Obtain power by applying this mapping's illuminant
    Spec apply_power(const Spec &sd) const {
      Spec e = illuminant * (n_scatters == 0 ? 1.f : sd.pow(n_scatters).eval());
      return e * sd;
    }

    bool operator==(const SpectralMapping &o) const {
      return cmfs == o.cmfs && 
             illuminant.matrix() == o.illuminant.matrix() && 
             n_scatters == o.n_scatters;
    }
  };
  
  // Shorthand for consistency
  using Mapp = SpectralMapping;

  // Convert a spectral reflectance distr. to a color under a given mapping
  inline
  Colr reflectance_to_color(const Spec &sd, const Mapp &m = Mapp()) {
    return m.apply_color(sd);
  }

  inline
  Spec reflectance_to_power(const Spec &sd, const Mapp &m = Mapp()) {
    return m.apply_power(sd);
  }

  // Convert a gamma-corrected sRGB value to linear sRGB
  template <typename Float>
  constexpr inline
  Float gamma_srgb_to_linear_srgb_f(Float f) {
    return f <= 0.003130 ? f * 12.92 : std::pow<Float>(f, 1.0 / 2.4) * 1.055 - 0.055;
  }

  // Convert a linear sRGB value to gamma-corrected sRGB
  template <typename Float>
  constexpr inline
  Float linear_srgb_to_gamma_srgb_f(Float f) {
    return f <= 0.04045 ? f / 12.92 : std::pow<Float>((f + 0.055) / 1.055, 2.4);
  }

  // Convert a gamma-corrected sRGB value to linear sRGB
  inline
  Colr gamma_srgb_to_linear_srgb(Colr c) {
    std::ranges::transform(c, c.begin(), gamma_srgb_to_linear_srgb_f<float>);
    return c;
  }

  // Convert a linear sRGB value to gamma-corrected sRGB
  inline
  Colr linear_srgb_to_gamma_srgb(Colr c) {
    std::ranges::transform(c, c.begin(), linear_srgb_to_gamma_srgb_f<float>);
    return c;
  }
} // namespace met