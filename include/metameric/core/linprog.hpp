#pragma once

#include <metameric/core/spectrum.hpp>

namespace met {
  std::vector<Spec> generate_metamer_boundary(const SpectralMapping &mapping_i,
                                              const SpectralMapping &mapping_j,
                                              const Spec            &reflectance,
                                              uint n_samples = 256);
  std::vector<Spec> generate_metamer_boundary(const CMFS &csystem_i,
                                              const CMFS &csystem_j,
                                              const Colr &csignal_i,
                                              uint n_samples = 256);
} // namespace met