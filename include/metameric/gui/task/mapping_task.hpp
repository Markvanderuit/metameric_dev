#pragma once

#include <metameric/core/spectral_mapping.hpp>
#include <metameric/gui/detail/linear_scheduler/task.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/texture.hpp>

namespace met {
  class MappingTask : public detail::AbstractTask {
    // TODO: remove
    gl::Buffer      m_debug_buffer;

    gl::Buffer      m_generate_buffer;
    gl::ComputeInfo m_generate_dispatch;
    gl::Program     m_generate_program;

    gl::Buffer      m_mapping_buffer;
    gl::Buffer      m_mapping_texture;
    gl::ComputeInfo m_mapping_dispatch;
    gl::Program     m_mapping_program;

    gl::Program     m_texture_program;
    gl::ComputeInfo m_texture_dispatch;

  public:
    MappingTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met