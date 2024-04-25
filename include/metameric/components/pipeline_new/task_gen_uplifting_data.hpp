#pragma once

#include <metameric/core/distribution.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/record.hpp>
#include <metameric/core/convex.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <queue>
#include <unordered_set>
#include <unordered_map>

namespace met {
  constexpr static uint mmv_uplift_samples_max  = 256u;
  constexpr static uint mmv_uplift_samples_iter = 16u;

  // Helper struct to recover spectra by "rolling" mismatch volume generation, which
  // seems to be much simpler than solving for spectra directly, at least in the
  // indirect case.
  struct MismatchingConstraintBuilder {
    using map_type      = std::unordered_map<Colr, 
                                             Basis::vec_type, 
                                             eig::detail::matrix_hash_t<Colr>,
                                             eig::detail::matrix_equal_t<Colr>>;
    using set_colr_type = std::unordered_set<Colr, eig::detail::matrix_hash_t<Colr>, eig::detail::matrix_equal_t<Colr> >;
    using deq_colr_type = std::deque<Colr>;
    using deq_coef_type = std::deque<Basis::vec_type>;
    using cnstr_type    = typename Uplifting::Vertex::cnstr_type;
    
  private:
    set_colr_type m_colr_set      = { };                     // For tracking overlapping or collapsed volume samples
    deq_colr_type m_colr_deq      = { };                     // For tracking incoming and exiting samples' positions
    deq_coef_type m_coef_deq      = { };                     // For tracking incoming and exiting samples' coefficeints
    uint          m_seed          = 0;                       // For progressive sampling
    uint          m_curr_deq_size = 0;                       // For tracking how many points in the deque are "active"
    bool          m_did_increment = false;                   // Cache; did we increment?
    cnstr_type    m_cstr_cache    = DirectColorConstraint(); // Cache of vertex constraint, prevent unnecessary changes
    
  public:
    ConvexHull chull;

    std::tuple<Colr, Spec, Basis::vec_type> generate(const Uplifting::Vertex &vert, const Scene &scene, const Uplifting &uplifting) {
      met_trace();

      constexpr static uint hardcoded_csys_j = 0u;

      // Update convex hull samples
      if (vert.has_mismatching(scene, uplifting)) {
        if (needs_increment()) {
          increment(vert.realize_mismatching(scene, uplifting, hardcoded_csys_j, m_seed, mmv_uplift_samples_iter));
          m_seed += mmv_uplift_samples_iter;
          m_did_increment = true;
        } else {
          m_did_increment = false;
        }
      } else {
        clear_all();
      }

      if (chull.has_delaunay()) {
        // We use the convex hull to quickly find a metamer, instead of doing costly
        // nonlinear solver runs
        Colr p = vert.get_mismatching_position(hardcoded_csys_j);

        // Find the best enclosing simplex in the convex hull, and then find
        // the coefficients for that mismatching
        auto [bary, elem] = chull.find_enclosing_elem(p);
        auto coeffs       = elem | index_into_view(m_coef_deq) | rng::to<std::vector>();

        // Linear combination reconstructs coefficients for this metamer
        auto coef =(bary[0] * coeffs[0]
                  + bary[1] * coeffs[1]
                  + bary[2] * coeffs[2]
                  + bary[3] * coeffs[3]).cwiseMax(-1.f).cwiseMin(1.f).eval();
        auto spec = scene.resources.bases[uplifting.basis_i].value()(coef);
        auto colr = scene.csys(uplifting.csys_i)(spec);
        return { colr, spec, coef };
      } else {
        // Fall back; let vertex' underlying solver handle constraint, probably
        // outputting a default metamer that does not satisfy constraints. Either
        // there are no constraints, or the constraints conflict somehow
        return vert.realize(scene, uplifting);
      }
    }

    void increment(std::span<const std::tuple<Colr, Spec, Basis::vec_type>> new_data) {
      met_trace();
      
      m_colr_set.insert_range(new_data | vws::elements<0>);

      // If old, stale samples exist and need to be incrementally discarded,
      // figure out which parts to discard as new samples come in
      if (m_curr_deq_size > 0) {
        auto reduce_size = std::min({ static_cast<uint>(new_data.size()),
                                      static_cast<uint>(m_colr_deq.size()),
                                      m_curr_deq_size });
        m_curr_deq_size -= reduce_size;
        m_colr_deq.erase(m_colr_deq.begin(), m_colr_deq.begin() + reduce_size);
        m_coef_deq.erase(m_coef_deq.begin(), m_coef_deq.begin() + reduce_size);
      }
      m_colr_deq.append_range(new_data | vws::elements<0>);
      m_coef_deq.append_range(new_data | vws::elements<2>);

      // Determine extents of current point sets
      auto maxb = rng::fold_left_first(m_colr_deq, [](auto a, auto b) { return a.max(b).eval(); }).value();
      auto minb = rng::fold_left_first(m_colr_deq, [](auto a, auto b) { return a.min(b).eval(); }).value();

      // Minimum threshold for convex hull generation exceeds simplex size,
      // because QHull can throw a fit on small inputs
      if (m_colr_set.size() <= 6 || (maxb - minb).minCoeff() <= .005f) {
        chull = { };
      } else {
        chull = ConvexHull::build(std::vector(range_iter(m_colr_deq)));
      }
    }

    bool has_equal_mismatching(const Uplifting::Vertex &v) const {
      return v.has_equal_mismatching(m_cstr_cache, 0);
    }

    bool needs_increment() const {
      return m_colr_set.size() < mmv_uplift_samples_max;
    }
    
    bool did_increment() const {
      return m_did_increment;
    }
    
    void clear_increment(const Uplifting::Vertex &v) {
      m_cstr_cache    = v.constraint;
      m_seed          = 0;
      m_did_increment = true;
      m_curr_deq_size = m_colr_deq.size();
      m_colr_set.clear();
    }

    void clear_all() {
      m_seed          = 0;
      m_curr_deq_size = 0;
      chull           = { };
      m_did_increment = true;
      m_colr_set.clear();
      m_colr_deq.clear();
      m_coef_deq.clear();
    }
  };

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

    // Helper data for four coefficients vectors, describing the four spectra generated
    // for the four vertices of a tetrahedron. Used in gen_object_data to determine the
    // per-pixel coefficients on a parameterized texture over the object surface.
    using SpecCoefLayout = eig::Array<float, wavelength_bases, 4>;

    // Packed spectrum representation; four spectra interleaved per tetrahedron
    // ensure we can access all four spectra as one texture sample during rendering
    using SpecPackLayout  = eig::Array<float, wavelength_samples, 4>;

    std::vector<MismatchingConstraintBuilder> m_mismatch_builders;

    // Miscellaneous data
    uint                         m_uplifting_i;
    std::vector<Spec>            m_csys_boundary_spectra;
    std::vector<Basis::vec_type> m_csys_boundary_coeffs;
    // std::vector<Moments> m_csys_boundary_coeffs;

    // Delaunay tesselation connecting colors/spectra on both
    // the boundary and interally in the color space, as well
    // as access to the packed gl-side data, which is used in
    // gen_objects_data to generate barycentric weights
    AlDelaunay                m_tesselation;
    std::span<MeshPackLayout> m_tesselation_pack_map;
    MeshDataLayout           *m_tesselation_data_map;
    std::span<SpecCoefLayout> m_tesselation_coef_map;

    // Color positions, corresponding assigned spectra, and derived coefficients
    // in the delaunay tesselation
    std::vector<Colr>            m_tesselation_points;
    std::vector<Spec>            m_tesselation_spectra;
    std::vector<Basis::vec_type> m_tesselation_coeffs;
    // std::vector<Moments> m_tesselation_coeffs;

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
    // Accessors to some internal data; used by indirect surface constraints
    Spec              query_constraint(uint i)         const;
    TetrahedronRecord query_tetrahedron(uint i)        const;
    TetrahedronRecord query_tetrahedron(const Colr &c) const;
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