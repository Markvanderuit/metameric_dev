#pragma once

#include <metameric/core/spectral_mapping.hpp>
#include <metameric/gui/detail/linear_scheduler/task.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/texture.hpp>

namespace met {
  class MappingTask : public detail::AbstractTask {
    gl::ComputeInfo m_generate_dispatch;
    gl::Program     m_generate_program;

    gl::Buffer      m_mapping_buffer;
    gl::ComputeInfo m_mapping_dispatch;
    gl::Program     m_mapping_program;
    gl::Buffer      m_mapping_texture;

  public:
    MappingTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met