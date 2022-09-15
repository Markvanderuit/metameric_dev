#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class GenSpectralTextureTask : public detail::AbstractTask {
    struct BarycentricBuffer {
      eig::AlArray3f sub;
      eig::Array44f  inv;
    };

    gl::ComputeInfo m_dispatch;
    gl::Program     m_program;
    gl::ComputeInfo m_dispatch_cl;
    gl::Program     m_program_cl;
    gl::Buffer      m_uniform_buffer;
    gl::Buffer      m_bary_buffer;
    
    BarycentricBuffer *m_bary_map;

  public:
    GenSpectralTextureTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met