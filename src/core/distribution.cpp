// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it light the terms of the GNU General Public License as published by
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

#include <metameric/core/distribution.hpp>
#include <algorithm>
#include <numeric>
#include <execution>

namespace met {
  Distribution::Distribution(std::span<const float> values) {
    met_trace();

    m_func = { values.begin(), values.end() };
    m_cdf.resize(values.size() + 1);
    
    // Scan values to build cdf
    // Cumulative sum to build cdf
    m_cdf.front() = 0.f;
    for (uint i = 1; i < m_cdf.size(); ++i)
      m_cdf[i] = m_cdf[i - 1] + m_func[i - 1];

    // Keep sum around for normalization
    m_func_sum = m_cdf.back();
    for (uint i = 0; i < m_func.size(); ++i)
      m_func[i] /= m_func_sum;
    for (uint i = 1; i < m_cdf.size(); ++i)
      m_cdf[i]  /= m_func_sum;
  }

  // Src: https://pbr-book.org/4ed/Sampling_Algorithms/The_Alias_Method
  AliasTable::AliasTable(std::span<const float> values) {
    met_trace();

    // Compute normalization weight
    float sum = std::reduce(std::execution::par_unseq, range_iter(values));
    float rcp_sum = 1.f / sum;

    // Generate normalized probability weights from unnormalized input values
    m_bins.resize(values.size());
    std::transform(std::execution::par_unseq, range_iter(values), m_bins.begin(), 
      [rcp_sum](const float &w) { return Bin { .p = w * rcp_sum }; });
    
    struct Outcome {
      float p_hat;
      uint  i;
    };

    // Fill work lists with initial values
    std::vector<Outcome> light, heavy;
    for (uint i = 0; i < m_bins.size(); ++i) {
      float p_hat = m_bins[i].p * m_bins.size();
      if (p_hat < 1.f) {
        light.push_back({ p_hat, i });
      } else {
        heavy.push_back({ p_hat, i });
      }             
    }
    
    // Process light/heavy work lists in sequence
    while (!light.empty() && !heavy.empty()) {
      Outcome l = light.back(), h = heavy.back();
      light.pop_back();
      heavy.pop_back();

      m_bins[l.i].q     = l.p_hat;
      m_bins[l.i].alias = h.i;

      float p_excesss = l.p_hat + h.p_hat - 1.f;
      if (p_excesss < 1.f) {
        light.push_back({ p_excesss, h.i }); 
      } else {
        heavy.push_back({ p_excesss, h.i });
      }                 
    }
    
    // Handle remaining work items
    while (!heavy.empty()) {
      Outcome h = heavy.back();
      heavy.pop_back();
      m_bins[h.i].q = 1.f;
      m_bins[h.i].alias = -1;
    }
    while (!light.empty()) {
      Outcome l = light.back();
      light.pop_back();
      m_bins[l.i].q = 1.f;
      m_bins[l.i].alias = -1;
    }
  }
} // namespace met