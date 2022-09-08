#define EIGEN_NO_STATIC_ASSERT

#include <metameric/core/pca.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <algorithm>
#include <execution>
#include <numeric>
#include <Eigen/Sparse>

namespace met {
  BMatrixType pca(const std::vector<Spec> &spectra) {
    // Operate on a copy in memory
    std::vector<Spec> buffer(range_iter(spectra));
    
    // Normalize data
    std::transform(std::execution::par_unseq, range_iter(buffer), buffer.begin(),
      [](const auto &s) { return s.matrix().normalized().eval(); });

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

    // Obtain eigenvalues/vectors using Eigen's self adjoint solver
    // The eigenvectors are useful as basis functions, so we return those
    eig::SelfAdjointEigenSolver<eig::MatrixXf> solver(cov_matrix);
    return solver.eigenvectors();
  }

  std::vector<Spec> pca_components(const std::vector<Spec> &spectra) {
    BMatrixType components = pca(spectra);

    std::vector<Spec> separate_components(wavelength_samples);
    std::copy(range_iter(components.colwise()), separate_components.begin());
    std::reverse(range_iter(separate_components));

    return separate_components;
  }

  eig::Matrix<float, 3, 16> orthogonal_complement(const CMFS &csys,
                                                   const BMatrixType &bases) {
    constexpr uint N = 16;

    eig::Matrix<float, wavelength_samples, N> basis = bases.rightCols(N);
    eig::Matrix<float, N, 3>                  A     = (csys.transpose() * basis).eval();
    
    // auto a  = (A * (A.transpose() * A).inverse() * A.transpose()).eval();
    // auto pa = (decltype(a)::Identity() - a).eval();

    eig::MatrixXf nullspace = A.fullPivLu().kernel().eval();

    // eig::MatrixXf nullspace = A.fullPivLu().kernel().eval();
    fmt::print("Null space size: {} x {}\n", nullspace.rows(), nullspace.cols());
    fmt::print("Null space data: {}\n", nullspace.reshaped());

    return (eig::Matrix<float, 3, 16>() << nullspace.transpose().eval()).finished();

    // // Find the orthogonal projection matrix
    // auto I  = eig::Matrix<float, 3, wavelength_samples>::Identity();
    // auto a  = (A * (A.transpose() * A).inverse() * A.transpose()).eval();
    // auto pa = (I - a).eval();
    // return mat.fullPivLu().kernel().eval();
    // eig::FullPivLU<BMatrixType> decomposition(mat);
    // return decomposition.kernel().eval();
  }

  template <uint N>
  eig::Matrix<float, wavelength_samples, N> basis_complement(const CMFS &csys,
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
  basis_complement<16u>(const CMFS &, const BMatrixType &);

  // std::vector<Spec> derive_pca(const std::vector<Spec> &spectra) {
  //   // Calculate maxima/minima
  //   /* auto max_coeff = std::reduce(range_iter(spectra), Spec(0.f),
  //     [](const auto &a, const auto &b) { return a.max(b).eval(); }).maxCoeff();
  //   auto min_coeff = std::reduce(range_iter(spectra), Spec(1.f),
  //     [](const auto &a, const auto &b) { return a.min(b).eval(); }).minCoeff();
  //   auto scale_factor = max_coeff - min_coeff; */

  //   // Calculate mean
  //   auto mean = std::reduce(range_iter(spectra), Spec(0.f),
  //     [](const auto &a, const auto &b) { return (a + b).eval(); })
  //     / static_cast<float>(spectra.size());

  //   // Normalize spectral distributions
  //   std::vector<Spec> normalized(spectra.size());
  //   std::transform(range_iter(spectra), normalized.begin(),
  //     [](const auto &s) { return s.matrix().normalized().eval(); });

  //   // Center spectral distribution
  //   std::vector<Spec> centered(spectra.size());
  //   std::transform(range_iter(normalized), centered.begin(),
  //     [&](const auto &s) { return (s - mean).eval(); });

  //   // Compute the covariance matrix
  //   eig::MatrixXf cov(spectra.size(), wavelength_samples);
  //   std::copy(range_iter(centered), cov.rowwise().begin());
  //   // fmt::print("{}\n", cov.reshaped());
  //   cov = (cov.adjoint() * cov);
  //   cov /= static_cast<float>(wavelength_samples - 1);

  //   // Solver step
  //   eig::SelfAdjointEigenSolver<eig::MatrixXf> slv(cov);
  //   eig::VectorXf eigen_values  = slv.eigenvalues() / slv.eigenvalues().sum();

  //   // Get the principal component eigenvectors and their orthogonal complement
  //   eig::MatrixXf basis = slv.eigenvectors();
  //   eig::MatrixXf ortho = basis.fullPivLu().kernel();

  //   std::vector<Spec> r(wavelength_samples);
  //   std::copy(range_iter(ortho.colwise()), r.begin());
  //   std::reverse(range_iter(r));

  //   // Get orthogonal complement

  //   // using KernTy = eig::Matrix<float, wavelength_samples, wavelength_samples>;
  //   // KernTy basis = eigen_vecs;
  //   // eig::FullPivLU<KernTy> lu;
  //   // lu.compute(basis);
  //   // auto nu = lu.kernel();

  //   //  basis.fullPivLu().kernel(); //.eval();

  //   /* eig::CompleteOrthogonalDecomposition<eig::MatrixXf> cod;
  //   cod.compute(eigen_vecs);
  //   eig::MatrixXf V = cod.matrixZ().transpose();
  //   eig::MatrixXf nullsp = V.block(0, cod.rank(), V.rows(), V.cols() - cod.rank());
  //   eig::MatrixXf P = cod.colsPermutation();
  //   nullsp = P * nullsp; */

  //   // auto prod = (basis * nullsp).eval();

  //   // fmt::print("Null space, {}x{}:\n{}\n", nullsp.rows(), nullsp.cols(), nullsp.reshaped());
  //   // fmt::print("Prod, {}x{}:\n{}\n", prod.rows(), prod.cols(), prod.reshaped());

  //   return r;
  // }
} // namespace met