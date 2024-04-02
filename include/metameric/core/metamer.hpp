#pragma once

#include <metameric/core/distribution.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/tree.hpp>

namespace met {
  // Argument struct and method for generating a spectral reflectance, given one or more
  // known color signals in corresponding color systems
  struct GenerateSpectrumInfo {
    const Basis           &basis;  // Spectral basis functions used for generation
    std::span<const CMFS> systems; // Color systems in which signals are available
    std::span<const Colr> signals; // Signal samples in their respective color systems
  };
  Spec generate_spectrum(GenerateSpectrumInfo info);

  // Argument struct and method for generating a spectral reflectance, given a system of
  // interreflections expressed as a truncated power series
  struct GenerateIndirectSpectrumInfo {
    const Basis           &basis;       // Spectral basis functions used for generation
    CMFS                  base_system;  // Color system spectra for input texture prior
    Colr                  base_signal;  // Color signal for input texture prior
    std::span<const CMFS> refl_systems; // Components of the truncated interreflection power series
    Colr                  refl_signal;  // Expected color signal at camera
  };
  Spec generate_spectrum(GenerateIndirectSpectrumInfo info);

  // Argument struct and method for generating points on the object color solid of a color system,
  // following the method of Mackiewicz et al., 2019 
  // "Spherical sampling methods for the calculation of metamer mismatch volumes"
  struct GenerateColorSystemOCSInfo {
    const Basis          &basis;          // Spectral basis functions
    CMFS                  system;         // Color system describing the expected gamut
    uint                  seed      = 4;  // Seed for (pcg) sampler state
    uint                  n_samples = 32; // Nr. of samples to solve for
  };
  std::vector<Spec> generate_color_system_ocs(const GenerateColorSystemOCSInfo &info);

  // Argument struct and method for generating points on the object color solid of a metameric
  // mismatching between two or more color systems, following the method of Mackiewicz et al., 2019 
  // "Spherical sampling methods for the calculation of metamer mismatch volumes"
  struct GenerateMismatchingOCSInfo {
    const Basis           &basis;          // Spectral basis functions
    std::span<const CMFS>  systems_i;      // Color system spectra for prior color signals
    std::span<const Colr>  signals_i;      // Color signals for prior constraints
    std::span<const CMFS>  systems_j;      // Color system spectra for objective function
    uint                   seed      = 4;  // Seed for (pcg) sampler state
    uint                   n_samples = 32; // Nr. of samples to solve for
  };
  std::vector<Spec> generate_mismatching_ocs(const GenerateMismatchingOCSInfo &info);

  // Argument struct and method for generating points on the object color solid of a metameric
  // mismatching between signal in a number pf base color systems, and a interreflection system
  // expressed as a truncated power series
  struct GenerateIndirectMismatchingOCSInfo {
    const Basis           &basis;          // Spectral basis functions
    std::span<const CMFS>  systems_i;      // Color system spectra for prior color signals
    std::span<const Colr>  signals_i;      // Color signals for prior constraints
    std::span<const CMFS>  systems_j;      // Color system spectra for objective function
    uint                   seed      = 4;  // Seed for (pcg) sampler state
    uint                   n_samples = 32; // Nr. of samples to solve for
  };
  std::vector<Spec> generate_mismatching_ocs(const GenerateIndirectMismatchingOCSInfo &info);

  struct DirectConstraintInfo {
    // Direct color system and matching color
    ColrSystem direct;
    ColrSystem direct_colr;
  };

  struct IndirectConstraintInfo {
    // Direct color system and matching color
    ColrSystem direct;
    Colr       direct_colr;

    // Indirect color system and matching output
    IndirectColrSystem indirect;
    Colr               indirect_colr;
  };

  struct DirectMismatchingOCSInfo {
    const DirectConstraintInfo           &direct_objective;
    std::span<const DirectConstraintInfo> direct_constraints;

    uint seed      = 4;  // Seed for (pcg) sampler state
    uint n_samples = 32; // Nr. of samples to solve for
  };

  struct IndirectMismatchingOCSInfo {
    const IndirectConstraintInfo           &indirect_objective;
    std::span<const IndirectConstraintInfo> direct_constraints;
    std::span<const IndirectConstraintInfo> indirect_constraints;

    uint seed      = 4;  // Seed for (pcg) sampler state
    uint n_samples = 32; // Nr. of samples to solve for
  };
} // namespace met