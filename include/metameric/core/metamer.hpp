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

    bool impose_boundedness = true;      // Impose [0, 1] boundedness cosntraints duringg solve
    bool solve_dual         = false;     // Define as a dual problem during solve
    bool reduce_basis_count = false;     // After solve, re-attempt solve with reduced nr. of bases

    uint basis_count = wavelength_bases; // Starting nr. of bases
  };
  
  /* Info struct for sampling-based generation of points on the object color solid of a color system */
  struct GenerateOCSBoundaryInfo {
    const Basis          &basis;      // Spectral basis functions
    CMFS                  system;     // Color system spectra describing the expected gamut
    std::span<const Colr> samples;    // Random unit vector samples in 3 dimensions
  };

  /* Info struct for sampling-based generation of points on the object color solid of a metamer mismatch volume */
  /* struct GenerateMMVBoundaryInfo {
    const Basis                  &basis;      // Spectral basis functions
    std::span<const CMFS>         systems_i;  // Color system spectra for prior color signals
    std::span<const Colr>         signals_i;  // Color signals for prior constraints
    const CMFS                   &system_j;   // Color system for mismatching region
    std::span<const eig::ArrayXf> samples;    // Random unit vector samples in (systems_i.size() + 1) * 3 dimensions
    std::optional<CMFS> system_j_override;    // TODO remove this absolute hack
  }; */
  struct GenerateMMVBoundaryInfo {
    const Basis                  &basis;      // Spectral basis functions
    std::span<const CMFS>         systems_i;  // Color system spectra for prior color signals
    std::span<const Colr>         signals_i;  // Color signals for prior constraints
    std::span<const CMFS>         systems_j;  // Color system spectra for objective function
    const CMFS                    &system_j;  // Color system for mismatching region
    std::span<const eig::ArrayXf> samples;    // Random unit vector samples in (systems_i.size() + 1) * 3 dimensions
  };

  // Corresponding functions to above generate objects
  Spec generate_spectrum(GenerateSpectrumInfo info);
  std::vector<Spec> generate_ocs_boundary_spec(const GenerateOCSBoundaryInfo &info);
  std::vector<Colr> generate_ocs_boundary_colr(const GenerateOCSBoundaryInfo &info);
  std::vector<Spec> generate_mmv_boundary_spec(const GenerateMMVBoundaryInfo &info);
  std::vector<Colr> generate_mmv_boundary_colr(const GenerateMMVBoundaryInfo &info);

  /* Info struct for generation of a gamut, given color constraint information */
  struct GenerateGamutInfo {
    struct Signal {
      Colr colr_v; // Color signal
      Bary bary_v; // Approximate barycentric coords. of the signal in the expected gamut
      uint syst_i; // Color system index for this given color signal
    };

    const Basis        &basis;      // Spectral basis functions
    std::vector<Colr>   gamut;      // Known gamut
    std::vector<CMFS>   systems;    // Color systems
    std::vector<Signal> signals;    // Color signals and corresponding information
  };

  // Generate a gamut solution using a linear programming problem; see GenerateGamutInfo 
  // above for necessary information. Note: returns n=generalized_weights spectra; 
  // the last (padded) spectra should be ignored
  /* std::vector<Spec> generate_gamut(const GenerateGamutInfo &info); */
} // namespace met