#pragma once

#include <metameric/core/scene.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/dispatch.hpp>

namespace met {
  class GenObjectDataTask : public detail::TaskNode {
    uint m_object_i;
    
    struct UnifLayout {
      uint object_i; // Index of active object
      uint n_verts;  // Nr. of vertices defining tesselation
      uint n_elems;  // Nr. of elements defining tesselation
    };

    // Packed wrapper data for tetrahedron; 64 bytes for std430 
    struct ElemPack {
      eig::Matrix<float, 4, 3> inv; // Last column is padding
      eig::Matrix<float, 4, 1> sub; // Last value is padding
    };

    gl::ComputeInfo           m_dispatch;
    gl::Program               m_program;
    gl::Buffer                m_pack_buffer;
    gl::Buffer                m_unif_buffer;
    UnifLayout               *m_unif_map;
    std::span<ElemPack>       m_pack_map;
    std::span<eig::AlArray3f> m_vert_map;
    std::span<eig::Array4u>   m_elem_map;

  public:
    GenObjectDataTask(uint object_i);

    bool is_active(SchedulerHandle &) override;
    void init(SchedulerHandle &)      override;
    void eval(SchedulerHandle &)      override;
  };

  class GenObjectsTask : public detail::TaskNode {
    detail::Subtasks<GenObjectDataTask> m_subtasks;

  public:
    void init(SchedulerHandle &info) override {
      met_trace();

      // Get external resources
      const auto &e_scene   = info.global("scene").getr<Scene>();
      const auto &e_objects = e_scene.components.objects;

      // Add subtasks to perform mapping
      m_subtasks.init(info, e_objects.size(), 
        [](uint i)         { return fmt::format("gen_object_{}", i); },
        [](auto &, uint i) { return GenObjectDataTask(i);                });
    }

    void eval(SchedulerHandle &info) override {
      met_trace();

      // Get external resources
      const auto &e_scene   = info.global("scene").getr<Scene>();
      const auto &e_objects = e_scene.components.objects;

      // Adjust nr. of subtasks
      m_subtasks.eval(info, e_objects.size());
    }
  };
} // namespace met