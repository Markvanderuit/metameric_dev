#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/record.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>

namespace met {
  class GenUpliftingDataTask : public detail::TaskNode {
    // Helper data for which tetrahedra go where, as in render data all meshes
    // are tightly packed into a single buffer
    struct MeshDataLayout {
      alignas(4) uint elem_offs; 
      alignas(4) uint elem_size;
    };

    // Packed wrapper data for tetrahedron barycentric test, used in gen_object_data
    // to quickly calculate barycentric coordinates for points inside a tetrahedron
    struct MeshPackLayout {
      eig::Matrix<float, 4, 3> inv; // Last column is padding
      eig::Matrix<float, 4, 1> sub; // Last value is padding
    };

    // Packed spectrum representation; four spectra interleaved per tetrahedron
    // ensure we can access all four spectra as one texture sample during rendering
    using SpecPackLayout = eig::Array<float, wavelength_samples, 4>;

    // Miscellaneous data
    uint              m_uplifting_i;
    std::vector<Colr> m_csys_boundary_samples;
    std::vector<Spec> m_csys_boundary_spectra;

    // Delaunay tesselation connecting colors/spectra on both
    // the boundary and interally in the color space, as well
    // as access to the packed gl-side data, which is used in
    // gen_objects_data to generate barycentric weights
    AlDelaunay                 m_tesselation;
    std::span<MeshPackLayout>  m_tesselation_pack_map;
    MeshDataLayout            *m_tesselation_data_map;

    // Color positions and corresponding assigned spectra in the 
    // delaunay tesselation
    std::vector<Colr> m_tesselation_points;
    std::vector<Spec> m_tesselation_spectra;

    // Buffer and accompanying mapping, store per-tetrahedron spectra
    // in an interleaved format. This data is copied to upliftings.gl
    // for fast sampled access during rendering
    gl::Buffer                m_buffer_spec_pack;
    std::span<SpecPackLayout> m_buffer_spec_pack_map;

    // Buffers for mesh data, if a accompanying viewer exists
    gl::Array  m_buffer_viewer_array;
    gl::Buffer m_buffer_viewer_verts;
    gl::Buffer m_buffer_viewer_elems;

  public:
    GenUpliftingDataTask(uint uplifting_i);

    bool is_active(SchedulerHandle &) override;
    void init(SchedulerHandle &)      override;
    void eval(SchedulerHandle &)      override;

  public:
    TetrahedronRecord query_tetrahedron(uint i) const;
  };

  class GenUpliftingsTask : public detail::TaskNode {
    detail::Subtasks<GenUpliftingDataTask> m_subtasks;

  public:
    void init(SchedulerHandle &info) override {
      met_trace();

      // Get external resources
      const auto &e_scene = info.global("scene").getr<Scene>();

      // Add subtasks of type GenUpliftingDataTask
      m_subtasks.init(info, e_scene.components.upliftings.size(), 
        [](uint i)         { return fmt::format("gen_uplifting_{}", i); },
        [](auto &, uint i) { return GenUpliftingDataTask(i);            });
    }

    void eval(SchedulerHandle &info) override {
      met_trace();

      // Get external resources
      const auto &e_scene = info.global("scene").getr<Scene>();

      // Adjust nr. of subtasks to current nr. of upliftings
      m_subtasks.eval(info, e_scene.components.upliftings.size());
    }
  };
} // namespace met