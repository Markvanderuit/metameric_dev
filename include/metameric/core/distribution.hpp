#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>
#include <algorithm>
#include <functional>
#include <numeric>
#include <random>
#include <ranges>

namespace met {
  class UniformSampler {
    std::mt19937                          m_rng;
    std::uniform_real_distribution<float> m_distr;
    
  public:
    UniformSampler()
    : m_rng(std::random_device()()),
      m_distr(0.f, 1.f) { }

    UniformSampler(uint seed)
    : m_rng(seed),
      m_distr(0.f, 1.f) { }

    UniformSampler(float min_v, float max_v)
    : m_rng(std::random_device()()),
      m_distr(min_v, max_v) { }

    UniformSampler(float min_v, float max_v, uint seed)
    : m_rng(seed),
      m_distr(min_v, max_v) { }

    float next_1d() {
      return m_distr(m_rng);
    }

    template <uint N>
    eig::Array<float, N, 1> next_nd() {
      eig::Array<float, N, 1> v;
      std::ranges::generate(v, std::bind(&UniformSampler::next_1d, this));
      return v;
    }

    eig::Array<float, -1, 1> next_nd(uint n) {
      eig::Array<float, -1, 1> v(n);
      std::ranges::generate(v, std::bind(&UniformSampler::next_1d, this));
      return v;
    }
  };

  class Distribution {
    std::vector<float> m_cdf;
    std::vector<float> m_pmf;

  public:
    Distribution() = default;

    Distribution(std::span<const float> values)
    : m_pmf(range_iter(values)) {
      m_cdf.resize(m_pmf.size());
      std::inclusive_scan(range_iter(m_pmf), m_cdf.begin());
    }

    float pmf(uint i) const {
      return m_pmf[i];
    }

    float cdf(uint i) const {
      return m_cdf[i];
    }

    float sum() const {
      return m_cdf.back();  
    }

    float norm() const {
      return 1.f / sum();
    }

    size_t size() const {
      return m_pmf.size();
    }

    uint sample(float value) const {
      return std::distance(m_cdf.begin(), 
                           std::ranges::lower_bound(m_cdf, value * sum()));
    }
  };
} // namespace met