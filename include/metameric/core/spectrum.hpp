#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>

namespace met {
  /* 
    Spectrum/color/color system data
  */

  /* Define metameric's spectral range layout */
  constexpr static float wavelength_min     = MET_WAVELENGTH_MIN;
  constexpr static float wavelength_max     = MET_WAVELENGTH_MAX;  
  constexpr static uint  wavelength_samples = MET_WAVELENGTH_SAMPLES;
  constexpr static uint  wavelength_bases   = MET_WAVELENGTH_BASES;

  /* Define derived variables from metameric's spectral range layout */
  constexpr static float wavelength_range = wavelength_max - wavelength_min;
  constexpr static float wavelength_ssize = wavelength_range / static_cast<float>(wavelength_samples);  
  constexpr static float wavelength_ssinv = static_cast<float>(wavelength_samples) / wavelength_range;

  /* Define program's underlying spectrum/cmfs/color types as renamed Eigen types */
  using CMFS = eig::Matrix<float, wavelength_samples, 3>; // Color matching function matrix
  using Spec = eig::Array<float, wavelength_samples, 1>;  // Discrete spectrum matrix
  using Colr = eig::Array<float,  3, 1>;                  // Color signal matrix

  /* Basis function object, using a offset around its mean */
  struct Basis {
    using mat_type = eig::Matrix<float, wavelength_samples, wavelength_bases>;
    using vec_type = eig::Vector<float, wavelength_bases>;

  public:
    Spec     mean;  // Ignored r.n.
    float    scale; // Ignored r.n.
    mat_type func;  // Basis functions around mean offset

  public: // Boilerplate
    Spec apply(const vec_type &c) const { 
      return ((func * c).array()).cwiseMax(0.f).cwiseMin(1.f).eval();
    }
    std::vector<Spec> apply(std::span<const vec_type> cs) const;

    auto operator()(const vec_type &c) const { return apply(c); }
    auto operator()(std::span<const vec_type> cs) const { return apply(cs); }
    bool operator==(const Basis &o) const;
    void to_stream(std::ostream &str) const;
    void from_stream(std::istream &str);
  };

  // Concept for a color system, which can take a spectral distribution or set of spectra,
  // and output one or more color signals in response
  template <typename Ty>
  concept is_colr_system = requires(Ty t, std::span<const Spec> sds, bool as_rgb) {
    { t.apply(sds.front(), as_rgb)      } -> std::same_as<Colr>;
    { t.apply(sds, as_rgb)              } -> std::same_as<std::vector<Colr>>;
    { t.operator()(sds.front(), as_rgb) } -> std::same_as<Colr>;
    { t.operator()(sds, as_rgb)         } -> std::same_as<std::vector<Colr>>;
  };

  /* Object defining how a reflectance-to-color conversion is performed */
  struct ColrSystem {
    CMFS cmfs;       // Sensor color matching or response functions
    Spec illuminant; // Illuminant under which observation is performed

  public:
    CMFS finalize(bool as_rgb = true) const;                                     // Simplify the CMFS/illuminant into color system spectra
    Colr apply(const Spec &s, bool as_rgb = true) const;                         // Obtain a color from a reflectance in this color system
    std::vector<Colr> apply(std::span<const Spec> sd, bool as_rgb = true) const; // Obtain colors from reflectances in this color system
    
  public: // Boilerplate
    auto operator()(const Spec &s, bool as_rgb = true) const { return apply(s, as_rgb); }
    auto operator()(std::span<const Spec> s, bool as_rgb = true) const { return apply(s, as_rgb); }
    bool operator==(const ColrSystem &o) const;
    void to_stream(std::ostream &str) const;
    void from_stream(std::istream &str);
  };
  static_assert(is_colr_system<ColrSystem>);

  /* Object defining how a reflectance-to-color conversion is performed,
     given a truncated power series describing interreflections */
  struct IndirectColrSystem {
    CMFS              cmfs;   // Sensor color matching or response functions
    std::vector<Spec> powers; // Truncated power series describing partial interreflections
  
  public:
    std::vector<CMFS> finalize(bool as_rgb = true) const;                        // Simplify the recrursive system into color system spectra
    Colr apply(const Spec &s, bool as_rgb = true) const;                         // Obtain a color from a reflectance in this color system
    std::vector<Colr> apply(std::span<const Spec> sd, bool as_rgb = true) const; // Obtain colors from reflectances in this color system

  public: // Boilerplate
    auto operator()(const Spec &s, bool as_rgb = true) const { return apply(s, as_rgb); }
    auto operator()(std::span<const Spec> s, bool as_rgb = true) const { return apply(s, as_rgb); }
    bool operator==(const IndirectColrSystem &o) const;
    void to_stream(std::ostream &str) const;
    void from_stream(std::istream &str);
  };
  static_assert(is_colr_system<IndirectColrSystem>);

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
    Color space conversion functions
  */

  // Convert a value in sRGB to linear sRGB
  constexpr inline
  float srgb_to_lrgb_f(float f) {
    return f <= 0.04045f 
      ? f / 12.92f 
      : powf((f + 0.055f) / 1.055f, 2.4f);
  }

  // Convert a value in linear sRGB to sRGB
  constexpr inline
  float lrgb_to_srgb_f(float f) {
    return f <= 0.003130f 
      ? f * 12.92f 
      : powf(f, 1.0f / 2.4f) * 1.055f - 0.055f;
  }

  // sRGB/linear sRGB/XYZ conversion shorthands
  Colr   srgb_to_lrgb(Colr c);
  Colr   lrgb_to_srgb(Colr c);
  inline Colr xyz_to_lrgb(Colr c) { return models::xyz_to_srgb_transform * c.matrix(); }
  inline Colr lrgb_to_xyz(Colr c) { return models::srgb_to_xyz_transform * c.matrix(); }
  inline Colr xyz_to_srgb(Colr c) { return lrgb_to_srgb(xyz_to_lrgb(c)); }
  inline Colr srgb_to_xyz(Colr c) { return lrgb_to_xyz(srgb_to_lrgb(c)); }

  // xyY/xy conversion shorthands
  inline eig::Array2f xyz_to_xy(Colr c) {
    auto sum = c.sum();
    return (eig::Array2f() << c.head<2>() / (sum > 0.f ? sum : 1.f)).finished();
  }
  inline Colr xyz_to_xyY(Colr c) {
    auto sum = c.sum();
    return (eig::Array3f() << c.head<2>() / (sum > 0.f ? sum : 1.f), c.y()).finished();
  }

  /* 
    Spectrum helper functions
  */

  // Given a spectral bin, obtain the relevant central wavelength of that bin
  constexpr inline
  float wavelength_at_index(size_t i) {
    return wavelength_min + wavelength_ssize * (static_cast<float>(i) + .5f);
  }

  // Given a wavelength, obtain the relevant surrounding spectral bin's index
  constexpr inline
  size_t index_at_wavelength(float wvl) {
    return std::min(static_cast<uint>((wvl - wavelength_min) * wavelength_ssinv), wavelength_samples - 1);
  }

  inline
  void accumulate_spectrum(Spec &s, float wvl, float value) {
    float v = std::clamp(wvl * wavelength_samples - 0.5f, 
                         0.f, static_cast<float>(wavelength_samples - 1));
    uint  t = static_cast<uint>(v);
    float a = v - static_cast<float>(t);

    if (a == 0.f) {
      s[t] += value;
    } else {
      s[t]     += value * (1.f - a);
      s[t + 1] += value * a;
    } 
  }
  
  inline
  float sample_spectrum(const float &wvl, const Spec &s) {
    float v = std::clamp(wvl * wavelength_samples - 0.5f, 
                         0.f, static_cast<float>(wavelength_samples - 1));
    uint  t = static_cast<uint>(v);
    float a = v - static_cast<float>(t);
    return a == 0.f ? s[t] : s[t] + a * (s[t + 1] - s[t]);
  }

  inline
  Colr sample_cmfs(const CMFS &cmfs, float wvl) {
    float v = std::clamp(wvl * wavelength_samples - 0.5f, 
                         0.f, static_cast<float>(wavelength_samples - 1));
    uint  t = static_cast<uint>(v);
    float a = v - static_cast<float>(t);
    
    if (a == 0.f)
      return cmfs.row(t);
    else
      return cmfs.row(t) + a * (cmfs.row(t + 1) - cmfs.row(t));
  }
  
  // Given a set of wavelengths and a spectrum, sample four valeus from
  // these wavelengths
  eig::Array4f sample_spectrum(const eig::Array4f &wvls, const Spec &s);

  // Given a set of wavelengths and spectral values, accumulate these into
  // a spectrum at the right positions
  void accumulate_spectrum(Spec &s, const eig::Array4f &wvls, const eig::Array4f &values);
  Spec accumulate_spectrum(const eig::Array4f &wvls, const eig::Array4f &values);

  // Given a set of wavelengths and spectral values, integrate
  // a color matching function into a resulting color
  Colr integrate_cmfs(const CMFS &cmfs, eig::Array4f wvls, eig::Array4f values);

  // Simple safe dividor in case some components may fall to 0
  inline
  Spec safe_div(const Spec &s, const Spec &div) {
    return (s / div.NullaryExpr([](float f) { return f != 0.f ? f : 1.f; })).eval();
  }

  // Luminance corresponding to a linear sRGB value
  inline
  float luminance(const Colr &c) {
    return c.matrix().dot(eig::Vector3f { 0.212671f, 0.715160f, 0.072169f });
  }
} // namespace met