#pragma once

#include <metameric/core/spectral_mapping.hpp>
#include <metameric/gui/detail/linear_scheduler/task.hpp>
#include <small_gl/buffer.hpp>

namespace met {
  class MappingTask : public detail::AbstractTask {
    gl::Buffer m_gamut_vertex_buffer;
    gl::Buffer m_gamut_spectra_buffer;

    std::span<Color> m_gamut_vertices;
    std::span<Spec>  m_gamut_spectra;
    
  public:
    MappingTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met