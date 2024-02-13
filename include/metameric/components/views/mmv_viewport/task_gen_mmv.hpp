#pragma once

#include <metameric/components/views/mesh_viewport/task_input_editor.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/scheduler.hpp>
#include <small_gl/buffer.hpp>
#include <unordered_set>

namespace met {
  class GenMMVTask : public detail::TaskNode  {
    using MMVPointSet = typename std::unordered_set<
      Colr, eig::detail::matrix_hash_t<Colr>, eig::detail::matrix_equal_t<Colr>
    >;

    InputSelection m_is;     // Active input selection
    uint           m_csys_j; // Visualized change-of-color-system
    MMVPointSet    m_points; // Cached, accumulated boundary points on mismatch volume
    AlMesh         m_chull;  // Current convex hull
    uint           m_iter;   // Current sampling iteration
    gl::Buffer     m_chull_verts;
    gl::Buffer     m_chull_elems;

  public:
    // Constructor; the task is specified for a specific,
    // selected constraint for now
    GenMMVTask(InputSelection is) : m_is(is) { }
    
    bool is_active(SchedulerHandle &info) override;
    void init(SchedulerHandle &info) override;
    void eval(SchedulerHandle &info) override;
  };
} // namespace met