#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <vector>

namespace met {
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

  Spec generate_spectrum(const CMFS &csystem, const Colr &csignal);
} // namespace met