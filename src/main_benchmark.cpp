// STL includes
#include <iostream>
#include <cstdlib>
#include <exception>
#include <vector>

// Misc includes
#include <immintrin.h>
#include <fmt/core.h>
#include <benchmark/benchmark.h>

// Metameric includes
#include <metameric/core/detail/eigen.hpp>
#include <metameric/core/spectrum.hpp>

using namespace met;

static void bm_eig_addition(benchmark::State &state) {
  Spec a = 5.f,  
       b = 1.f,
       c = (a > b).select(a, 5.f);

  for (size_t i = 0; i < a.size(); ++i) {
    a[i] = i;
    b[i] = a.size() - i;
  }

  for (auto _ : state) {
      b *= (a * a);
  }
}

BENCHMARK(bm_eig_addition);
BENCHMARK_MAIN();