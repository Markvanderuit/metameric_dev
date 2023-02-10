#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class GenBarycentricWeightsTask : public detail::AbstractTask {
    struct WorkBuffer {
      uint i_vert;                     // Index of current vertex to process
      uint n_elems;                    // Nr. of relevant mesh elements for vertex
      uint elems[barycentric_weights]; // Idxs of relevant mesh elements for vertex
    };
    
    struct UniformBuffer {
      uint n;       // Nr. of points to dispatch computation for
      uint n_verts; // Nr. of vertices defining surrounding hull
      uint n_elems; // Nr. of elements defining surrounding hull
    };

    gl::ComputeInfo m_dispatch_bary;
    gl::ComputeInfo m_dispatch_bsum;
    gl::Program     m_program_bary;
    gl::Program     m_program_bsum;
    gl::Buffer      m_uniform_buffer;
    UniformBuffer  *m_uniform_map;

  public:
    GenBarycentricWeightsTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void dstr(detail::TaskDstrInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met