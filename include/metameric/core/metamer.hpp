#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>

namespace met {
  /* Info struct for generation of a spectral reflectance, given color signals and color systems */
  struct GenerateSpectrumInfo {
    Basis                 &basis;  // Spectral basis functions
    std::span<const CMFS> systems;  // Color systems in which signals are available
    std::span<const Colr> signals;  // Signal samples in their respective color systems
    bool impose_boundedness = true; // Impose boundedness cosntraints
  };
  
  /* Info struct for sampling-based generation of points on the object color solid of a color system */
  struct GenerateOCSBoundaryInfo {
    Basis                &basis;  // Spectral basis functions
    CMFS                   system; // Color system spectra describing the expected gamut
    std::span<const Colr> samples; // Random unit vector samples in 3 dimensions
  };

  /* Info struct for sampling-based generation of points on the object color solid of a metamer mismatch volume */
  struct GenerateMismatchBoundaryInfo {
    Basis                        &basis;     // Spectral basis functions
    std::span<const CMFS>          systems_i; // Color system spectra for prior color signals
    std::span<const Colr>          signals_i; // Color signals for prior constraints
    const CMFS                    &system_j;  // Color system for mismatching region
    std::span<const eig::ArrayXf>  samples;   // Random unit vector samples in (systems_i.size() + 1) * 3 dimensions
  };

  // Corresponding functions to above generate objects
  Spec generate_spectrum(GenerateSpectrumInfo info);
  std::vector<Colr> generate_ocs_boundary(const GenerateOCSBoundaryInfo &info);
  std::vector<Colr> generate_mismatch_boundary(const GenerateMismatchBoundaryInfo &info);

  /* Info struct for simplified, unbounded generation of a gamut, given spectral information */
  struct GenerateGamutSimpleInfo {
    uint               bary_weights;
    std::vector<WSpec> weights; // Approximate barycentric coordinates inside the expected gamut
    std::vector<Colr>  samples; // Sample colors inside the expected gamut
  };

  /* Info struct for generation of a gamut, given spectral information */
  struct GenerateGamutSpectrumInfo {
    Basis            &basis;   // Spectral basis functions
    CMFS               system;  // Color system spectra describing the expected gamut
    std::vector<Colr>  gamut;   // Approximate color coordinates of the expected gamut
    std::vector<WSpec> weights; // Approximate barycentric coordinates inside the expected gamut
    std::vector<Spec>  samples; // Sample spectral distributions in the expected gamut
  };

  /* Info struct for generation of a gamut, given color constraint information */
  struct GenerateGamutConstraintInfo {
    struct Signal {
      Colr  colr_v; // Color signal
      WSpec bary_v; // Approximate barycentric coords. of the signal in the expected gamut
      uint  syst_i; // Color system index for this given color signal
    };

    Basis             &basis;   // Spectral basis functions
    std::vector<Colr>   gamut;   // Known gamut
    std::vector<CMFS>   systems; // Color systems
    std::vector<Signal> signals; // Color signals and corresponding information
  };

  // Generate a gamut solution using a linear programming problem; ee GenerateGamut*Info 
  // above for necessary information. Note: returns n=barycentric_weights spectra; 
  // the last (padded) spectra should be ignored
  std::vector<Colr> generate_gamut(const GenerateGamutSimpleInfo &info);
  std::vector<Spec> generate_gamut(const GenerateGamutSpectrumInfo &info);
  std::vector<Spec> generate_gamut(const GenerateGamutConstraintInfo &info);
} // namespace met