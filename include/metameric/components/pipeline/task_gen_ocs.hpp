#pragma once

#include <metameric/core/mesh.hpp>
#include <metameric/core/scheduler.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class GenOCSTask : public detail::AbstractTask {
    gl::Buffer      m_uniform_buffer;
    gl::Program     m_program;    
    gl::ComputeInfo m_dispatch;    
    bool            m_stale;
    BasicAlMesh     m_sphere_mesh;
    std::vector<eig::Array<float, 6, 1>>
                    m_sphere_samples;    
    int             m_gamut_idx;

  public:
    GenOCSTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met