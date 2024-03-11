#pragma once

#include <metameric/components/views/scene_viewport/task_input_editor.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/scheduler.hpp>
#include <small_gl/buffer.hpp>
#include <unordered_set>

namespace met {
  class GenMMVTask : public detail::TaskNode  {
    // Internal unordered_set for storing unique convex hull points
    // as not all generated points may be strictly unique
    using MMVPointSet = typename std::unordered_set<
      Colr, eig::detail::matrix_hash_t<Colr>, eig::detail::matrix_equal_t<Colr>
    >;

    uint           m_csys_j; // Visualized change-of-color-system
    MMVPointSet    m_points; // Cached, accumulated boundary points on mismatch volume
    AlMesh         m_chull;  // Current convex hull
    uint           m_iter;   // Current sampling iteration
    gl::Buffer     m_chull_verts;
    gl::Buffer     m_chull_elems;
    gl::Buffer     m_points_verts;

  public:
    bool is_active(SchedulerHandle &info) override;
    void init(SchedulerHandle &info) override;
    void eval(SchedulerHandle &info) override;
  };
} // namespace met