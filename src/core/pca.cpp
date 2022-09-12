#define EIGEN_NO_STATIC_ASSERT

#include <metameric/core/pca.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <algorithm>
#include <execution>
#include <numeric>
#include <Eigen/Sparse>

namespace met {
  
  BMatrixType covariance_matrix(const std::vector<Spec> &spectra) {
    // Operate on a copy in memory
    std::vector<Spec> buffer(range_iter(spectra));
    
    // Normalize data
    // std::transform(std::execution::par_unseq, range_iter(buffer), buffer.begin(),
    //   [](const auto &s) { return s.matrix().normalized().eval(); });

    // Calculate empirical mean
    Spec mean = std::reduce(std::execution::par_unseq, range_iter(buffer), Spec(0.f),
      [](const auto &a, const auto &b) { return (a + b).eval(); })
      / static_cast<float>(buffer.size());

    // Subtract empirical mean to center data
    std::transform(std::execution::par_unseq, range_iter(buffer), buffer.begin(),
      [&](const auto &s) { return (s - mean).eval(); });

    // Compute the covariance matrix
    eig::MatrixXf cov_matrix(buffer.size(), wavelength_samples);
    std::copy(range_iter(buffer), cov_matrix.rowwise().begin());
    cov_matrix = (cov_matrix.adjoint() * cov_matrix)
               / static_cast<float>(wavelength_samples - 1);
    
    return (BMatrixType() << cov_matrix).finished();
  }

  BMatrixType eigen_vectors(const BMatrixType &cov) {
    eig::SelfAdjointEigenSolver<BMatrixType> solver(cov);
    return solver.eigenvectors();
  }
  
  eig::Matrix<float, 13, 16> orthogonal_complement(const CMFS &csys,
                                                   const BMatrixType &bases) {
    constexpr uint K = 16;

    eig::Matrix<float, wavelength_samples, K> basis = bases.rightCols(K);
    eig::Matrix<float, 3, wavelength_samples> csys_ = csys.transpose();
    eig::Matrix<float, 3, K>                  A     = (csys_ * basis).eval();
    
    // Return null space of matrix
    return A.fullPivLu().kernel().transpose().eval();
  }
  
  template <uint N, uint M>
  eig::Matrix<float, N, N - M> orthogonal_complement(const eig::Matrix<float, N, M> &mat) {
    auto C = (mat.transpose().eval()).fullPivLu().kernel().transpose().eval();
    return (eig::Matrix<float, N, N - M>() << C).finished();
  }
  
  template 
  eig::Matrix<float, wavelength_bases, wavelength_bases - 3> 
  orthogonal_complement<wavelength_bases, 3>(const eig::Matrix<float, wavelength_bases, 3> &mat);
  
  /* template <uint Components, uint Remainder>
  eig::Matrix<float, Remainder, Components> orthogonal_complement(const CMFS &csys,
                                                                  const BMatrixType &mat) {
    eig::Matrix<float, wavelength_samples, N> basis = mat.rightCols(N);
    eig::Matrix<float, N, 3>                  A     = (csys.transpose() * basis).transpose().eval();
    
    // Find the orthogonal projection matrix
    auto a  = (A * (A.transpose() * A).inverse() * A.transpose()).eval();
    auto pa = (decltype(a)::Identity() - a).eval();


    auto Gamma  = (csys.transpose() * basis).eval();
    auto Gamma_ = eig::Matrix<float, wavelength_samples, N>::Identity() - (Gamma * (Gamma.transpose() * Gamma).inverse() * Gamma.transpose()).eval();

    return Gamma_;
  }

  template eig::Matrix<float, wavelength_samples, 16u>
  basis_complement<16u>(const CMFS &, const BMatrixType &); */
} // namespace met