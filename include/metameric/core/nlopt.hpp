#pragma once

#include <metameric/core/spectrum.hpp>
#include <nlopt.hpp>

namespace met {
  struct OptInfo {
    nlopt::algorithm algorithm;
    
  };

  eig::ArrayXd solve(const OptInfo &info);

  void test_nlopt();
} // namespace met