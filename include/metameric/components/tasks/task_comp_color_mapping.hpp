#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/texture.hpp>

namespace met {
  class CompColorMappingTask : public detail::AbstractTask {
    gl::ComputeInfo m_mapping_dispatch;
    gl::Program     m_mapping_program;

    gl::Program     m_texture_program;
    gl::ComputeInfo m_texture_dispatch;

  public:
    CompColorMappingTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met