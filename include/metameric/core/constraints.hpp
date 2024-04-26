#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/record.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/detail/scene_fwd.hpp>
#include <utility>

namespace met {
  using MetamerSample = std::pair<Spec, Basis::vec_type>;

  // Small helper struct for implementations of is_colr_constraint
  struct ColrConstraint {
    uint cmfs_j = 0, illm_j = 0; // Index of observer and illuminant functions in scene data
    Colr colr_j = 0.f;           // Color under this direct color system

    bool operator==(const ColrConstraint &o) const;
    bool is_similar(const ColrConstraint &o) const;
  };

  // Concept for a constraint on metameric behavior used throughout the application's
  // spectral uplifting pipeline. Constraints can exist on their own, or together
  // with other, secondary constraints.
  template <typename Ty>
  concept is_metameric_constraint = requires(Ty t, Scene scene, Uplifting uplifting, uint csys_i, uint seed, uint samples) {
    // The constraint has a specific, primary color which forms a vertex position in the uplifting
    { t.position(scene, uplifting) } -> std::same_as<Colr>;

    // The constraint allows for realizing a metamer (and attached color under uplifting's color system)
    { t.realize(scene, uplifting) } -> std::same_as<std::pair<Spec, Basis::vec_type>>;

    // The constraint does or does not allow for configuration 
    // through metameric mismatch editing in its current state
    { t.has_mismatching(scene, uplifting) } -> std::same_as<bool>;

    // The constraint allows for realizing mismatch volume sample points, if has_mismatching() is true
    { t.realize_mismatching(scene, uplifting, csys_i, seed, samples) } -> std::same_as<std::vector<std::tuple<Colr, Spec, Basis::vec_type>>>;
  };

  // Concept defining the expected components of color-system constraints
  template <typename Ty>
  concept is_colr_constraint = is_metameric_constraint<Ty> && requires(Ty t) {
    // The constraint specifies base color data, which must be reproduced
    // under the uplifting's primary color system
    { t.colr_i } -> std::same_as<Colr &>;

    // The constraint specifies secondary color data, under secondary
    // color systems assembled from scene data
    { t.cstr_j } -> std::same_as<std::vector<ColrConstraint> &>;
  };

  // Concept for a constraint on metameric behavior, which bases itself on data
  // sampled from a surface position in the scene.
  template <typename Ty>
  concept is_surface_constraint = is_metameric_constraint<Ty> && requires(Ty t) {
    // The constraint specifies surface data, sampled from the scene.
    { t.surface } -> std::same_as<SurfaceInfo &>;

    // The surface data can be evaluated for its validity.
    { t.has_surface() } -> std::same_as<bool>;
  };

  // Constraint imposing reproduction of a specific spectral reflectance.
  struct MeasurementConstraint {
    Spec measure = 0.5; // Measured spectral data
    
  public:
    // Determine the constraint's position in the spectral uplifting tesselation based on the measure
    Colr position(const Scene &scene, const Uplifting &uplifting) const;

    // Simply return the constraint's measure
    std::pair<Spec, Basis::vec_type> realize(const Scene &scene, const Uplifting &uplifting) const;

    // Generate points on the constraint's metamer mismatching volume
    std::vector<std::tuple<Colr, Spec, Basis::vec_type>> realize_mismatching(const Scene &scene, const Uplifting &uplifting, uint csys_i, uint seed, uint samples) const {
      return { };
    }

  public:
    // The constraint data is in a invalid state for metameric mismatching
    bool has_mismatching(const Scene &scene, const Uplifting &uplifting) const {
      return false;
    }

    // The constraint always produces the same mismatching; none
    bool equal_mismatching(const Scene &scene, const Uplifting &uplifting, const MeasurementConstraint &o, uint csys_i) {
      return true;
    }

  public:
    bool operator==(const MeasurementConstraint &o) const;
  };
  static_assert(is_metameric_constraint<MeasurementConstraint>);

  // Constraint imposing specific color reproduction under a set of known
  // color systems, i.e. direct illumination.
  struct DirectColorConstraint {
    Colr                        colr_i = 0.0; // Expected base color
    std::vector<ColrConstraint> cstr_j = { }; // Secondary constraints for color reproduction
    
  public:
    // Obtain the constraint's position in the spectral uplifting tesselation
    Colr position(const Scene &scene, const Uplifting &uplifting) const { 
      return colr_i;
    }

    // Solve for the constraint's metamer based on its current configuration
    std::pair<Spec, Basis::vec_type> realize(const Scene &scene, const Uplifting &uplifting) const;

    // Generate points on the constraint's metamer mismatching volume
    std::vector<std::tuple<Colr, Spec, Basis::vec_type>> realize_mismatching(const Scene &scene, const Uplifting &uplifting, uint csys_i, uint seed, uint samples) const;

  public:
    // The constraint data is in a valid state for metameric mismatching
    bool has_mismatching(const Scene &scene, const Uplifting &uplifting) const;
    
  public:
    bool operator==(const DirectColorConstraint &o) const;
  };
  static_assert(is_colr_constraint<DirectColorConstraint>);

  // Constraint imposing specific color reproduction under a set of known
  // color systems, i.e. direct illumination. The base color is sampled
  // from a scene surface.
  struct DirectSurfaceConstraint {
    Colr                        colr_i = 0.0; // Expected base color, obtained from underlying surface
    std::vector<ColrConstraint> cstr_j = { }; // Secondary constraints for color reproduction

    // Surface data recorded through user interaction
    SurfaceInfo surface = SurfaceInfo::invalid();
    
  public:
    // The underlying surface is in a valid state
    bool has_surface() const { return surface.is_valid() && surface.record.is_object(); }

    // The constraint data is in a valid state for metameric mismatching
    bool has_mismatching(const Scene &scene, const Uplifting &uplifting) const;
    
  public:
    // Obtain the constraint's position in the spectral uplifting tesselation
    Colr position(const Scene &scene, const Uplifting &uplifting) const {
      met_trace();
      return colr_i;
    }

    // Solve for the constraint's metamer based on its current configuration
    std::pair<Spec, Basis::vec_type> realize(const Scene &scene, const Uplifting &uplifting) const;

    // Generate points on the constraint's metamer mismatching volume
    std::vector<std::tuple<Colr, Spec, Basis::vec_type>> realize_mismatching(const Scene &scene, const Uplifting &uplifting, uint csys_i, uint seed, uint samples) const;

  public:
    bool operator==(const DirectSurfaceConstraint &o) const;
  };
  static_assert(is_surface_constraint<DirectSurfaceConstraint> && is_colr_constraint<DirectSurfaceConstraint>);

  // Constraint imposing specific color reproduction under a known illuminant,
  // accounting for interreflections. The interreflection system is based on
  // measured light transport data from a scene surface.
  struct IndirectSurfaceConstraint {
    // Components of power series for solving, initially recorded 
    // at time of constraint building and used for metamer generation w.r.t. scene light transport
    std::vector<Spec> powers = { };
    
    // Requested output color, initially recorded at time of constraint building
    // and then moddified by the user through constraint editing
    Colr colr;

    // Surface data recorded through user interaction
    SurfaceInfo surface = SurfaceInfo::invalid();
    
  public:
    // The underlying surface is in a valid state
    bool has_surface() const { return surface.is_valid() && surface.record.is_object(); }

    // The constraint data is in a valid state for metameric mismatching
    bool has_mismatching(const Scene &scene, const Uplifting &uplifting) const;
    
  public:
    // Obtain the constraint's position in the spectral uplifting tesselation
    Colr position(const Scene &scene, const Uplifting &uplifting) const {
      met_trace();
      return surface.diffuse;
    }

    // Solve for the constraint's metamer based on its current configuration
    std::pair<Spec, Basis::vec_type> realize(const Scene &scene, const Uplifting &uplifting) const;

    // Generate points on the constraint's metamer mismatching volume
    std::vector<std::tuple<Colr, Spec, Basis::vec_type>> realize_mismatching(const Scene &scene, const Uplifting &uplifting, uint csys_i, uint seed, uint samples) const;

  public:
    bool operator==(const IndirectSurfaceConstraint &o) const;
  };
  static_assert(is_surface_constraint<IndirectSurfaceConstraint>);

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