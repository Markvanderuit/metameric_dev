#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/spectrum.hpp>

namespace met {
  /* Constraint definition used in uplifting;
     A direct constraint imposes specific color reproduction under a 
     specified color system, i.e. direct illumination. */
  struct DirectColorConstraint {
    // Whether the constraint is used in the scene
    bool is_active = true;

    // Constraint data for direct color
    Colr              colr_i; // Expected color under uplifting's color system 
    std::vector<Colr> colr_j; // Expected colors under secondary color systems
    std::vector<uint> csys_j; // Indices of the secondary color systems

  public:
    bool operator==(const DirectColorConstraint &o) const;
  };

  /* Constraint definition used in uplifting;
     A measurement constraint imposes specific spectrum reproduction
     for some given spectra, for at the least the corresponding color
     in the uplifting's primary color system. */
  struct MeasurementConstraint {
    // Whether the constraint is used in the scene
    bool is_active = true;

    // Measured spectral data
    Spec measurement; 

  public:
    bool operator==(const MeasurementConstraint &o) const;
  };

  /* Constraint definition used in uplifting;
     A direct surface constraint imposes specific color reproduction
     for a position on a scene surface, under a specified color system,
     given direct illumination. */
  struct DirectSurfaceConstraint {
    // Whether the constraint is used in the scene
    bool is_active = true;

    // Constraint data for direct color
    Colr              colr_i; // Expected color under primary color system 
    std::vector<Colr> colr_j; // Expected colors under secondary color systems
    std::vector<uint> csys_j; // Indices of the secondary color systems

    // Surface data
    uint         object_i;         // Index of object to which constraint belongs
    uint         object_elem_i;    // Index of element where constraint is located on object
    eig::Array3f object_elem_bary; // Barycentric coordinates inside element

  public:
    bool operator==(const DirectSurfaceConstraint &o) const;
  };

  /* Constraint definition used in upliftin
     A indirect surface constraint imposes specific color reproduction
     for a position on a scene surface, taking into account light transport
     affecting this surface position. */
  struct IndirectSurfaceConstraint {
    // Whether the constraint is used in the scene
    bool is_active = true;

    // TODO ...

  public:
    bool operator==(const IndirectSurfaceConstraint &o) const;
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