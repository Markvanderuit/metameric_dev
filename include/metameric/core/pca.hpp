#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>

namespace met {
  std::vector<Spec> derive_pca(const std::vector<Spec> &spectra);
} // namespace met