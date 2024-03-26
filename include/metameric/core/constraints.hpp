#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/record.hpp>

namespace met {
  // Concept defining the expected components of color-system constraints
  template <typename Ty>
  concept ColorConstraint = requires(Ty t) {
    { t.get_colr_i() } -> std::same_as<Colr &>;
    { t.colr_j       } -> std::same_as<std::vector<Colr> &>;
    { t.csys_j       } -> std::same_as<std::vector<uint> &>;
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
    // Constraint data for direct color with sensible defaults
    Colr              colr_i = 0.5; // Expected color under uplifting's color system 
    std::vector<Colr> colr_j = { }; // Expected colors under secondary color systems
    std::vector<uint> csys_j = { }; // Indices of the secondary color systems

  public:
    bool has_mismatching() const { return !colr_j.empty(); }
    bool operator==(const DirectColorConstraint &o) const;

  public:
          Colr &get_colr_i()       { return colr_i; }
    const Colr &get_colr_i() const { return colr_i; }
  };
  static_assert(is_color_constraint<DirectColorConstraint>);

  /* Constraint definition used in uplifting;
     A direct surface constraint imposes specific color reproduction
     for a position on a scene surface, under a specified color system,
     given direct illumination. */
  struct DirectSurfaceConstraint {
    // Constraint data for direct color
    // Note: colr_i as in DirectConstraint is sampled from the underlying surface;
    // alternatively, see get_colr_i()
    std::vector<Colr> colr_j = { }; // Expected colors under secondary color systems
    std::vector<uint> csys_j = { }; // Indices of the secondary color systems

    // Surface data recorded through user interaction
    SurfaceInfo surface = SurfaceInfo::invalid();

  public:
    bool has_mismatching() const { return !colr_j.empty(); }
    bool is_valid() const { return surface.is_valid() && surface.record.is_object(); }
    bool operator==(const DirectSurfaceConstraint &o) const;

  public:
          Colr &get_colr_i()       { return surface.diffuse; }
    const Colr &get_colr_i() const { return surface.diffuse; }
  };
  static_assert(is_surface_constraint<DirectSurfaceConstraint>);
  static_assert(is_color_constraint<DirectSurfaceConstraint>);

  /* Constraint definition used in uplifting
     A indirect surface constraint imposes specific color constraints
     for several positions on scene surfaces, taking into account light transport
     that affects each individual surface position. The reason multiple constraints
     are used is that the underlying constraint vertex, practically, shows up
     throughout a scene.
  */
  struct _IndirectSurfaceConstraint {
    struct Constraint {
      SurfaceInfo        surface = SurfaceInfo::invalid(); // Underlying surface data, recorded by constraint position in scene
      IndirectColrSystem csys;                             // Color system based on light transport exitant from surface position
      Colr               colr;                             // Constrained output color, user-specified inside a mismatch volume

    public:
      bool operator==(const Constraint &o) const;
      bool is_valid() const;
      bool has_mismatching() const;
    };

    // The set of scene constraints bound together into a single constraint vertex
    std::vector<Constraint> constraints;

  public:
    bool operator==(const _IndirectSurfaceConstraint &o) const;
    bool is_valid() const;
    bool has_mismatching() const;
  };

  /* Constraint definition used in uplifting
     A indirect surface constraint imposes specific color reproduction
     for a position on a scene surface, taking into account light transport
     affecting this surface position. */
  struct IndirectSurfaceConstraint {
    // Surface data recorded through user interaction
    SurfaceInfo surface = SurfaceInfo::invalid();
    
    // Components of power series for solving, initially recorded 
    // at time of constraint building and used for metamer generation w.r.t. scene light transport
    std::vector<Spec> powers = { };
    
    // Requested output color, initially recorded at time of constraint building
    // and then moddified by the user through constraint editing
    Colr colr;

  public:
    bool has_mismatching() const { return !powers.empty() && !colr.isZero(); }
    bool is_valid() const { return surface.is_valid() && surface.record.is_object(); }
    bool operator==(const IndirectSurfaceConstraint &o) const;
    
  public:
          Colr &get_colr_i()       { return surface.diffuse; }
    const Colr &get_colr_i() const { return surface.diffuse; }
  };
  static_assert(is_surface_constraint<IndirectSurfaceConstraint>);

  /* Constraint definition used in uplifting;
     A measurement constraint imposes specific spectrum reproduction
     for some given spectra, for at the least the corresponding color
     in the uplifting's primary color system. */
  struct MeasurementConstraint {
    // Measured spectral data
    Spec measurement = 0.5;

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