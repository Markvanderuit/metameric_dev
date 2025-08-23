// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/utility.hpp>
#include <random>

namespace met {
  // Encapsulation of PCG hash that 
  // conforms to std::uniform_random_bit_generator
  class PCGEngine {
    // Underlying sequence
    uint m_state;
    constexpr uint pcg_hash() {
      m_state = m_state * 747796405u + 2891336453u;
      uint v = m_state;
      v ^= v >> ((v >> 28u) + 4u);
      v *= 277803737u;
      v ^= v >> 22u;
      return v;
    }

  public:
    // Construct the engine, optionally provide a seed
    constexpr PCGEngine(uint seed = 0)
    : m_state(seed) { }

    // Set engine's current internal state
    constexpr void seed(uint seed) {
      m_state = seed;
    }

    // Advance engine's state z times and discard generated values
    constexpr void discard(uint z = 1) { 
      for (uint i = 0; i < z; ++i)
        pcg_hash(); 
    }
    
    // Advance engine's state and return generated value
    constexpr uint operator()() { 
      return pcg_hash(); 
    }

    // Return smallest and largest possible values in the output range
    constexpr static uint min() { return 0;          }
    constexpr static uint max() { return 4294967295; }
  };
  static_assert(std::uniform_random_bit_generator<PCGEngine>);

  // Simple sampler class that encapsulates a random number engine
  template <typename E> requires (std::uniform_random_bit_generator<E>)
  class UniformSampler {
    E                                     m_engine;
    std::uniform_real_distribution<float> m_distr;
    
  public:
    UniformSampler(uint seed = std::random_device()())
    : m_engine(seed), m_distr(0.f, 1.f) { }

    UniformSampler(float min_v, float max_v, uint seed = std::random_device()())
    : m_engine(seed), m_distr(min_v, max_v) { }

    float next_1d() {
      return m_distr(m_engine);
    }

    template <uint N>
    eig::Array<float, N, 1> next_nd() {
      eig::Array<float, N, 1> v;
      for (float &f : v) f = next_1d();
      return v;
    }

    eig::Array<float, -1, 1> next_nd(uint n) {
      eig::Array<float, -1, 1> v(n);
      for (float &f : v) f = next_1d();
      return v;
    }
  };

  // fwd
  template <uint N> struct DistributionArray;

  // Simple 1d sampling distribution
  class Distribution {
    float              m_func_sum;
    std::vector<float> m_func;
    std::vector<float> m_cdf;

  public:
    Distribution() = default;
    Distribution(std::span<const float> values);

    float cdf(uint i) const {
      return m_cdf[i];
    }

    float sum() const {
      return m_func_sum;
    }

    size_t size() const {
      return m_func.size();
    }

    // Returns a specific index
    uint sample_discrete(float u) const {
      // Find iterator to first element greater than u; ergo std::upper_bound
      int i = 0;
      while (u > m_cdf[i] && i < m_cdf.size() - 1) 
        i++;
      i -= 1;
      return static_cast<uint>(std::clamp(i, 0, static_cast<int>(m_func.size()) - 1));
    }

    float pdf_discrete(uint i) const {
      return m_func[i];
    }

    // Returns between 0 and 1
    float sample(float u) const {
      uint i = sample_discrete(u);
      
      // Size of bucket in CDF, exact interpolant between the two;
      // note, alpha can div-by-0 so u is masked below
      float range = m_cdf[i + 1] - m_cdf[i];
      float alpha = (u - m_cdf[i]) / range;
      
      return (range == 0.f ? i : i + alpha) / m_func.size();
    }

    float pdf(float sample) const {
      uint  i = static_cast<uint>(sample * (m_func.size() - 1));
      float a = sample * (m_func.size() - 1) - i;

      if (a == 0.f) {
        return m_func[i];
      } else {
        return std::lerp(m_func[i], m_func[i + 1], a);
      }
    }
    
    // Data accessors
    std::span<const float> data_func() const { return m_func; }
    std::span<const float> data_cdf()  const { return m_cdf;  }
  };

  // Simple 1d alias table for O(1) sampling
  struct AliasTable {
    struct Bin {
      float p;
      float q;
      int   alias;
      int   _padding;
    };

  private:
    std::vector<Bin> m_bins;

  public: // Construction
    AliasTable() = default;
    AliasTable(std::span<const float> values);
  
  public: // Distribution methods
    // Returns a specific index 
    uint sample(float u) const {
      int    i = std::min<int>(u * m_bins.size(), m_bins.size() - 1);
      float up = std::min<float>(u * m_bins.size() - i, 0x1.fffffep-1);
      return (up < m_bins[i].q) ? i : m_bins[i].alias;
    }

    // Return probability density for a specific sample
    float pdf(uint i) const {
      return m_bins[i].p;
    }

  public: // Accessors and boilerplate
    size_t size() const { return m_bins.size(); }
    
    const Bin &operator[](uint i) const { return m_bins[i]; }
          Bin &operator[](uint i)       { return m_bins[i]; }

    std::span<const Bin> data_bins() const { return m_bins; }
  };
} // namespace met