#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/pca.hpp>
#include <metameric/core/spectrum.hpp>
#include <limits>
#include <vector>

namespace met {
  // Comparative relationship operands for a linear program
  enum class LPComp : int {
    eEQ = 0, // ==
    eLE =-1, // <=
    eGE = 1  // >=
  };

  // Full set of parameters for a linear program
  template <typename Ty, uint N, uint M>
  struct LPParams {
    // Components for defining "min C^T x + c0, w.r.t. Ax <=> b"
    eig::Array<Ty, N, 1> C;
    eig::Array<Ty, M, N> A;
    eig::Array<Ty, M, 1> b;
    Ty                   c0;

    // Relation for Ax <=> b (<=, ==, >=)
    eig::Array<LPComp, M, 1> r = LPComp::eEQ;

    // Upper/lower limits to x, and masks to activate/deactivate them
    eig::Array<Ty, N, 1> l     = std::numeric_limits<Ty>::min();
    eig::Array<Ty, N, 1> u     = std::numeric_limits<Ty>::max();
  };

  // Solve a linear program using a params object
  template <typename Ty, uint N, uint M>
  eig::Matrix<Ty, N, 1> linprog(LPParams<Ty, N, M> &params);

  // Solve a linear program using provided data
  template <typename Ty, uint N, uint M>
  eig::Matrix<Ty, N, 1> linprog(const eig::Array<Ty, N, 1> &C,
                                const eig::Array<Ty, M, N> &A,
                                const eig::Array<Ty, M, 1> &b,
                                const eig::Array<LPComp, M, 1>
                                                           &r = LPComp::eEQ,
                                const eig::Array<Ty, N, 1> &l = std::numeric_limits<Ty>::min(),
                                const eig::Array<Ty, N, 1> &u = std::numeric_limits<Ty>::max());

  namespace detail {
    template <uint N>
    std::vector<eig::Array<float, N, 1>> generate_unit_dirs(uint n_samples);
  } // namespace detail

  std::vector<Spec> generate_metamer_boundary(const CMFS &csystem_i,
                                              const CMFS &csystem_j,
                                              const Colr &csignal_i,
                                              const std::vector<eig::Array<float, 6, 1>> &samples);

  std::vector<Spec> generate_metamer_boundary(const CMFS &csystem_i,
                                              const CMFS &csystem_j,
                                              const Colr &csignal_i,
                                              uint n_samples = 256);
  
  std::vector<Colr> generate_metamer_boundary_c(const CMFS &csystem_i,
                                                const CMFS &csystem_j,
                                                const Colr &csignal_i,
                                                const std::vector<eig::Array<float, 6, 1>> &samples);
  
  std::vector<Colr> generate_metamer_boundary_c(const CMFS &csystem_i,
                                                const CMFS &csystem_j,
                                                const Colr &csignal_i,
                                                uint n_samples = 256);

  Spec generate_spectrum_from_basis(const BMatrixType &basis, 
                                    const CMFS &csystem, 
                                    const Colr &csignal);

  Spec generate_spectrum(const CMFS &csystem, const Colr &csignal);
} // namespace met