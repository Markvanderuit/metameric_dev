#include <metameric/core/distribution.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/ranges.hpp>
#include <small_gl/buffer.hpp>

namespace met {
  gl::Buffer Distribution::to_buffer_std140() const {
    met_trace();

    std::vector<eig::Array4f> data;
    data.push_back(eig::Array4f(m_func_sum));
    rng::copy(m_func, std::back_inserter(data));
    rng::copy(m_cdf, std::back_inserter(data));

    return gl::Buffer {{ .data = cnt_span<const std::byte>(data) }};    
  }

  gl::Buffer Distribution::to_buffer_std430() const {
    met_trace();

    std::vector<float> data;
    data.push_back(m_func_sum);
    rng::copy(m_func, std::back_inserter(data));
    rng::copy(m_cdf,  std::back_inserter(data));

    return gl::Buffer {{ .data = cnt_span<const std::byte>(data) }};    
  }

  // template <uint N>
  // gl::Buffer DistributionArray<N>::to_buffer_std140() const {
  //   met_trace();

  //   struct alignas(16) DistrBlock {
      
  //   };
    
  //   std::vector<eig::Array4f> out;
  //   for (const auto &distr : data) {
  //     out.push_back(eig::Array4f(distr.m_func_sum));
  //     rng::copy(distr.m_func, std::back_inserter(out));
  //     rng::copy(distr.m_cdf, std::back_inserter(out));
  //   }

  //   return gl::Buffer {{ .data = cnt_span<const std::byte>(out) }}; 
  // }
  
  // template <uint N>
  // gl::Buffer DistributionArray<N>::to_buffer_std430() const {
  //   met_trace();

  //   std::vector<float> out;
  //   for (const auto &distr : data) {
  //     out.push_back(distr.m_func_sum);
  //     out.append_range(distr.m_func);
  //     out.append_range(distr.m_cdf);
  //   }

  //   return gl::Buffer {{ .data = cnt_span<const std::byte>(out) }}; 
  // }

  // template DistributionArray<wavelength_samples>;
} // namespace met