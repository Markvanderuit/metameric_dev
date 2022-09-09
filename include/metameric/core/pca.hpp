#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>

namespace met {
  using BMatrixType = eig::Matrix<float, wavelength_samples, wavelength_samples>;
  using SMatrix     = eig::Matrix<float, wavelength_samples, wavelength_samples>;

  SMatrix covariance_matrix(const std::vector<Spec> &spectra);
  SMatrix eigen_vectors(const SMatrix &cov);
  
  template <uint Components>
  eig::Matrix<float, Components, wavelength_samples> eigen_vectors(const SMatrix &cov) {
    return eigen_vectors(cov).rightCols(Components);
  }

  // SMatrix pca(const std::vector<Spec> &spectra);
  // std::vector<Spec> pca_components(const std::vector<Spec> &spectra);
  
  eig::Matrix<float, 13, 16> orthogonal_complement(const CMFS &csys, const SMatrix &bases);

  template <uint N, uint M>
  eig::Matrix<float, N, N - M> orthogonal_complement(const eig::Matrix<float, N, M> &mat);
} // namespace met