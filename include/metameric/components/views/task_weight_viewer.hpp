#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>
#include <metameric/components/pipeline/detail/task_texture_from_buffer.hpp>
#include <metameric/components/pipeline/detail/task_texture_resample.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/texture.hpp>

namespace met {
  class WeightViewerTask : public detail::AbstractTask {
    using TextureSubtask  = detail::TextureFromBufferTask<gl::Texture2d4f>;
    using ResampleSubtask = detail::TextureResampleTask<gl::Texture2d4f>;
    
    struct UniformBuffer {
      uint n;                                       // Nr. of points to dispatch computation for
      uint n_verts;                                 // Nr. of vertices defining convex hull
      eig::Array4u selection[barycentric_weights];  // Selection flags for vertices in convex hull
    };

    // Local state
    eig::Array2u m_texture_size; // Current output size of texture

    // Weight sum computation components
    gl::ComputeInfo m_dispatch;
    gl::Program     m_program;
    gl::Buffer      m_buffer;
    gl::Buffer      m_unif_buffer;
    UniformBuffer  *m_unif_map;

    // Subfunctions
    void eval_view(detail::TaskEvalInfo &);
    void eval_draw(detail::TaskEvalInfo &);

  public:
    WeightViewerTask(const std::string &name);

    void init(detail::TaskInitInfo &) override;
    void dstr(detail::TaskDstrInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met