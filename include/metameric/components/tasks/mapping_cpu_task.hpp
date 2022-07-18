#pragma once

#include <metameric/core/spectrum.hpp>
#include <metameric/core/scheduler.hpp>
#include <small_gl/texture.hpp>
#include <vector>

namespace met {
  class MappingCPUTask : public detail::AbstractTask {
    std::vector<eig::Array3f>  m_input;
    std::vector<eig::Vector4f> m_barycentric_texture;
    std::vector<Spec>          m_spectral_texture;

    std::vector<eig::Array3f> m_output_d65, 
                              m_output_d65_err,
                              m_output_fl2, 
                              m_output_fl11;

    gl::Texture2d3f m_input_texture,
                    m_output_d65_texture,
                    m_output_d65_err_texture, 
                    m_output_fl2_texture,
                    m_output_fl11_texture;

  public:
    MappingCPUTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met