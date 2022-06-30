#pragma once

#include <metameric/core/spectral_mapping.hpp>
#include <metameric/gui/detail/linear_scheduler/task.hpp>
#include <small_gl/program.hpp>
#include <small_gl/dispatch.hpp>

namespace met {
  class MappingTask : public detail::AbstractTask {
    gl::Program     m_generate_program;
    gl::ComputeInfo m_generate_dispatch;

    gl::Program     m_mapping_program;
    gl::ComputeInfo m_mapping_dispatch;

  public:
    MappingTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met