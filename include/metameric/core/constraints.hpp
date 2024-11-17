#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/record.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/utility.hpp>

namespace met {
  // Small helper struct; specifying linear illumination or observer constraints
  struct LinearConstraint {
    bool is_active = true; // Constraint is active
    uint cmfs_j    = 0;    // Index of observer in scene data
    uint illm_j    = 0;    // Index of illuminant in scene data
    Colr colr_j    = 0.f;  // Color specified as constraint

  public:
    bool operator==(const LinearConstraint &o) const;
    bool is_similar(const LinearConstraint &o) const;
  };

  // Small helper struct; specifying nonlinear illumination constraints with truncated power series
  struct NLinearConstraint {
    bool              is_active = true; // Constraint is in effect
    uint              cmfs_j    = 0;    // Index of observer in scene data
    std::vector<Spec> powr_j    = { };  // Interreflection power series
    Colr              colr_j    = 0.f;  // Color specified as constraint

  public:      
    bool operator==(const NLinearConstraint &o) const;
    bool is_similar(const NLinearConstraint &o) const;
  };

  // Concept for a constraint on metameric behavior used throughout the application's
  // spectral uplifting pipeline. Constraints can exist on their own, or together
  // with other, secondary constraints.
  template <typename Ty>
  concept is_metameric_constraint = requires(Ty t, Scene scene, Uplifting uplifting, uint seed, uint samples, Colr c) {
    // The constraint allows for realizing a metamer (and attached color under uplifting's color system)
    { t.realize(scene, uplifting) } -> std::same_as<SpectrumSample>;

    // The constraint allows for realizing mismatch volume sample points
    { t.realize_mismatch(scene, uplifting, seed, samples) } -> std::same_as<std::vector<MismatchSample>>;
  };

  // Concept for a constraint on metameric behavior used throughout the application's
  // spectral uplifting pipeline. Constraints enforce roundtrip in an uplifting's
  // color system to this specified value. This behavior can be turned off, so
  // minor error is better handled. See components.hpp for Uplifting::Vertex
  template <typename Ty>
  concept is_roundtrip_constraint = is_metameric_constraint<Ty> && requires(Ty t) {
    // Base roundtrip linear constraint can be (de)activated
    { t.is_base_active } -> std::convertible_to<bool>;

    // Base roundtrip targets this color value
    { t.colr_i } -> std::convertible_to<Colr>;
  };

  // Concept defining the expected components of linear color-system constraints
  template <typename Ty>
  concept is_linear_constraint = is_roundtrip_constraint<Ty> && requires(Ty t) {
    // The constraint specifies secondary color data, under secondary
    // color systems assembled from scene data
    { t.cstr_j } -> std::convertible_to<std::vector<LinearConstraint>>;
  };

  // Concept defining the expected components of nonlinear color-system constraints
  template <typename Ty>
  concept is_nlinear_constraint = is_roundtrip_constraint<Ty> && requires(Ty t) {
    // The constraint specifies secondary color data, under secondary
    // color systems assembled from scene data
    { t.cstr_j } -> std::convertible_to<std::vector<NLinearConstraint>>;
  };

  // Constraint imposing reproduction of a specific spectral reflectance.
  struct MeasurementConstraint {
    Spec measure = 0.f; // Measured spectral data
    
  public:
    // Simply return the constraint's measure
    SpectrumSample realize(const Scene &scene, const Uplifting &uplifting) const;

    // Generate points on the constraint's metamer mismatching volume
    std::vector<MismatchSample> realize_mismatch(const Scene &scene, const Uplifting &uplifting, uint seed, uint samples) const {
      return { };
    }

  public:
    bool operator==(const MeasurementConstraint &o) const;
  };
  static_assert(is_metameric_constraint<MeasurementConstraint>);

  // Constraint imposing specific color reproduction under a set of known
  // color systems, i.e. direct illumination.
  struct DirectColorConstraint {
    bool                          is_base_active = true; // Base roundtrip linear constraint is active
    Colr                          colr_i         = 0.0;  // Expected base roundtrip color
    std::vector<LinearConstraint> cstr_j         = { };  // Secondary constraints for color reproduction
    
  public:
    // Solve for the constraint's metamer based on its current configuration
    SpectrumSample realize(const Scene &scene, const Uplifting &uplifting) const;

    // Generate points on the constraint's metamer mismatching volume
    std::vector<MismatchSample> realize_mismatch(const Scene &scene, const Uplifting &uplifting, uint seed, uint samples) const;

  public:
    bool operator==(const DirectColorConstraint &o) const;
  };
  static_assert(is_linear_constraint<DirectColorConstraint>);

  // Constraint imposing specific color reproduction under a set of known
  // color systems, i.e. direct illumination. The base color is sampled
  // from a scene surface.
  struct DirectSurfaceConstraint {
    bool                          is_base_active = true; // Base roundtrip linear constraint is active
    Colr                          colr_i         = 0.0;  // Expected base roundtrip color, obtained from first surface
    std::vector<LinearConstraint> cstr_j         = { };  // Secondary constraints for color reproduction

    // Surface data recorded through user interaction
    SurfaceInfo surface = SurfaceInfo::invalid();
    
  public:
    // Solve for the constraint's metamer based on its current configuration
    SpectrumSample realize(const Scene &scene, const Uplifting &uplifting) const;

    // Generate points on the constraint's metamer mismatching volume
    std::vector<MismatchSample> realize_mismatch(const Scene &scene, const Uplifting &uplifting, uint seed, uint samples) const;

  public:
    bool operator==(const DirectSurfaceConstraint &o) const;
  };
  static_assert(is_linear_constraint<DirectSurfaceConstraint>);
  
  // Constraint imposing specific color reproduction under a known illuminant,
  // accounting for nonlinear interreflections as well as linear constraints. 
  // The interreflection system is based on measured light transport data from a scene surface.
  struct IndirectSurfaceConstraint {
    bool                           is_base_active = true;  // Base roundtrip linear constraint is active
    Colr                           colr_i         = 0.0;   // Expected base roundtrip color, obtained from the first underlying surface
    std::vector<NLinearConstraint> cstr_j         = { };   // Secondary nonlinear constraints for color reproduction

    // Surface data recorded through user interaction for each secondary nonlinear constraint; the first specifies base constraint
    std::vector<SurfaceInfo> surfaces = { };   
    
  public:
    // Solve for the constraint's metamer based on its current configuration
    SpectrumSample realize(const Scene &scene, const Uplifting &uplifting) const;

    // Generate points on the constraint's metamer mismatching volume
    std::vector<MismatchSample> realize_mismatch(const Scene &scene, const Uplifting &uplifting, uint seed, uint samples) const;

  public:
    bool operator==(const IndirectSurfaceConstraint &o) const;
  };
  static_assert(is_nlinear_constraint<IndirectSurfaceConstraint>);

  // JSON (de)serialization of constraint variants
  void from_json(const json &js, DirectColorConstraint &c);
  void to_json(json &js, const DirectColorConstraint &c);
  void from_json(const json &js, LinearConstraint &c);
  void to_json(json &js, const LinearConstraint &c);
  void from_json(const json &js, MeasurementConstraint &c);
  void to_json(json &js, const MeasurementConstraint &c);
  void from_json(const json &js, DirectSurfaceConstraint &c);
  void to_json(json &js, const DirectSurfaceConstraint &c);
  void from_json(const json &js, IndirectSurfaceConstraint &c);
  void to_json(json &js, const IndirectSurfaceConstraint &c);
} // namespace met

template<>
struct fmt::formatter<met::DirectColorConstraint>{
  template <typename context_ty>
  constexpr auto parse(context_ty& ctx) { 
    return ctx.begin(); 
  }

  template <typename fmt_context_ty>
  constexpr auto format(const met::DirectColorConstraint& ty, fmt_context_ty& ctx) const {
    std::string s = "direct";
    return fmt::format_to(ctx.out(), "{}", s);
  }
};

template<>
struct fmt::formatter<met::MeasurementConstraint>{
  template <typename context_ty>
  constexpr auto parse(context_ty& ctx) { 
    return ctx.begin(); 
  }

  template <typename fmt_context_ty>
  constexpr auto format(const met::MeasurementConstraint& ty, fmt_context_ty& ctx) const {
    std::string s = "measurement";
    return fmt::format_to(ctx.out(), "{}", s);
  }
};

template<>
struct fmt::formatter<met::DirectSurfaceConstraint>{
  template <typename context_ty>
  constexpr auto parse(context_ty& ctx) { 
    return ctx.begin(); 
  }

  template <typename fmt_context_ty>
  constexpr auto format(const met::DirectSurfaceConstraint& ty, fmt_context_ty& ctx) const {
    std::string s = "direct surface";
    return fmt::format_to(ctx.out(), "{}", s);
  }
};

template<>
struct fmt::formatter<met::IndirectSurfaceConstraint>{
  template <typename context_ty>
  constexpr auto parse(context_ty& ctx) { 
    return ctx.begin(); 
  }

  template <typename fmt_context_ty>
  constexpr auto format(const met::IndirectSurfaceConstraint, fmt_context_ty& ctx) const {
    std::string s = "indirect surface";
    return fmt::format_to(ctx.out(), "{}", s);
  }
};