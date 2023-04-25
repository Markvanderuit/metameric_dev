#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/components/pipeline/detail/bvh.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class GenDelaunayWeightsTask : public detail::TaskNode {
    using BVH     = detail::BVH<eig::AlArray3f, detail::BVHNode, 8, detail::BVHPrimitive::eTetrahedron>;
    using BVHColr = detail::BVH<eig::Array3f, detail::BVHNode, 8, detail::BVHPrimitive::ePoint>;
    
    struct UniformBuffer {
      uint n;       // Nr. of points to dispatch computation for
      uint n_verts; // Nr. of vertices defining surrounding hull
      uint n_elems; // Nr. of elements defining surrounding hull
    };
    
    // Packed wrapper data for tetrahedron; 64 bytes for std430 
    struct ElemPack {
      eig::Matrix<float, 4, 3> inv; // Last column is padding
      eig::Matrix<float, 4, 1> sub; // Last value is padding
    };

    gl::ComputeInfo           m_dispatch;
    gl::Program               m_program;
    gl::Buffer                m_pack_buffer;
    gl::Buffer                m_tree_buffer;
    gl::Buffer                m_uniform_buffer;
    UniformBuffer            *m_uniform_map;
    std::span<ElemPack>       m_pack_map;
    std::span<eig::AlArray3f> m_vert_map;
    std::span<eig::Array4u>   m_elem_map;

    struct BVHUniformBuffer {
      uint n_colr_nodes;
      uint n_elem_nodes;
      uint n_elems;
    };

    struct BVHWorkBuffer {
      gl::Buffer data;

      void clear_head() {
        data.clear({ }, 1u, sizeof(eig::Array4u));
      }
    };

    gl::Buffer              m_bvh_div_sg_buffer;
    gl::ComputeInfo         m_bvh_div_sg_dispatch;
    gl::Program             m_bvh_div_sg_program;
    gl::Buffer              m_bvh_div_32_buffer;
    gl::ComputeInfo         m_bvh_div_32_dispatch;
    gl::Program             m_bvh_div_32_program;

    gl::ComputeIndirectInfo m_bvh_desc_dispatch;
    gl::Program             m_bvh_desc_program;
    gl::ComputeIndirectInfo m_bvh_bary_dispatch;
    gl::Program             m_bvh_bary_program;

    gl::Buffer              m_bvh_comp_buffer;
    gl::Buffer              m_bvh_colr_buffer;
    gl::Buffer              m_bvh_elem_buffer;
    gl::Buffer              m_bvh_unif_buffer;
    BVHUniformBuffer       *m_bvh_unif_map;
    gl::Buffer              m_bvh_init_work; // This is continuously copied over
    gl::Buffer              m_bvh_init_head; // This is continuously copied over
    gl::Buffer              m_bvh_curr_work; // This is continuously swapped
    gl::Buffer              m_bvh_next_work; // This is continuously swapped

  public:
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
    bool is_active(SchedulerHandle &) override;
  };
} // namespace met