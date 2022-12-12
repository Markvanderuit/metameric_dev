#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/math.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/sampler.hpp>

namespace met {
  class DrawWeightsTask : public detail::AbstractTask {
    struct UniformBuffer {
      uint n;                               // Nr. of points to dispatch computation for
      uint n_verts;                         // Nr. of vertices defining convex hull
      eig::Array4u selection[barycentric_weights];  // Selection flags for vertices in convex hull
    };

    // State information
    std::string       m_parent;
    uint              m_mapping_i_cache;
    std::vector<uint> m_selection_cache;

    // Weight sum computation components
    gl::ComputeInfo m_dispatch;
    gl::Program     m_program;
    gl::Buffer      m_unif_buffer;
    UniformBuffer  *m_unif_map;

    // Buffer to texture components
    gl::ComputeInfo m_texture_dispatch;
    gl::Program     m_texture_program;

    // Gamma correction components
    gl::ComputeInfo m_srgb_dispatch;
    gl::Program     m_srgb_program;
    gl::Sampler     m_srgb_sampler;
    
  public:
    DrawWeightsTask(const std::string &name, const std::string &parent);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met