#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>
#include <small_gl/fwd.hpp>
#include <algorithm>
#include <functional>
#include <numeric>
#include <random>
#include <fmt/core.h>
#include <fmt/ranges.h>

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

  public:
    constexpr static uint32_t min() { return std::mt19937::min(); }
    constexpr static uint32_t max() { return std::mt19937::max(); }
    uint32_t g() { return m_rng(); }
    uint32_t operator()() { return m_rng(); }
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

  // Simple 1d sampling distribution
  class Distribution {
    float              m_func_sum;
    std::vector<float> m_func;
    std::vector<float> m_pdf;
    std::vector<float> m_cdf;

  public:
    Distribution() = default;

    Distribution(std::span<const float> values) {
      m_func.assign_range(values);
      m_pdf.resize(values.size());
      m_cdf.resize(values.size() + 1);
      
      // Scan values to build cdf
      m_cdf.front() = 0.f;
      for (uint i = 1; i < m_cdf.size(); ++i)
        m_cdf[i] = m_cdf[i - 1] + m_func[i - 1] / static_cast<float>(m_func.size());    

      // Keep sum around
      m_func_sum = m_cdf.back();

      // Normalize cdf
      for (uint i = 1; i < m_cdf.size(); ++i)
        m_cdf[i] /= m_func_sum;
    }

    float cdf(uint i) const {
      return m_cdf[i];
    }

    float sum() const {
      return m_func_sum;  
    }

    float inv_sum() const {
      return sum() == 0.f ? 0.f : 1.f / sum();
    }

    size_t size() const {
      return m_pdf.size();
    }

    uint sample_discrete(float u) const {
      // Find iterator to first element greater than u; ergo std::upper_bound
      int i = 0;
      while (u > m_cdf[i] && i < m_cdf.size() - 1) 
        i++;
      i -= 1;
      return static_cast<uint>(rng::clamp(i, 0, static_cast<int>(m_func.size()) - 1));
    }

    // Returns between 0 and 1
    float sample(float u) const {
      uint  i     = sample_discrete(u);
      float d_cdf = m_cdf[i + 1] - m_cdf[i];

      if (d_cdf == 0.f) {
        return static_cast<float>(i) / m_func.size();
      } else {
        float a = (u - m_cdf[i]) / d_cdf;
        return (static_cast<float>(i) + a) / m_func.size();
      }
    }

    float pdf_discrete(uint i) const {
      return m_func[i] / m_func_sum;
    }

    float pdf(float sample) const {
      uint  i = static_cast<uint>(sample * (m_func.size() - 1));
      float a = sample * (m_func.size() - 1) - static_cast<float>(i);
      
      if (a == 0.f) {
        return pdf_discrete(i);
      } else {
        return std::lerp(pdf_discrete(i), pdf_discrete(i + 1), a);
      }
    }

    gl::Buffer to_buffer_std140() const;
    gl::Buffer to_buffer_std430() const;
  };
} // namespace met