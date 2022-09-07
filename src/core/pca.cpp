#define EIGEN_NO_DEBUG
#define EIGEN_NO_STATIC_ASSERT
#include <metameric/core/pca.hpp>
#include <algorithm>
#include <execution>
#include <numeric>
#include <Eigen/Sparse>

namespace met {
  std::vector<Spec> derive_pca(const std::vector<Spec> &spectra) {
    // Calculate maxima/minima
    /* auto max_coeff = std::reduce(range_iter(spectra), Spec(0.f),
      [](const auto &a, const auto &b) { return a.max(b).eval(); }).maxCoeff();
    auto min_coeff = std::reduce(range_iter(spectra), Spec(1.f),
      [](const auto &a, const auto &b) { return a.min(b).eval(); }).minCoeff();
    auto scale_factor = max_coeff - min_coeff; */

    // Calculate mean
    auto mean = std::reduce(range_iter(spectra), Spec(0.f),
      [](const auto &a, const auto &b) { return (a + b).eval(); })
      / static_cast<float>(spectra.size());

    // Normalize spectral distributions
    std::vector<Spec> normalized(spectra.size());
    std::transform(range_iter(spectra), normalized.begin(),
      [](const auto &s) { return s.matrix().normalized().eval(); });

    // Center spectral distribution
    std::vector<Spec> centered(spectra.size());
    std::transform(range_iter(normalized), centered.begin(),
      [&](const auto &s) { return (s - mean).eval(); });

    // Compute the covariance matrix
    eig::MatrixXf cov(spectra.size(), wavelength_samples);
    std::copy(range_iter(centered), cov.rowwise().begin());
    // fmt::print("{}\n", cov.reshaped());
    cov = (cov.adjoint() * cov);
    cov /= static_cast<float>(wavelength_samples - 1);

    // Solver step
    eig::SelfAdjointEigenSolver<eig::MatrixXf> slv(cov);
    eig::VectorXf normalized_values = slv.eigenvalues() / slv.eigenvalues().sum();

    // Get the principal component eigenvectors and their orthogonal complement
    eig::MatrixXf basis = slv.eigenvectors();
    eig::MatrixXf ortho = basis.fullPivLu().kernel();

    std::vector<Spec> r(wavelength_samples);
    std::copy(range_iter(ortho.colwise()), r.begin());
    std::reverse(range_iter(r));

    // Get orthogonal complement

    // using KernTy = eig::Matrix<float, wavelength_samples, wavelength_samples>;
    // KernTy basis = eigen_vecs;
    // eig::FullPivLU<KernTy> lu;
    // lu.compute(basis);
    // auto nu = lu.kernel();

    //  basis.fullPivLu().kernel(); //.eval();

    /* eig::CompleteOrthogonalDecomposition<eig::MatrixXf> cod;
    cod.compute(eigen_vecs);
    eig::MatrixXf V = cod.matrixZ().transpose();
    eig::MatrixXf nullsp = V.block(0, cod.rank(), V.rows(), V.cols() - cod.rank());
    eig::MatrixXf P = cod.colsPermutation();
    nullsp = P * nullsp; */

    // auto prod = (basis * nullsp).eval();

    // fmt::print("Null space, {}x{}:\n{}\n", nullsp.rows(), nullsp.cols(), nullsp.reshaped());
    // fmt::print("Prod, {}x{}:\n{}\n", prod.rows(), prod.cols(), prod.reshaped());

    return r;
  }
} // namespace met