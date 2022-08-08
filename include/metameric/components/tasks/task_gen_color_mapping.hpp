#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class GenColorMappingTask : public detail::AbstractTask {
    uint            m_mapping_i;
    gl::Program     m_mapping_program;
    gl::ComputeInfo m_mapping_dispatch;
    gl::Program     m_mapping_program_sg;
    gl::ComputeInfo m_mapping_dispatch_sg;

  public:
    GenColorMappingTask(const std::string &name, uint mapping_i);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
    void dstr(detail::TaskDstrInfo &) override;
  };
} // namespace met