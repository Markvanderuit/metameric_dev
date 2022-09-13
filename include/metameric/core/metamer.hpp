#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>

namespace met {
  constexpr static uint wavelength_bases  = 12;
  constexpr static uint wavelength_blacks = wavelength_bases - 3;

  using BBasis = eig::Matrix<float, wavelength_samples, wavelength_bases>;
  using BBlack = eig::Matrix<float, wavelength_bases, wavelength_blacks>;
  using BCMFS  = eig::Matrix<float, wavelength_bases, 3>;
  using BSpec  = eig::Matrix<float, wavelength_bases, 1>;

  struct MetamerMapping {
    // Spectrum->color mappings i and j, and requested signals for each
    SpectralMapping mapping_i;
    SpectralMapping mapping_j;

    // Basis functions for spectral fundamentals and metameric blacks
    BBasis basis_funcs;
    BBlack black_funcs;

    // Produce a spectral reflectance for the given data
    Spec generate(const Colr &color_i, const Colr &color_j);
    Spec generate(const std::vector<Colr> &constraints);
  };

  Spec generate(const BBasis         &basis,
                std::span<const CMFS> systems,
                std::span<const Colr> signals);
} // namespace met