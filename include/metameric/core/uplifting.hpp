#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/record.hpp>

namespace met {
  // Concept defining the expected components of color-system constraints
  template <typename Ty>
  concept ColorConstraint = requires(Ty t) {
    { t.colr_j } -> std::same_as<std::vector<Colr> &>;
    { t.csys_j } -> std::same_as<std::vector<uint> &>;
  };
  template <typename Ty>
  concept is_color_constraint = ColorConstraint<Ty>;

  // Concept defining the expected components of on-surface constraints 
  template <typename Ty>
  concept SurfaceConstraint = requires(Ty t) {
    { t.is_valid() } -> std::same_as<bool>;
    { t.surface    } -> std::same_as<SurfaceInfo &>;
  };
  template <typename Ty>
  concept is_surface_constraint = SurfaceConstraint<Ty>;

  /* Constraint definition used in uplifting;
     A direct constraint imposes specific color reproduction under a 
     specified color system, i.e. direct illumination. */
  struct DirectColorConstraint {
    // Constraint data for direct color
    Colr              colr_i; // Expected color under uplifting's color system 
    std::vector<Colr> colr_j; // Expected colors under secondary color systems
    std::vector<uint> csys_j; // Indices of the secondary color systems

  public:
    bool has_mismatching() const { return !colr_j.empty(); }
    bool operator==(const DirectColorConstraint &o) const;
  };
  static_assert(is_color_constraint<DirectColorConstraint>);

  /* Constraint definition used in uplifting;
     A direct surface constraint imposes specific color reproduction
     for a position on a scene surface, under a specified color system,
     given direct illumination. */
  struct DirectSurfaceConstraint {
    // Constraint data for direct color
    // Note: colr_i as in DirectConstraint is sampled from the underlying surface
    std::vector<Colr> colr_j; // Expected colors under secondary color systems
    std::vector<uint> csys_j; // Indices of the secondary color systems

    // Surface data recorded through user interaction
    SurfaceInfo surface = SurfaceInfo::invalid();

  public:
    bool has_mismatching() const { return !colr_j.empty(); }
    bool is_valid() const { return surface.is_valid() && surface.record.is_object(); }
    bool operator==(const DirectSurfaceConstraint &o) const;
  };
  static_assert(is_surface_constraint<DirectSurfaceConstraint>);
  static_assert(is_color_constraint<DirectSurfaceConstraint>);

  /* Constraint definition used in upliftin
     A indirect surface constraint imposes specific color reproduction
     for a position on a scene surface, taking into account light transport
     affecting this surface position. */
  struct IndirectSurfaceConstraint {
    // Surface data recorded through user interaction
    SurfaceInfo surface = SurfaceInfo::invalid();

    // ...

  public:
    // bool has_mismatching() const { return !colr_j.empty(); }
    bool is_valid() const { return surface.is_valid(); }
    bool operator==(const IndirectSurfaceConstraint &o) const;
  };
  static_assert(is_surface_constraint<IndirectSurfaceConstraint>);


  /* Constraint definition used in uplifting;
     A measurement constraint imposes specific spectrum reproduction
     for some given spectra, for at the least the corresponding color
     in the uplifting's primary color system. */
  struct MeasurementConstraint {
    // Measured spectral data
    Spec measurement; 

  public:
    bool operator==(const MeasurementConstraint &o) const;
  };

  // JSON (de)serialization of constraint variants
  void from_json(const json &js, DirectColorConstraint &c);
  void to_json(json &js, const DirectColorConstraint &c);
  void from_json(const json &js, MeasurementConstraint &c);
  void to_json(json &js, const MeasurementConstraint &c);
  void from_json(const json &js, DirectSurfaceConstraint &c);
  void to_json(json &js, const DirectSurfaceConstraint &c);
  void from_json(const json &js, IndirectSurfaceConstraint &c);
  void to_json(json &js, const IndirectSurfaceConstraint &c);
} // namespace met