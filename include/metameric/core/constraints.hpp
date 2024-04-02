#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/record.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/detail/scene_fwd.hpp>

namespace met {
  // Helper object for handling selection of a specific 
  // uplifting/vertex/constraint in the scene data
  struct ConstraintSelection {
    constexpr static uint invalid_data = 0xFFFFFFFF;

  public:
    uint uplifting_i  = invalid_data; // ID of uplifting component
    uint vertex_i     = 0;            // ID of vertex in specific uplifting
    uint constraint_i = 0;            // ID of constraint on constraint vertex; always 0 (except for the indirect surface constraint)
  
  public:
    bool is_valid() const { return uplifting_i != invalid_data; }
    static auto invalid() { return ConstraintSelection(); }
    friend auto operator<=>(const ConstraintSelection &, const ConstraintSelection &) = default;
  };

  // Concept for a constraint on metameric behavior used throughout the application's
  // spectral uplifting pipeline. Constraints can exist on their own, or together
  // with other, secondary constraints.
  template <typename Ty>
  concept is_metameric_constraint = requires(Ty t, Scene scene, Uplifting uplifting, uint csys_i, uint seed, uint samples) {
    // The constraint does or does not allow for configuration 
    // through metameric mismatch editing in its current state
    { t.has_mismatching() } -> std::same_as<bool>;

    // The constraint allows for realizing a metamer (and attached color under uplifting's color system)
    { t.realize(scene, uplifting) } -> std::same_as<std::pair<Colr, Spec>>;

    // The constraint allows for realizing mismatch volume points, if has_mismatching() is true
    { t.realize_mismatching(scene, uplifting, csys_i, seed, samples) } -> std::same_as<std::vector<Colr>>;
  };

  template <typename Ty>
  concept is_vector_constraint = requires(Ty t) {
    { t.size()  } -> std::same_as<size_t>;
    { t.empty() } -> std::same_as<bool>;
  };

  // Small helper struct for constraints under direct color illumination;
  // used by implementations of is_colr_constraint
  struct ColrConstraint {
    uint cmfs_j = 0,   // Index of observer function
         illm_j = 0;   // Index of illuminant function
    Colr colr_j = 0.f; // Color under direct color system

    bool operator==(const ColrConstraint &o) const;
  };
  
  // Small helper struct for constraints under a system of light transport;
  // used by IndirectSurfaceConstraint in particular
  struct IndirectConstraint {
    uint              cmfs_i = 0,   // Index of (direct) observer
                      illm_i = 0;   // Index of (direct) illuminant
    Colr              colr_i = 0.f; // Color under direct color system

    uint              cmfs_j = 0;   // Index of (scene) observer
    std::vector<Spec> pwrs_j = { }; // Scene interreflection data
    Colr              colr_j = 0.0; // Color under indirect color system

    bool operator==(const IndirectConstraint &o) const;
  };

  // Concept defining the expected components of color-system constraints
  template <typename Ty>
  concept is_colr_constraint = requires(Ty t) {
    // The constraint specifies base color data, which must be reproduced
    // under the uplifting's primary color system
    { t.colr_i } -> std::same_as<Colr &>;

    // The constraint specifies secondary color data, under secondary
    // color systems assembled from scene data
    { t.cstr_j } -> std::same_as<std::vector<ColrConstraint> &>;
  } && is_metameric_constraint<Ty>;

  // Concept for a constraint on metameric behavior, which bases itself on data
  // sampled from a surface position in the scene.
  template <typename Ty>
  concept is_surface_constraint = requires(Ty t) {
    // The constraint specifies surface data, sampled from the scene.
    { t.surface } -> std::same_as<SurfaceInfo &>;

    // The surface data can be evaluated for its validity.
    { t.has_surface() } -> std::same_as<bool>;
  } && is_metameric_constraint<Ty>;

  template <typename Ty>
  concept is_multi_surface_constraint = requires (Ty t) {
    // The constraint specifies surface data, sampled from the scene.
    { t.surfaces } -> std::same_as<std::vector<SurfaceInfo> &>;

    // The surface data can be evaluated for its validity.
    { t.has_surface() } -> std::same_as<bool>;
  } && is_metameric_constraint<Ty>;

  // Constraint imposing reproduction of a specific spectral reflectance.
  struct MeasurementConstraint {
    // Measured spectral data
    Spec measure = 0.5;

  public:
    constexpr bool has_mismatching() const { return false; }
    bool operator==(const MeasurementConstraint &o) const;
    
  public:
    std::pair<Colr, Spec> realize(const Scene &scene, const Uplifting &uplifting) const;
    std::vector<Colr> realize_mismatching(const Scene &scene, const Uplifting &uplifting, uint csys_i, uint seed, uint samples) const;
  };
  static_assert(is_metameric_constraint<MeasurementConstraint>);

  // Constraint imposing specific color reproduction under a set of known
  // color systems, i.e. direct illumination.
  struct DirectColorConstraint {
    // Constraint data for direct color with sensible defaults
    Colr                        colr_i = 0.0; // Expected base color
    std::vector<ColrConstraint> cstr_j = { }; // Secondary constraints for color reproduction

  public:
    bool has_mismatching() const;
    bool operator==(const DirectColorConstraint &o) const;
    
  public:
    std::pair<Colr, Spec> realize(const Scene &scene, const Uplifting &uplifting) const;
    std::vector<Colr> realize_mismatching(const Scene &scene, const Uplifting &uplifting, uint csys_i, uint seed, uint samples) const;
  };
  static_assert(is_colr_constraint<DirectColorConstraint>);

  // Constraint imposing specific color reproduction under a set of known
  // color systems, i.e. direct illumination. The base color is sampled
  // from a scene surface.
  struct DirectSurfaceConstraint {
    // Constraint data for direct color with sensible defaults
    Colr                        colr_i = 0.0; // Expected base color, obtained from underlying surface
    std::vector<ColrConstraint> cstr_j = { }; // Secondary constraints for color reproduction

    // Surface data recorded through user interaction
    SurfaceInfo surface = SurfaceInfo::invalid();

  public:
    bool has_mismatching() const;
    bool has_surface() const { return surface.is_valid() && surface.record.is_object(); }
    bool operator==(const DirectSurfaceConstraint &o) const;
    
  public:
    std::pair<Colr, Spec> realize(const Scene &scene, const Uplifting &uplifting) const;
    std::vector<Colr> realize_mismatching(const Scene &scene, const Uplifting &uplifting, uint csys_i, uint seed, uint samples) const;
  };
  static_assert(is_surface_constraint<DirectSurfaceConstraint>);
  static_assert(is_colr_constraint<DirectSurfaceConstraint>);

  // Constraint imposing specific color reproduction under a known illuminant,
  // accounting for interreflections. The interreflection system is based on
  // measured light transport data from a scene surface.
  struct IndirectSurfaceConstraint {
    // Constraint data with sensible defaults
    std::vector<IndirectConstraint> cstr_j   = { }; // Individual interreflection constraints
    std::vector<SurfaceInfo>        surfaces = { }; // Attached surface data
    
    // Components of power series for solving, initially recorded 
    // at time of constraint building and used for metamer generation w.r.t. scene light transport
    std::vector<Spec> powers = { };
    
    // Requested output color, initially recorded at time of constraint building
    // and then moddified by the user through constraint editing
    Colr colr;

    // Surface data recorded through user interaction
    SurfaceInfo surface = SurfaceInfo::invalid();

  public:
    bool has_mismatching() const;
    bool has_surface() const { return surface.is_valid() && surface.record.is_object(); }
    bool operator==(const IndirectSurfaceConstraint &o) const;

  public:
    std::pair<Colr, Spec> realize(const Scene &scene, const Uplifting &uplifting) const;
    std::vector<Colr> realize_mismatching(const Scene &scene, const Uplifting &uplifting, uint csys_i, uint seed, uint samples) const;
  };
  static_assert(is_surface_constraint<IndirectSurfaceConstraint>);

  // JSON (de)serialization of constraint variants
  void from_json(const json &js, DirectColorConstraint &c);
  void to_json(json &js, const DirectColorConstraint &c);
  void from_json(const json &js, ColrConstraint &c);
  void to_json(json &js, const ColrConstraint &c);
  void from_json(const json &js, IndirectConstraint &c);
  void to_json(json &js, const IndirectConstraint &c);
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