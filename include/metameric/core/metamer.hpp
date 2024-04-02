#pragma once

#include <metameric/core/distribution.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/tree.hpp>

namespace met {
  // Argument struct and method for generating a spectral reflectance, given one or more
  // known color signals in corresponding color systems
  struct DirectSpectrumInfo {
    using DirectConstraint = std::pair<ColrSystem, Colr>;

  public:
    std::vector<DirectConstraint> direct_constraints = { }; // Direct metamerism constraints
    const Basis &basis;                                     // Spectral basis functions
  };
  Spec generate_spectrum(DirectSpectrumInfo info);

  // Argument struct and method for generating a spectral reflectance, given a system of
  // interreflections expressed as a truncated power series
  struct IndrctSpectrumInfo {
    using DirectConstraint = std::pair<ColrSystem, Colr>;
    using IndrctConstraint = std::pair<IndirectColrSystem, Colr>;

  public:
    std::vector<DirectConstraint> direct_constraints   = { }; // Direct metamerism constraints
    std::vector<IndrctConstraint> indirect_constraints = { }; // Indirect metamerism constraints
    const Basis &basis;                                       // Spectral basis functions
  };
  Spec generate_spectrum(IndrctSpectrumInfo info);

  // Argument struct and method for generating points on the object color solid of a color system,
  // following the method of Mackiewicz et al., 2019 
  // "Spherical sampling methods for the calculation of metamer mismatch volumes"
  struct DirectColorSystemOCSInfo {
    ColrSystem  direct_objective; // Color system that builds objective function

    const Basis &basis;  // Spectral basis functions
    uint seed      = 4;  // Seed for (pcg) sampler state
    uint n_samples = 32; // Nr. of samples to solve for
  };
  std::vector<Spec> generate_color_system_ocs(const DirectColorSystemOCSInfo &info);

  // Argument struct and method for generating points on the object color solid of a metameric
  // mismatching between two or more color systems, following the method of Mackiewicz et al., 2019 
  // "Spherical sampling methods for the calculation of metamer mismatch volumes"
  struct DirectMismatchingOCSInfo {
    using DirectConstraint = std::pair<ColrSystem, Colr>;

  public:
    std::array<ColrSystem, 2>     direct_objectives  = { }; // Direct objective function in two parts
    std::vector<DirectConstraint> direct_constraints = { }; // Direct metamerism constraints

    const Basis &basis;  // Spectral basis functions
    uint seed      = 4;  // Seed for (pcg) sampler state
    uint n_samples = 32; // Nr. of samples to solve for
  };
  std::vector<Spec> generate_mismatching_ocs(const DirectMismatchingOCSInfo &info);

  // Argument struct and method for generating points on the object color solid of a metameric
  // mismatching between signal in a number of base color systems, and a interreflection system
  // expressed as a truncated power series
  struct IndirectMismatchingOCSInfo {
    using DirectConstraint = std::pair<ColrSystem,         Colr>;
    using IndrctConstraint = std::pair<IndirectColrSystem, Colr>;
      
  public:
    ColrSystem                    direct_objective;           // Direct part of the objective function
    IndirectColrSystem            indirect_objective;         // Indirect part of the objective function
    std::vector<DirectConstraint> direct_constraints   = { }; // Direct metamerism constraints
    std::vector<IndrctConstraint> indirect_constraints = { }; // Indirect metamerism constraints

    const Basis &basis;  // Spectral basis functions
    uint seed      = 4;  // Seed for (pcg) sampler state
    uint n_samples = 32; // Nr. of samples to solve for
  };
  std::vector<Spec> generate_mismatching_ocs(const IndirectMismatchingOCSInfo &info);
} // namespace met