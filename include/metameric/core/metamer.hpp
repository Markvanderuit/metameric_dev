#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>

namespace met {
  constexpr static uint wavelength_bases  = 12;
  constexpr static uint wavelength_blacks = wavelength_bases - 3;

  using BBasis = eig::Matrix<float, wavelength_samples, wavelength_bases>;
  using BBlack = eig::Matrix<float, wavelength_bases, wavelength_blacks>;
  using BCMFS  = eig::Matrix<float, wavelength_bases, 3>;
  using BSpec  = eig::Matrix<float, wavelength_bases, 1>;
  using WSpec  = eig::Matrix<float, barycentric_weights, 1>;

  struct GenerateSpectrumInfo {
    BBasis                 &basis;   // Spectral basis functions
    std::span<const CMFS> systems;  // Color systems in which signals are available
    std::span<const Colr> signals;  // Signal samples in their respective color systems
    bool impose_boundedness = true; // Impose boundedness cosntraints
  };

  Spec generate_spectrum(GenerateSpectrumInfo info);

  Spec generate(const BBasis         &basis,
                std::span<const CMFS> systems,
                std::span<const Colr> signals);
  
  struct GenerateOCSBoundaryInfo {
    BBasis                &basis;  // Spectral basis functions
    CMFS                   system; // Color system spectra describing the expected gamut
    std::span<const Colr> samples; // Random unit vector samples in 3 dimensions
  };

  std::vector<Colr> generate_ocs_boundary(const GenerateOCSBoundaryInfo &info);

  std::vector<Colr> generate_boundary_i(const BBasis &basis,
                                       std::span<const CMFS> systems_i,
                                       std::span<const Colr> signals_i,
                                       const CMFS &system_j,
                                       std::span<const eig::ArrayXf> samples);

  using Wght = std::vector<float>;

  /* Info struct for simplified, unbounded generation of a gamut, given spectral information */
  struct GenerateGamutSimpleInfo {
    uint               bary_weights;
    std::vector<WSpec> weights; // Approximate barycentric coordinates inside the expected gamut
    std::vector<Colr>  samples; // Sample colors inside the expected gamut
  };

  /* Info struct for generation of a gamut, given spectral information */
  struct GenerateGamutSpectrumInfo {
    BBasis            &basis;   // Spectral basis functions
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

    BBasis             &basis;   // Spectral basis functions
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