#pragma once

#include <metameric/core/distribution.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/record.hpp>
#include <metameric/core/convex.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <queue>
#include <unordered_set>

namespace met {
  constexpr static uint mmv_uplift_samples_max  = 256u;
  constexpr static uint mmv_uplift_samples_iter = 16u;

  // Helper struct to recover spectra by "rolling window" mismatch volume generation. The resulting
  // convex structure is then used to construct interior spectra through linear interpolation. 
  // This is much faster than solving for metamers directly, if the user is going to edit constraints.
  struct MetamerConstraintBuilder {
    ConvexHull chull; // Convex hull data is exposed for UI components to use
  
  private:
    using cnstr_type  = typename Uplifting::Vertex::cnstr_type;

    bool                        m_did_sample    = false;                   // Cache; did we generate samples this iteration?
    std::deque<Colr>            m_colr_samples  = { };                     // For tracking incoming and exiting samples' positions
    std::deque<Basis::vec_type> m_coef_samples  = { };                     // For tracking incoming and exiting samples' coefficeints
    uint                        m_curr_samples  = 0;                       // How many samples are of the current vertex constraint
    uint                        m_prev_samples  = 0;                       // How many samples are of an old vertex constriant
    cnstr_type                  m_cstr_cache    = DirectColorConstraint(); // Cache of current vertex constraint, to detect mismatch volume change

    void insert(std::span<const MismatchSample> samples) {
      met_trace();
      // If old samples exist, these need to be incrementally discarded,
      // figure out which parts to discard at the front before adding new samples
      if (m_prev_samples > 0) {
        auto reduce_size = std::min({ static_cast<uint>(samples.size()),
                                      static_cast<uint>(m_colr_samples.size()),
                                      m_prev_samples });
        m_prev_samples -= reduce_size;
        m_colr_samples.erase(m_colr_samples.begin(), m_colr_samples.begin() + reduce_size);
        m_coef_samples.erase(m_coef_samples.begin(), m_coef_samples.begin() + reduce_size);
      }

      // Add new samples to the end of the queue
      m_colr_samples.append_range(samples | vws::elements<0>);
      m_coef_samples.append_range(samples | vws::elements<2>);

      // Determine extents of current full point set
      auto maxb = rng::fold_left_first(m_colr_samples, [](auto a, auto b) { return a.max(b).eval(); }).value();
      auto minb = rng::fold_left_first(m_colr_samples, [](auto a, auto b) { return a.min(b).eval(); }).value();

      // Minimum threshold for convex hull generation exceeds simplex size,
      // because QHull can throw a fit on small inputs
      // if (m_colr_samples.size() >= 6 && (maxb - minb).minCoeff() > .005f) {
      if (m_colr_samples.size() >= 6 && (maxb - minb).minCoeff() > .0005f) {
        chull = {{ .data = m_colr_samples | rng::to<std::vector>() }};
      } else {
        chull = { };
      }
    }

  public: // Public methods
    // Generate a spectrum and matching color in the uplifting's color system
    MismatchSample realize(const Uplifting::Vertex &vert, const Scene &scene, const Uplifting &uplifting) {
      met_trace();

      // Update convex hull samples, or discard them if mismatching is not possible
      if (vert.has_mismatching(scene, uplifting)) {
        if (m_did_sample = !is_converged(); m_did_sample) {
          auto samples = vert.realize_mismatch(scene, uplifting, m_curr_samples, mmv_uplift_samples_iter);
          insert(samples);
          m_curr_samples += mmv_uplift_samples_iter;
        }
      } else {
        // If mismatching is not possible, clear internal state entirely
        chull          = { };
        m_curr_samples = 0;
        m_prev_samples = 0;
        m_did_sample   = true;
        m_colr_samples.clear();
        m_coef_samples.clear();
      }

      // Return zero constraint for inactive vertices
      guard(vert.is_active, { Colr(0), Spec(0), Basis::vec_type(0) });
      
      // If a mismatch volume exists
      if (chull.has_delaunay()) {
        // We use the convex hull to quickly find a metamer, instead of doing costly
        // nonlinear solver runs. Find the best enclosing simplex, and then mix the
        // attached coefficients to generate a spectrum at said position
        auto [bary, elem] = chull.find_enclosing_elem(vert.get_mismatch_position());
        auto coeffs       = elem | index_into_view(m_coef_samples) | rng::to<std::vector>();

        // Linear combination reconstructs coefficients for this metamer
        auto coef =(bary[0] * coeffs[0] + bary[1] * coeffs[1]
                  + bary[2] * coeffs[2] + bary[3] * coeffs[3]).cwiseMax(-1.f).cwiseMin(1.f).eval();
        auto spec = scene.resources.bases[uplifting.basis_i].value()(coef);
        auto colr = vert.is_position_shifting()
                  ? scene.csys(uplifting)(spec)
                  : vert.get_vertex_position();

        // Return all three
        return { colr, spec, coef };
      } else {
        // Fallback; let a solver handle the constraint, potentially
        // outputting a metamer that does not satisfy all constraints. Either
        // there are no constraints, or the constraints conflict somehow
        return vert.realize(scene, uplifting);
      }
    }

    // Does the builder need to do any sampling work still? Otherwise, generate() just spits out the previous result
    bool is_converged() const {
      return (chull.deln.verts.size() - m_prev_samples) >= mmv_uplift_samples_max;
    }
    
    // Did generate() do sampling, thereby making changes
    bool did_sample() const {
      return m_did_sample;
    }

    // Does the underlying cached constraint match that of the current vertex, w.r.t. a generated mismatch region?
    bool matches_vertex(const Uplifting::Vertex &v) const {
      return v.has_equal_mismatching(m_cstr_cache);
    }
    
    // Set the underlying cached constriant that a mismatch region is built for. Resets sampling.
    void assign_vertex(const Uplifting::Vertex &v) {
      m_cstr_cache    = v.constraint;
      m_curr_samples  = 0;
      m_did_sample    = true;
      m_prev_samples  = m_colr_samples.size();
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

    std::vector<MetamerConstraintBuilder> m_mismatch_builders;

    // Miscellaneous data
    uint                         m_uplifting_i;
    std::vector<Spec>            m_csys_boundary_spectra;
    std::vector<Basis::vec_type> m_csys_boundary_coeffs;

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