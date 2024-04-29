#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/record.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/detail/scene_fwd.hpp>
#include <utility>

namespace met {
  // Small helper struct for implementations of is_colr_constraint derivatives
  struct ColrConstraint {
    // Constraint data
    bool is_active = true; // Constraint is in effect
    uint cmfs_j    = 0;    // Index of observer in scene data
    uint illm_j    = 0;    // Index of illuminant in scene data
    Colr colr_j    = 0.f;  // Color under this direct color system

  public:
    bool operator==(const ColrConstraint &o) const;
    bool is_similar(const ColrConstraint &o) const;
  };

  // Small helper struct for constraints in IndirectSurfaceConstraint
  struct PowrConstraint {
    // Constraint data
    bool              is_active = true; // Constraint is in effect
    uint              cmfs_j    = 0;    // Index of observer in scene data
    std::vector<Spec> powr_j    = { };  // Interreflection power series
    Colr              colr_j    = 0.f;  // Color under this direct color system

    // Surface data recorded through user interaction
    SurfaceInfo surface = SurfaceInfo::invalid();

  public:      
    bool operator==(const PowrConstraint &o) const;
    bool is_similar(const PowrConstraint &o) const;
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

  // Concept defining the expected components of color-system constraints
  template <typename Ty>
  concept is_colr_constraint = is_roundtrip_constraint<Ty> && requires(Ty t) {
    // The constraint specifies secondary color data, under secondary
    // color systems assembled from scene data
    { t.cstr_j } -> std::convertible_to<std::vector<ColrConstraint>>;
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
    bool                        is_base_active = true; // Base roundtrip linear constraint is active
    Colr                        colr_i         = 0.0;  // Expected base roundtrip color
    std::vector<ColrConstraint> cstr_j         = { };  // Secondary constraints for color reproduction
    
  public:
    // Solve for the constraint's metamer based on its current configuration
    SpectrumSample realize(const Scene &scene, const Uplifting &uplifting) const;

    // Generate points on the constraint's metamer mismatching volume
    std::vector<MismatchSample> realize_mismatch(const Scene &scene, const Uplifting &uplifting, uint seed, uint samples) const;

  public:
    bool operator==(const DirectColorConstraint &o) const;
  };
  static_assert(is_colr_constraint<DirectColorConstraint>);

  // Constraint imposing specific color reproduction under a set of known
  // color systems, i.e. direct illumination. The base color is sampled
  // from a scene surface.
  struct DirectSurfaceConstraint {
    bool                        is_base_active = true; // Base roundtrip linear constraint is active
    Colr                        colr_i         = 0.0;  // Expected base roundtrip color, obtained from first surface
    std::vector<ColrConstraint> cstr_j         = { };  // Secondary constraints for color reproduction

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
  static_assert(is_colr_constraint<DirectSurfaceConstraint>);


  // Constraint imposing specific color reproduction under a known illuminant,
  // accounting for nonlinear interreflections as well as linear constraints. 
  // The interreflection system is based on measured light transport data from a scene surface.
  struct IndirectSurfaceConstraint {
    bool                        is_base_active = true;  // Base roundtrip linear constraint is active
    Colr                        colr_i         = 0.0;   // Expected base roundtrip color, obtained from the first underlying surface
    bool                        target_direct  = false; // Free variable is the last linear, instead of last nonlinear constraint
    std::vector<ColrConstraint> cstr_j_direct  = { };   // Secondary linear constraints for color reproduction
    std::vector<PowrConstraint> cstr_j_indrct  = { };   // Secondary nonlinear constraints for color reproduction
    
  public:
    // Solve for the constraint's metamer based on its current configuration
    SpectrumSample realize(const Scene &scene, const Uplifting &uplifting) const;

    // Generate points on the constraint's metamer mismatching volume
    std::vector<MismatchSample> realize_mismatch(const Scene &scene, const Uplifting &uplifting, uint seed, uint samples) const;

  public:
    bool operator==(const IndirectSurfaceConstraint &o) const;
  };
  static_assert(is_metameric_constraint<IndirectSurfaceConstraint> && is_roundtrip_constraint<IndirectSurfaceConstraint>);

  // JSON (de)serialization of constraint variants
  void from_json(const json &js, DirectColorConstraint &c);
  void to_json(json &js, const DirectColorConstraint &c);
  void from_json(const json &js, ColrConstraint &c);
  void to_json(json &js, const ColrConstraint &c);
  void from_json(const json &js, MeasurementConstraint &c);
  void to_json(json &js, const MeasurementConstraint &c);
  void from_json(const json &js, DirectSurfaceConstraint &c);
  void to_json(json &js, const DirectSurfaceConstraint &c);
  void from_json(const json &js, IndirectSurfaceConstraint &c);
  void to_json(json &js, const IndirectSurfaceConstraint &c);
} // namespace met

namespace std {
  template <>
  struct std::formatter<met::DirectColorConstraint> : std::formatter<string_view> {
    auto format(const met::DirectColorConstraint& ty, std::format_context& ctx) const {
      std::string s = "direct";
      return std::formatter<std::string_view>::format(s, ctx);
    }
  };

  template <>
  struct std::formatter<met::MeasurementConstraint> : std::formatter<string_view> {
    auto format(const met::MeasurementConstraint& ty, std::format_context& ctx) const {
      std::string s = "measurement";
      return std::formatter<std::string_view>::format(s, ctx);
    }
  };

  template <>
  struct std::formatter<met::DirectSurfaceConstraint> : std::formatter<string_view> {
    auto format(const met::DirectSurfaceConstraint& ty, std::format_context& ctx) const {
      std::string s = "direct surface";
      return std::formatter<std::string_view>::format(s, ctx);
    }
  };

  template <>
  struct std::formatter<met::IndirectSurfaceConstraint> : std::formatter<string_view> {
    auto format(const met::IndirectSurfaceConstraint& ty, std::format_context& ctx) const {
      std::string s = "indirect surface";
      return std::formatter<std::string_view>::format(s, ctx);
    }
  };
} // namespace std