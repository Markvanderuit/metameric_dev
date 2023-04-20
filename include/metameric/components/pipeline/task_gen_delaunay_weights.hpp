#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/components/pipeline/detail/bvh.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class GenDelaunayWeightsTask : public detail::TaskNode {
    using BVH = detail::BVH<eig::AlArray3f, detail::BVHNode, 8, detail::BVHPrimitive::eTetrahedron>;
    
    struct UniformBuffer {
      uint n;       // Nr. of points to dispatch computation for
      uint n_verts; // Nr. of vertices defining surrounding hull
      uint n_elems; // Nr. of elements defining surrounding hull
    };

    gl::ComputeInfo           m_dispatch;
    gl::Program               m_program;
    gl::Buffer                m_tree_buffer;
    gl::Buffer                m_uniform_buffer;
    UniformBuffer            *m_uniform_map;
    std::span<eig::AlArray3f> m_vert_map;
    std::span<eig::Array4u>   m_elem_map;

  public:
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
    bool is_active(SchedulerHandle &) override;
  };
} // namespace met