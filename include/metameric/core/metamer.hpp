#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/tree.hpp>

namespace met {
  /* Info struct for generation of a spectral reflectance, given color signals and color systems */
  struct GenerateSpectrumInfo {
    const Basis          &basis;         // Spectral basis functions
    const Spec           &basis_mean;    // Average of spectral basis function data
    std::span<const CMFS> systems;       // Color systems in which signals are available
    std::span<const Colr> signals;       // Signal samples in their respective color systems
    bool impose_boundedness = true;      // Impose boundedness cosntraints
    bool reduce_basis_count = false;     // After solve, re-attempt solve with reduced nr. of bases
    uint basis_count = wavelength_bases; // Starting nr. of bases
  };
  
  /* Info struct for sampling-based generation of points on the object color solid of a color system */
  struct GenerateOCSBoundaryInfo {
    const Basis          &basis;  // Spectral basis functions
    const Spec           &basis_avg;
    CMFS                  system; // Color system spectra describing the expected gamut
    std::span<const Colr> samples; // Random unit vector samples in 3 dimensions
  };

  /* Info struct for sampling-based generation of points on the object color solid of a metamer mismatch volume */
  struct GenerateMismatchBoundaryInfo {
    const Basis                  &basis;     // Spectral basis functions
    const Spec                   &basis_avg;
    std::span<const CMFS>         systems_i; // Color system spectra for prior color signals
    std::span<const Colr>         signals_i; // Color signals for prior constraints
    const CMFS                   &system_j;  // Color system for mismatching region
    std::span<const eig::ArrayXf> samples;   // Random unit vector samples in (systems_i.size() + 1) * 3 dimensions
  };

  // Corresponding functions to above generate objects
  Spec generate_spectrum(GenerateSpectrumInfo info);
  std::vector<Colr> generate_ocs_boundary(const GenerateOCSBoundaryInfo &info);
  std::vector<Colr> generate_mismatch_boundary(const GenerateMismatchBoundaryInfo &info);

  /* Info struct for generation of a gamut, given color constraint information */
  struct GenerateGamutInfo {
    struct Signal {
      Colr colr_v; // Color signal
      Bary bary_v; // Approximate barycentric coords. of the signal in the expected gamut
      uint syst_i; // Color system index for this given color signal
    };

    const Basis        &basis;   // Spectral basis functions
    const Spec         &basis_avg;
    std::vector<Colr>   gamut;   // Known gamut
    std::vector<CMFS>   systems; // Color systems
    std::vector<Signal> signals; // Color signals and corresponding information
  };

  // Generate a gamut solution using a linear programming problem; see GenerateGamutInfo 
  // above for necessary information. Note: returns n=mvc_weights spectra; 
  // the last (padded) spectra should be ignored
  std::vector<Spec> generate_gamut(const GenerateGamutInfo &info);
} // namespace met