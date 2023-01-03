#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/spectrum.hpp>

namespace met {
  constexpr static uint wavelength_bases  = 12;
  constexpr static uint wavelength_blacks = wavelength_bases - 3;

  using BBasis = eig::Matrix<float, wavelength_samples, wavelength_bases>;
  using BBlack = eig::Matrix<float, wavelength_bases, wavelength_blacks>;
  using BCMFS  = eig::Matrix<float, wavelength_bases, 3>;
  using BSpec  = eig::Matrix<float, wavelength_bases, 1>;
  using WSpec  = eig::Matrix<float, barycentric_weights, 1>;

  Spec generate(const BBasis         &basis,
                std::span<const CMFS> systems,
                std::span<const Colr> signals);
  
  std::vector<Colr> generate_boundary_i(const BBasis &basis,
                                       std::span<const CMFS> systems_i,
                                       std::span<const Colr> signals_i,
                                       const CMFS &system_j,
                                       std::span<const eig::ArrayXf> samples);

  using Wght = std::vector<float>;

  /* Info struct for generation of a spectral gamut, given preliminary information */
  struct GenerateSpectralGamutInfo {
    BBasis             basis;   // Spectral basis functions
    CMFS               system;  // Color system spectra describing the expected gamut
    std::vector<Colr>  gamut;   // Approximate color coordinates of the expected gamut
    std::vector<WSpec> weights; // Approximate barycentric coordinates inside the expected gamut
    std::vector<Spec>  samples; // Sample spectral distributions in the expected gamut
  };
  std::vector<Spec> generate_gamut(const GenerateSpectralGamutInfo &info);

  /* Info struct for generation of a spectral gamut, given preliminary information */
  struct GenerateGamutInfo {
    struct Signal {
      Colr  colr_v; // Color signal
      WSpec bary_v; // Approximate barycentric coords. of the signal in the expected gamut
      uint  syst_i; // Color system index for this given color signal
    };

    BBasis              basis;   // Spectral basis functions
    std::vector<Colr>   gamut;   // Known gamut
    std::vector<CMFS>   systems; // Color systems
    std::vector<Signal> signals; // Color signals and corresponding information
  };

  // Generate a spectral gamut by solving a linear programming problem;
  // see GenerateGamutInfo for necessary information.
  // Note: returns barycentric_weights spectra; the last (padding) spectra should be ignored
  std::vector<Spec> generate_gamut(const GenerateGamutInfo &info);
} // namespace met