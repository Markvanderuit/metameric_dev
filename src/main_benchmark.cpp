// STL includes
#include <iostream>
#include <cstdlib>
#include <exception>
#include <vector>

// Misc includes
#include <immintrin.h>
#include <fmt/core.h>
#include <benchmark/benchmark.h>
// #define EIGEN_DONT_VECTORIZE
#include <Eigen/Dense>

// Metameric includes
#include <metameric/core/spectrum.hpp>

static void bm_eig_addition(benchmark::State &state) {
  using Spectrum = Eigen::Vector<float, met::wavelength_samples>;
  Spectrum a = Spectrum::Ones(), b = Spectrum::Ones();
  for (size_t i = 0; i < a.size(); ++i) {
    a[i] = i;
    b[i] = a.size() - i;
  }

  for (auto _ : state) {
    for (int i = 0; i < 1'000'000; ++i)
      b += a;
  }
}

BENCHMARK(bm_eig_addition);

void add_to_m256(met::Spectrum &a, met::Spectrum &b) {
  constexpr size_t n  = met::wavelength_samples;
  constexpr size_t u8 = (n / 8) * 8;
  constexpr bool do_u4 = n - u8 >= 4;

  float *a_p = a.data();
  float *b_p = b.data();

  // unrolled primary 8 wide
  size_t i = 0;
  __m256 a256, b256, r256;
  for (; i < u8; i += 8) {
    a256 = _mm256_load_ps(a_p + i);
    b256 = _mm256_load_ps(b_p + i);
    r256 = _mm256_add_ps(a256, b256);
    _mm256_store_ps(b_p + i, r256);
  }

  // unrolled primary 4 wide
  if  (do_u4) {
    __m128 a256, b256, r256;
    a256 = _mm_load_ps(a_p + i);
    b256 = _mm_load_ps(b_p + i);
    r256 = _mm_add_ps(a256, b256);
    _mm_store_ps(b_p + i, r256);
    i += 4;
  }

  // remainder
  for (; i < n; ++i)
    b[i] += a[i];
}

void add_to_m256_16(met::Spectrum &a, met::Spectrum &b) {
  constexpr size_t n  = met::wavelength_samples;
  constexpr size_t u16 = (n / 16) * 16;
  constexpr size_t u8 = ((n - u16) / 8) * 8 + u16;
  constexpr size_t u4 = ((n - u8) / 8) * 8 + u8;

  float *a_p = a.data();
  float *b_p = b.data();

  // unrolled primary 8 wide
  size_t i = 0;
  
  for (; i < u16; i += 16) {
    __m256 a256_1 = _mm256_loadu_ps(a_p + i);
    __m256 b256_1 = _mm256_loadu_ps(b_p + i);
    __m256 r256_1 = _mm256_add_ps(a256_1, b256_1);
    _mm256_store_ps(b_p + i, r256_1);

    __m256 a256_2 = _mm256_loadu_ps(a_p + i + 8);
    __m256 b256_2 = _mm256_loadu_ps(b_p + i + 8);
    __m256 r256_2 = _mm256_add_ps(a256_2, b256_2);
    _mm256_store_ps(b_p + i + 8, r256_2);
  }

  /* for (; i < u8; i += 8) {
    __m256 a256 = _mm256_loadu_ps(a_p + i);
    __m256 b256 = _mm256_loadu_ps(b_p + i);
    __m256 r256 = _mm256_add_ps(a256, b256);
    _mm256_store_ps(b_p + i, r256);
  }

  for (; i < u4; i += 4) {
    __m128 a128 = _mm_loadu_ps(a_p + i);
    __m128 b128 = _mm_loadu_ps(b_p + i);
    __m128 r128 = _mm_add_ps(a128, b128);
    _mm_store_ps(b_p + i, r128);
  }

  // remainder
  for (; i < n; ++i)
    b[i] += a[i]; */
}

static void bm_m256_addition(benchmark::State& state) {
  using namespace met;

  Spectrum a = 1.f, b = 1.f;
  for (size_t i = 0; i < a.size(); ++i) {
    a[i] = i;
    b[i] = a.size() - i;
  }
  // std::cout << a.to_string() << '\n' << b.to_string() << '\n';

  // size_t u4 = ((n - u8) / 4) * 4 + u8;
  // size_t u4 = (n / 4) * 4; //((n - u8) / 4) * 4 + u8;

  for (auto _ : state) {
    for (int j = 0; j < 1'000'000; ++j)
      add_to_m256_16(a, b);
  }

  // fmt::print("m256 - {}\n", b.to_string());
}

BENCHMARK(bm_m256_addition);

static void bm_linear_addition(benchmark::State& state) {
  using namespace met;
  
  Spectrum a = 1.f, b = 1.f;
  for (size_t i = 0; i < a.size(); ++i) {
    a[i] = i;
    b[i] = a.size() - i;
  }

  for (auto _ : state) {
    for (int i = 0; i < 1'000'000; ++i)
      b += a;
  }

  // fmt::print("m256 - {}\n", b.to_string());
}

BENCHMARK(bm_linear_addition);

BENCHMARK_MAIN();