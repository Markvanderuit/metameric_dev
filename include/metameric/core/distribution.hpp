#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>
#include <algorithm>
#include <functional>
#include <numeric>
#include <random>

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
      rng::generate(v, std::bind(&UniformSampler::next_1d, this));
      return v;
    }

    eig::Array<float, -1, 1> next_nd(uint n) {
      eig::Array<float, -1, 1> v(n);
      rng::generate(v, std::bind(&UniformSampler::next_1d, this));
      return v;
    }
  };

  class PCGSampler {
    using Distr = std::uniform_real_distribution<float>;

    uint  m_state;
    Distr m_distr;

    uint pcg_hash() {
        m_state = m_state * 747796405u + 2891336453u;
        uint v = m_state;
        v ^= v >> ((v >> 28u) + 4u);
        v *= 277803737u;
        v ^= v >> 22u;
        return v;
    }

  public:
    PCGSampler()
    : m_state(std::random_device()()),
      m_distr(0.f, 1.f) { }

    PCGSampler(uint seed)
    : m_state(seed),
      m_distr(0.f, 1.f) { }

    PCGSampler(float min_v, float max_v)
    : m_state(std::random_device()()),
      m_distr(min_v, max_v) { }

    PCGSampler(float min_v, float max_v, uint seed)
    : m_state(seed),
      m_distr(min_v, max_v) { }
    
    float next_1d() {
      return m_distr(*this);
    }

    template <uint N>
    eig::Array<float, N, 1> next_nd() {
      eig::Array<float, N, 1> v;
      rng::generate(v, std::bind(&PCGSampler::next_1d, this));
      return v;
    }

    eig::Array<float, -1, 1> next_nd(uint n) {
      eig::Array<float, -1, 1> v(n);
      rng::generate(v, std::bind(&PCGSampler::next_1d, this));
      return v;
    }
    
  // Implement conformance to std::uniform_random_bit_generator<Sampler>
  public:
    constexpr static uint32_t min() { return 0; }
    constexpr static uint32_t max() { return 4294967295; }
    uint32_t g() { return pcg_hash(); }
    uint32_t operator()() { return pcg_hash(); }
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
      return std::distance(m_cdf.begin(), rng::lower_bound(m_cdf, value * sum()));
    }
  };
} // namespace met