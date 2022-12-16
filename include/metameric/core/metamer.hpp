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

  Spec generate(const BBasis         &basis,
                std::span<const CMFS> systems,
                std::span<const Colr> signals);

  std::vector<Colr> generate_boundary(const BBasis                 &basis,
                                      const CMFS                   &system_i,
                                      const Colr                   &signal_i,
                                      std::span<const CMFS>         systems_j,
                                      std::span<const Colr>         signals_j,
                                      std::span<const eig::ArrayXf> samples);

  std::vector<Colr> generate_boundary(const BBasis &basis,
                                      const CMFS   &system_i,
                                      const CMFS   &system_j,
                                      const Colr   &signal_i,
                                      const std::vector<eig::Array<float, 6, 1>> &samples);
  using Wght = std::vector<float>;
  std::vector<Colr> generate_gamut(const std::vector<Wght> &weights,
                                   const std::vector<Colr> &samples);
} // namespace met