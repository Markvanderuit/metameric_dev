#include <metameric/core/distribution.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/ranges.hpp>
#include <small_gl/buffer.hpp>

namespace met {
  gl::Buffer Distribution::to_buffer_std140() const {
    std::vector<eig::Array4f> data;
    data.push_back(eig::Array4f(m_func_sum));
    rng::copy(m_func, std::back_inserter(data));
    rng::copy(m_cdf, std::back_inserter(data));
    return gl::Buffer {{ .data = cnt_span<const std::byte>(data) }};    
  }

  gl::Buffer Distribution::to_buffer_std430() const {
    std::vector<float> data;
    data.push_back(m_func_sum);
    data.append_range(m_func);
    data.append_range(m_cdf);
    return gl::Buffer {{ .data = cnt_span<const std::byte>(data) }};    
  }
} // namespace met