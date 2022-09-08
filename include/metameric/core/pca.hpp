#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>

namespace met {
  using BMatrixType = eig::Matrix<float, wavelength_samples, wavelength_samples>;

  BMatrixType pca(const std::vector<Spec> &spectra);
  std::vector<Spec> pca_components(const std::vector<Spec> &spectra);
  
  eig::Matrix<float, 3, 16> orthogonal_complement(const CMFS &csys, const BMatrixType &bases);

  template <uint N>
  eig::Matrix<float, wavelength_samples, N> basis_complement(const CMFS &csys, const BMatrixType &mat);
} // namespace met