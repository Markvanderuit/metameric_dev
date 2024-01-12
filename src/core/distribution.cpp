#include <metameric/core/distribution.hpp>
#include <metameric/core/math.hpp>
#include <small_gl/buffer.hpp>

namespace met {
  gl::Buffer Distribution::to_buffer_std140() const {
    std::vector<eig::Array4f> data;
    data.resize(m_pdf.size() + m_cdf.size());
    for (uint i = 0; i < m_pdf.size(); ++i)
      data[i] = eig::Array4f(m_pdf[i]);
    for (uint i = 0; i < m_cdf.size(); ++i)
      data[m_pdf.size() + i] = eig::Array4f(m_cdf[i]);
    return gl::Buffer {{ .data = cnt_span<const std::byte>(data) }};    
  }

  gl::Buffer Distribution::to_buffer_std430() const {
    std::vector<float> data;
    data.resize(m_pdf.size() + m_cdf.size());
    for (uint i = 0; i < m_pdf.size(); ++i)
      data[i] = m_pdf[i];
    for (uint i = 0; i < m_cdf.size(); ++i)
      data[m_pdf.size() + i] = m_cdf[i];
    return gl::Buffer {{ .data = cnt_span<const std::byte>(data) }};    
  }
} // namespace met