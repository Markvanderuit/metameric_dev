#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>

namespace met {
  /* 
    Spectrum/color/color system data
  */

  /* Define metameric's spectral range layout */
  constexpr static float wavelength_min      = MET_WAVELENGTH_MIN;
  constexpr static float wavelength_max      = MET_WAVELENGTH_MAX;  
  constexpr static uint  wavelength_samples  = MET_WAVELENGTH_SAMPLES;
  constexpr static uint  wavelength_bases    = MET_WAVELENGTH_BASES;
  constexpr static uint  generalized_weights = MET_GENERALIZED_WEIGHTS;

  /* Define derived variables from metameric's spectral range layout */
  constexpr static float wavelength_range = wavelength_max - wavelength_min;
  constexpr static float wavelength_ssize = wavelength_range / static_cast<float>(wavelength_samples);  
  constexpr static float wavelength_ssinv = static_cast<float>(wavelength_samples) / wavelength_range;

  /* Define program's underlying spectrum/cmfs/color classes as renamed Eigen objects */
  using CMFS = eig::Matrix<float, wavelength_samples, 3>; // Color matching function matrix
  using Spec = eig::Array<float, wavelength_samples, 1>;  // Discrete spectrum matrix
  using Bary = eig::Array<float, generalized_weights, 1>; // Discrete convex weight data
  using Colr = eig::Array<float,  3, 1>;                  // Color signal matrix
  using Chro = eig::Array<float,  2, 1>;                  // Color chromaticity matrix

  /* Miscellaneous types, mostly used for basis function operations in src/core/metamer.cpp */
  using AlColr = eig::AlArray<float, 3>;
  using Basis = eig::Matrix<float, wavelength_samples, wavelength_bases>;

  /* System object defining how a reflectance-to-color conversion is performed */
  struct ColrSystem {
    // Public members
    CMFS cmfs;       // Color matching or sensor response functions, defining the observer
    Spec illuminant; // Illuminant under which observation is performed
    uint n_scatters; // Nr. of recursive scatters of observed refletance; default 1

    // Simplify the CMFS/illuminant into color system spectra
    CMFS finalize_direct() const;
    CMFS finalize_indirect(const Spec &sd) const;
    
    // Obtain a color by applying this spectral mapping
    Colr apply_color_direct(const Spec &sd) const { met_trace(); return finalize_direct().transpose() * sd.pow(n_scatters).matrix().eval();  }
    Colr apply_color_indirect(const Spec &sd) const { met_trace(); return finalize_indirect(sd).transpose() * sd.matrix();  }

    // Operator shorthands
    Colr operator()(const Spec &s) const { met_trace(); return apply_color_direct(s);  }
    bool operator==(const ColrSystem &o) const { met_trace(); return cmfs.isApprox(o.cmfs) && illuminant.isApprox(o.illuminant); }
  };

  /* 
    Hardcoded model data.
  */
  
  /* Define pre-included color matching functions, SPD models, etc. */
  namespace models {
    // Linear color space transformations
    extern eig::Matrix3f xyz_to_srgb_transform;
    extern eig::Matrix3f xyz_to_rec709_transform;
    extern eig::Matrix3f xyz_to_rec2020_transform;
    extern eig::Matrix3f xyz_to_ap1_transform;
    extern eig::Matrix3f srgb_to_xyz_transform;
    extern eig::Matrix3f rec709_to_xyz_transform;
    extern eig::Matrix3f rec2020_to_xyz_transform;
    extern eig::Matrix3f ap1_to_xyz_transform;

    // Color matching functions
    extern CMFS cmfs_cie_xyz; // CIE 1931 2 deg. color matching functions

    // Illuminant spectra
    extern Spec emitter_cie_e;        // CIE standard illuminant E, equal energy
    extern Spec emitter_cie_d65;      // CIE standard illuminant D65, noon daylight
    extern Spec emitter_cie_fl2;      // CIE standard illuminant FL2
    extern Spec emitter_cie_fl11;     // CIE standard illuminant FL11
    extern Spec emitter_cie_ledb1;    // CIE standard illuminant LED-B1; blue LED
    extern Spec emitter_cie_ledrgb1;  // CIE standard illuminant LED-RGB1; R/G/B LEDs
  } // namespace models

  /* 
    Miscellaneous functions.
  */

  // Given a spectral bin, obtain the relevant central wavelength of that bin
  constexpr inline
  float wavelength_at_index(size_t i) {
    debug::check_expr(i >= 0 && i < wavelength_samples, 
      fmt::format("index {} out of range", i));
    return wavelength_min + wavelength_ssize * (static_cast<float>(i) + .5f);
  }

  // Given a wavelength, obtain the relevant surrounding spectral bin's index
  constexpr inline
  size_t index_at_wavelength(float wvl) {
    debug::check_expr(wvl >= wavelength_min && wvl <= wavelength_max, 
      fmt::format("wavelength {} out of range", wvl));
    return std::min(static_cast<uint>((wvl - wavelength_min) * wavelength_ssinv), wavelength_samples - 1);
  }

  // Convert a value in sRGB to linear sRGB
  constexpr inline
  float srgb_to_lrgb_f(float f) {
    return f <= 0.04045f ? f / 12.92f : std::pow<float>((f + 0.055f) / 1.055f, 2.4f);
  }

  // Convert a value in linear sRGB to sRGB
  constexpr inline
  float lrgb_to_srgb_f(float f) {
    return f <= 0.003130f ? f * 12.92f : std::pow<float>(f, 1.0f / 2.4f) * 1.055f - 0.055f;
  }

  // sRGB/linear sRGB/XYZ conversion shorthands
  inline Colr srgb_to_lrgb(Colr c) { rng::transform(c, c.begin(), srgb_to_lrgb_f); return c; }
  inline Colr lrgb_to_srgb(Colr c) { rng::transform(c, c.begin(), lrgb_to_srgb_f); return c; }
  inline Colr xyz_to_lrgb(Colr c)  { return models::xyz_to_srgb_transform * c.matrix();      }
  inline Colr lrgb_to_xyz(Colr c)  { return models::srgb_to_xyz_transform * c.matrix();      }
  inline Colr xyz_to_srgb(Colr c)  { return lrgb_to_srgb(xyz_to_lrgb(c));                    }
  inline Colr srgb_to_xyz(Colr c)  { return lrgb_to_xyz(srgb_to_lrgb(c));                    }

  // Convert a XYZ color to xyY
  inline
  Chro xyz_to_xy(Colr c) {
    float y = c.sum();
    return y > 0.f ? Chro(c[0] / y, c[2] / y) : 0.f;
  }
} // namespace met