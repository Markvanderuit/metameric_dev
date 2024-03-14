#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/tree.hpp>

namespace met {
  /* Info struct for generation of a spectral reflectance, given color signals and color systems */
  struct GenerateSpectrumInfo {
    const Basis           &basis;        // Spectral basis functions
    std::span<const CMFS> systems;       // Color systems in which signals are available
    std::span<const Colr> signals;       // Signal samples in their respective color systems
  };

  /* Info struct for generation of a spectral reflectance, given a system of interreflections
     expressed as a truncated power series. */
  struct GenerateIndirectSpectrumInfo {
    const Basis           &basis;        // Spectral basis functions
    CMFS                  base_system;  // Color system spectra for input texture prior
    Colr                  base_signal;  // Color signal for input texture prior
    std::span<const CMFS> refl_systems; // Components of the truncated interreflection power series
    Colr                  refl_signal;  // Expected color signal at camera
  };
  
  /* Info struct for sampling-based generation of points on the object color solid of a color system */
  struct GenerateOCSBoundaryInfo {
    const Basis          &basis;      // Spectral basis functions
    CMFS                  system;     // Color system spectra describing the expected gamut
    std::span<const Colr> samples;    // Random unit vector samples in 3 dimensions
  };

  /* Info struct for sampling-based generation of points on the object color solid of a metamer mismatch volume */
  struct GenerateMMVBoundaryInfo {
    const Basis                  &basis;      // Spectral basis functions
    std::span<const CMFS>         systems_i;  // Color system spectra for prior color signals
    std::span<const Colr>         signals_i;  // Color signals for prior constraints
    std::span<const CMFS>         systems_j;  // Color system spectra for objective function
    std::span<const eig::ArrayXf> samples;    // Random unit vector samples in (systems_i.size() + 1) * 3 dimensions
  };

  /* Info struct for sampling-based generation of points on the object color solid of a metamer mismatch volume
     which is the result of a system of interreflections, expressed as a truncated power series */
  struct GenerateIndirectMMVBoundaryInfo {
    const Basis                  &basis;      // Spectral basis functions
    std::span<const CMFS>         systems_i;  // Color system spectra for prior color signals
    std::span<const Colr>         signals_i;  // Color signals for prior constraints
    std::span<const Spec>         components; // Increasing components of the power series
    std::span<const CMFS>         systems_j;  // Color system spectra for objective function
    std::span<const eig::ArrayXf> samples;    // Random unit vector samples in (systems_i.size() + 1) * 3 dimensions
  };

  // Corresponding functions to above objects
  Spec generate_spectrum(GenerateSpectrumInfo info);
  Spec generate_spectrum(GenerateIndirectSpectrumInfo info);
  std::vector<Spec> generate_ocs_boundary_spec(const GenerateOCSBoundaryInfo &info);
  std::vector<Spec> generate_mmv_boundary_spec(const GenerateMMVBoundaryInfo &info);
  std::vector<Spec> generate_mmv_boundary_spec(const GenerateIndirectMMVBoundaryInfo &info);
} // namespace met