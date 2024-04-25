#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/convex.hpp>
#include <metameric/core/spectrum.hpp>

namespace met {
  // Argument struct and method for generating a spectral reflectance, given one or more
  // known color signals in corresponding color systems
  struct DirectSpectrumInfo {
    using DirectConstraint = std::pair<ColrSystem, Colr>;

  public:
    std::vector<DirectConstraint> direct_constraints = { }; // Direct metamerism constraints
    const Basis &basis;                                     // Spectral basis functions
  };
  Basis::vec_type generate_spectrum_coeffs(const DirectSpectrumInfo &info);

  // Argument struct and method for generating a spectral reflectance, given a system of
  // interreflections expressed as a truncated power series
  struct IndirectSpectrumInfo {
    using DirectConstraint   = std::pair<ColrSystem, Colr>;
    using IndirectConstraint = std::pair<IndirectColrSystem, Colr>;

  public:
    std::vector<DirectConstraint>   direct_constraints   = { }; // Direct metamerism constraints
    std::vector<IndirectConstraint> indirect_constraints = { }; // Indirect metamerism constraints
    const Basis &basis;                                         // Spectral basis functions
  };
  Basis::vec_type generate_spectrum_coeffs(const IndirectSpectrumInfo &info);

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
  std::vector<Basis::vec_type> generate_mismatching_ocs_coeffs(const DirectMismatchingOCSInfo &info);
  
  // Argument struct and method for generating points on the object color solid of a metameric
  // mismatching between signal in a number of base color systems, and a interreflection system
  // expressed as a truncated power series
  struct IndirectMismatchingOCSInfo {
    using DirectConstraint = std::pair<ColrSystem,         Colr>;
    using IndirectConstraint = std::pair<IndirectColrSystem, Colr>;
      
  public:
    ColrSystem                      direct_objective;           // Direct part of the objective function
    IndirectColrSystem              indirect_objective;         // Indirect part of the objective function
    std::vector<DirectConstraint>   direct_constraints   = { }; // Direct metamerism constraints
    std::vector<IndirectConstraint> indirect_constraints = { }; // Indirect metamerism constraints

    const Basis &basis;  // Spectral basis functions
    uint seed      = 4;  // Seed for (pcg) sampler state
    uint n_samples = 32; // Nr. of samples to solve for
  };
  std::vector<Basis::vec_type> generate_mismatching_ocs_coeffs(const IndirectMismatchingOCSInfo &info);

  // Argument struct and method for generating points on the object color solid of a color system,
  // following the method of Mackiewicz et al., 2019 
  // "Spherical sampling methods for the calculation of metamer mismatch volumes"
  struct DirectColorSystemOCSInfo {
    ColrSystem  direct_objective; // Color system that builds objective function

    const Basis &basis;  // Spectral basis functions
    uint seed      = 4;  // Seed for (pcg) sampler state
    uint n_samples = 32; // Nr. of samples to solve for
  };
  std::vector<Basis::vec_type> generate_color_system_ocs_coeffs(const DirectColorSystemOCSInfo &info);
  std::vector<Spec>            generate_color_system_ocs(const DirectColorSystemOCSInfo &info);

  // Argument struct and method for generating a closest representation in the basis
  // for a given spectral distribution.
  struct SpectrumCoeffsInfo {
    const Spec  &spec;  // Input spectrum to fit
    const Basis &basis; // Spectral basis functions
  };
  Basis::vec_type generate_spectrum_coeffs(const SpectrumCoeffsInfo &info);

  
  // Helpers; generate coefficients producing a spectrum in a basis, and return said spectrum
  // plus the coefficients 
  std::pair<Spec, Basis::vec_type> generate_spectrum(const auto &info) {
    met_trace();
    auto c = generate_spectrum_coeffs(info);
    return { info.basis(c), c };
  }
  std::vector<std::tuple<Colr, Spec, Basis::vec_type>> generate_mismatching_ocs(const DirectMismatchingOCSInfo &info);
  std::vector<std::tuple<Colr, Spec, Basis::vec_type>> generate_mismatching_ocs(const IndirectMismatchingOCSInfo &info);
} // namespace met