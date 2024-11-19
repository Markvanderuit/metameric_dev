#include <metameric/scene/scene.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/ranges.hpp>

namespace met {
  namespace detail {
    // Helper for surface() calls; invalid return for non-surface constraints
    static SurfaceInfo invalid_visitor_return_si = SurfaceInfo::invalid();

    bool has_duplicates(const rng::range auto &r) {
      for (auto i = r.begin(); i != r.end(); ++i)
        for (auto j = i + 1; j != r.end(); ++j) {
          if (*i == *j)
            return true;
        }
      return false;
    }
  } // namespace detail
  
  bool Uplifting::operator==(const Uplifting &o) const {
    return std::tie(observer_i, illuminant_i, basis_i)
      == std::tie(o.observer_i, o.illuminant_i, o.basis_i) 
      && rng::equal(verts, o.verts);
  }
  
  std::vector<MismatchSample> Uplifting::sample_color_solid(const Scene &scene, uint seed, uint n) const {
    met_trace();
    // Assemble color system data, then forward to metamer.hpp to 
    // generate n points on color system boundary
    ColrSystem csys = { .cmfs       = *scene.resources.observers[observer_i],
                        .illuminant = *scene.resources.illuminants[illuminant_i] };
    return solve_color_solid({ .direct_objective = csys,
                               .basis            = *scene.resources.bases[basis_i],
                               .seed             = seed,
                               .n_samples        = n });
  }

  MismatchSample Uplifting::Vertex::realize(const Scene &scene, const Uplifting &uplifting) const {
    met_trace();

    // Return zero constraint for inactive vertices
    guard(is_active, { Colr(0), Spec(0), Basis::vec_type(0) });
    
    // Visit the underlying constraint to generate output data
    return constraint | visit([&](const auto &cstr) -> MismatchSample { 
      auto [s, c] = cstr.realize(scene, uplifting);
      auto p = is_position_shifting()
             ? scene.csys(uplifting)(s)
             : get_vertex_position();
      return { p, s, c }; 
    });
  }

  std::vector<MismatchSample> Uplifting::Vertex::realize_mismatch(const Scene     &scene, 
                                                                  const Uplifting &uplifting,
                                                                        uint       seed,
                                                                        uint       samples) const {
    met_trace();

    // Return zero constraint for inactive vertices or those without mismatching
    guard(has_mismatching(scene, uplifting), { });

    // Otherwise, visit the underlying constraint to generate output data
    return constraint | visit([&](const auto &cstr) { 
      return cstr.realize_mismatch(scene, uplifting, seed, samples); 
    });
  }

  bool Uplifting::Vertex::is_position_shifting() const {
    met_trace();
    return constraint | visit {
      [](const is_roundtrip_constraint auto &cstr) { 
        return cstr.is_base_active;
      },
      [](const auto &) { return true; }
    };
  }

  Colr Uplifting::Vertex::get_vertex_position() const {
    met_trace();
    return constraint | visit {
      [](const is_roundtrip_constraint auto &cstr) { 
        return cstr.colr_i;
      },
      [](const auto &) { return Colr(0); }
    };
  }

  void Uplifting::Vertex::set_mismatch_position(const Colr &c) {
    met_trace();
    constraint | visit { 
      [c](is_linear_constraint auto &cstr) { 
        cstr.cstr_j.back().colr_j = c; 
      }, 
      [c](is_nlinear_constraint auto &cstr) { 
        cstr.cstr_j.back().colr_j = c;
      },
      [](const auto &cstr) {}
    };
  }

  Colr Uplifting::Vertex::get_mismatch_position() const {
    met_trace();
    return constraint | visit {
      [](const is_linear_constraint auto &cstr) { 
        guard(!cstr.cstr_j.empty(), Colr(0));
        return (cstr.cstr_j | vws::filter(&LinearConstraint::is_active)).back().colr_j; 
      },
      [](const is_nlinear_constraint auto &cstr) { 
        guard(!cstr.cstr_j.empty(), Colr(0));
        return cstr.cstr_j.back().colr_j; 
      },
      [](const auto &) { return Colr(0); },
    };
  }
  
  bool Uplifting::Vertex::has_equal_mismatching(const cnstr_type &other_v) const {
    met_trace();
    guard(constraint.index() == other_v.index(), false);
    return constraint | visit {
      [&](const DirectColorConstraint &cstr) {
        const auto &other = std::get<std::decay_t<decltype(cstr)>>(other_v);

        guard(cstr.colr_i.isApprox(other.colr_i), false);
        guard(cstr.is_base_active == other.is_base_active, false);
        guard(cstr.cstr_j.size() == other.cstr_j.size(), false);
        
        if (!cstr.cstr_j.empty()) {
          // The "known" connstraints should be identical
          guard(rng::equal(cstr.cstr_j  | vws::filter(&LinearConstraint::is_active) | vws::take(cstr.cstr_j.size() - 1),
                           other.cstr_j | vws::filter(&LinearConstraint::is_active) | vws::take(cstr.cstr_j.size() - 1)), false);
          
          // The "free variable" should be identical outside of the specified color value
          guard(cstr.cstr_j.back().is_similar(other.cstr_j.back()), false);
        }
        return true; // only the free variable differs
      },
      [&](const DirectSurfaceConstraint &cstr) {
        const auto &other = std::get<std::decay_t<decltype(cstr)>>(other_v);

        guard(cstr.is_base_active == other.is_base_active, false);
        guard(cstr.colr_i.isApprox(other.colr_i), false);
        guard(cstr.cstr_j.size() == other.cstr_j.size(), false);

        if (!cstr.cstr_j.empty()) {
          // The "known" connstraints should be identical
          guard(rng::equal(cstr.cstr_j  | vws::filter(&LinearConstraint::is_active) | vws::take(cstr.cstr_j.size() - 1),
                           other.cstr_j | vws::filter(&LinearConstraint::is_active) | vws::take(cstr.cstr_j.size() - 1)), false);
          
          // The "free variable" constraint should be identical outside of the specified color value
          guard(cstr.cstr_j.back().is_similar(other.cstr_j.back()), false);
        }
        return true; // only the free variable differs
      },
      [&](const IndirectSurfaceConstraint &cstr) {
        const auto &other = std::get<std::decay_t<decltype(cstr)>>(other_v);
        
        guard(cstr.is_base_active == other.is_base_active, false);
        guard(cstr.colr_i.isApprox(other.colr_i), false);
        guard(cstr.cstr_j.size() == other.cstr_j.size(), false);
        
        if (!cstr.cstr_j.empty()) {
          // The "known" connstraints should be identical
          guard(rng::equal(cstr.cstr_j  | vws::filter(&NLinearConstraint::is_active) | vws::take(cstr.cstr_j.size() - 1),
                           other.cstr_j | vws::filter(&NLinearConstraint::is_active) | vws::take(cstr.cstr_j.size() - 1)), false);

          // The "free variable" constraint should be identical outside of the specified color value
          guard(cstr.cstr_j.back().is_similar(other.cstr_j.back()), false);
        }
        return true; // only the free variable differs
      },
      [](const auto &) { return true; },
    };
  }

  bool Uplifting::Vertex::has_mismatching(const Scene &scene, const Uplifting &uplifting) const {
    met_trace();
    return constraint | visit { 
      [&](const DirectColorConstraint &c) {
        // Merge all known color system data
        auto cstr = c.cstr_j
                  | vws::filter(&LinearConstraint::is_active)
                  | vws::transform([](const auto &v) { return std::pair { v.cmfs_j, v.illm_j }; })
                  | view_to<std::vector<std::pair<uint, uint>>>();
        cstr.push_back(std::pair { uplifting.observer_i, uplifting.illuminant_i });

        // Mismatching only occurs if there are two or more color systems, and all are unique
        return cstr.size() > 1 && !detail::has_duplicates(cstr);
      },
      [&](const DirectSurfaceConstraint &c) {
        // Merge all known color system data
        auto cstr = c.cstr_j
                  | vws::filter(&LinearConstraint::is_active)
                  | vws::transform([](const auto &v) { return std::pair { v.cmfs_j, v.illm_j }; })
                  | view_to<std::vector<std::pair<uint, uint>>>();
        cstr.push_back(std::pair { uplifting.observer_i, uplifting.illuminant_i });
        
        // Mismatching only occurs if there are two or more color systems, and all are unique
        return cstr.size() > 1 && !detail::has_duplicates(cstr);
      },
      [&](const IndirectSurfaceConstraint &c) {
        auto cstr = c.cstr_j
                  | vws::filter(&NLinearConstraint::is_active)
                  | view_to<std::vector<NLinearConstraint>>();
        return !cstr.empty() 
            && !detail::has_duplicates(cstr) 
            && !cstr.back().powr_j.empty();
      },
      [&](const MeasurementConstraint &v) { 
        return false;
      }
    };
  }

  bool Uplifting::Vertex::has_surface() const {
    met_trace();
    return constraint | visit {
      [](const DirectSurfaceConstraint &) {
        return true;
      },
      [](const IndirectSurfaceConstraint &cstr) { 
        return !cstr.cstr_j.empty(); 
      },
      [](const auto &) { 
        return false;
      },
    };
  }
  
  void Uplifting::Vertex::set_surface(const SurfaceInfo &si) {
    met_trace();
    constraint | visit {
      [si](DirectSurfaceConstraint &cstr) { 
        cstr.surface = si;
        cstr.colr_i  = si.diffuse;
      },
      [si](IndirectSurfaceConstraint &cstr) { 
        guard(!cstr.cstr_j.empty());
        cstr.surfaces.back() = si;
        if (cstr.cstr_j.size() == 1)
          cstr.colr_i = si.diffuse;
      },
      [&](auto &) { /* ... */ }
    };
  }

  const SurfaceInfo &Uplifting::Vertex::surface() const {
    met_trace();
    return constraint | visit {
      [](const DirectSurfaceConstraint &c)   -> const SurfaceInfo & { 
        return c.surface; 
      },
      [](const IndirectSurfaceConstraint &c) -> const SurfaceInfo & { 
        return !c.surfaces.empty() 
          ? c.surfaces.back() 
          : detail::invalid_visitor_return_si; 
      },
      [&](const auto &) -> const SurfaceInfo & { return detail::invalid_visitor_return_si; }
    };
  }

  std::span<const SurfaceInfo> Uplifting::Vertex::surfaces() const {
    met_trace();
    return constraint | visit {
      [](const DirectSurfaceConstraint &c) { 
        return std::span<const SurfaceInfo> { &c.surface, 1 };
      },
      [](const IndirectSurfaceConstraint &c) { 
        return std::span<const SurfaceInfo>(c.surfaces);
      },
      [&](const auto &) { return std::span<const SurfaceInfo>(); }
    };
  }

  namespace detail {
    SceneGLHandler<met::Uplifting>::SceneGLHandler() {
      met_trace_full();
      texture_coef  = {{ .levels  = 1, .padding = 0 }};
      texture_brdf  = {{ .levels  = 1, .padding = 0 }};
      texture_basis = {{ .size = { wavelength_samples, wavelength_bases } }};
    }
        
    void SceneGLHandler<met::Uplifting>::update(const Scene &scene) {
      met_trace_full();

      // Get relevant resources
      const auto &e_objects    = scene.components.objects;
      const auto &e_upliftings = scene.components.upliftings;
      const auto &e_images     = scene.resources.images;
      const auto &e_settings   = scene.components.settings;
      
      // Flag that the atlas' internal texture has **not** been invalidated by internal resize
      e_upliftings.gl.texture_coef.set_invalitated(false);
      e_upliftings.gl.texture_brdf.set_invalitated(false);

      // Only rebuild if there are upliftings and objects
      guard(!e_upliftings.empty() && !e_upliftings.empty());
      guard(e_upliftings                                        || 
            e_objects                                           ||
            e_settings.state.texture_size                       ||
            !e_upliftings.gl.texture_coef.texture().is_init()   ||
            !e_upliftings.gl.texture_coef.buffer().is_init()    );
      
      // Check for texture_coef resize
      {
        // Gather necessary texture sizes for each object
        // If the texture index was specified, we insert the texture size as an input
        // for the atlas. If a color was specified, we allocate a small patch
        std::vector<eig::Array2u> inputs(e_objects.size());
        rng::transform(e_objects, inputs.begin(), [&](const auto &object) -> eig::Array2u {
          return object->diffuse | visit {
            [&](uint i) { return e_images[i]->size(); },
            [&](Colr f) { return eig::Array2u { 16, 16 }; },
          };
        });

        // Scale atlas inputs to respect the maximum texture size set in Settings::texture_size
        eig::Array2u maximal = rng::fold_left(inputs, eig::Array2u(0), [](auto a, auto b) { return a.cwiseMax(b).eval(); });
        eig::Array2f scaled  = e_settings->apply_texture_size(maximal).cast<float>() / maximal.cast<float>();
        for (auto &input : inputs)
          input = (input.cast<float>() * scaled).max(2.f).cast<uint>().eval();

        // Regenerate atlas if inputs don't match the atlas' current layout
        // Note; barycentric weights will need a full rebuild, which is detected
        //       by the nr. of objects changing or the texture setting changing. A bit spaghetti-y :S
        texture_coef.resize(inputs);
        if (texture_coef.is_invalitated()) {
          // The barycentric texture was re-allocated, which means underlying memory was all invalidated.
          // So in a case of really bad spaghetti-code, we force object-dependent parts to update
          auto &e_scene = const_cast<Scene &>(scene);
          e_scene.components.objects.set_mutated(true);
        }
      }

      // Check for texture_brdf resize
      {
        // Gather necessary texture sizes for each object
        // If the texture index was specified, we insert the texture size as an input
        // for the atlas. If a value was specified, we allocate a small patch
        // As we'd like to cram multiple brdf values into a single lookup, we will
        // take the larger size for each patch. Makes baking slightly more expensive
        std::vector<eig::Array2u> inputs(e_objects.size());
        rng::transform(e_objects, inputs.begin(), [&](const auto &object) -> eig::Array2u {
          auto metallic_size = object->metallic | visit {
            [&](uint  i) { return e_images[i]->size(); },
            [&](float f) { return eig::Array2u { 16, 16 }; },
          };
          auto roughness_size = object->roughness | visit {
            [&](uint  i) { return e_images[i]->size(); },
            [&](float f) { return eig::Array2u { 16, 16 }; },
          };
          return metallic_size.cwiseMax(roughness_size).eval();
        });

        // Scale atlas inputs to respect the maximum texture size set in Settings::texture_size
        eig::Array2u maximal = rng::fold_left(inputs, eig::Array2u(0), [](auto a, auto b) { return a.cwiseMax(b).eval(); });
        eig::Array2f scaled  = e_settings->apply_texture_size(maximal).cast<float>() / maximal.cast<float>();
        for (auto &input : inputs)
          input = (input.cast<float>() * scaled).max(2.f).cast<uint>().eval();
        
        // Regenerate atlas if inputs don't match the atlas' current layout
        // Note; barycentric weights will need a full rebuild, which is detected
        //       by the nr. of objects changing or the texture setting changing. A bit spaghetti-y :S
        texture_brdf.resize(inputs);
        if (texture_brdf.is_invalitated()) {
          // The barycentric texture was re-allocated, which means underlying memory was all invalidated.
          // So in a case of really bad spaghetti-code, we force object-dependent parts to update
          auto &e_scene = const_cast<Scene &>(scene);
          e_scene.components.objects.set_mutated(true);
        }
      }

      // Push basis function data, just defaulted to basis 0 for now
      {
        const auto &basis = scene.resources.bases[0].value();
        texture_basis.set(obj_span<const float>(basis.func));
      }

      // Adjust nr of UpliftingData and ObjectData blocks up to or down to relevant size
      for (uint i = uplifting_data.size(); i < scene.components.upliftings.size(); ++i)
        uplifting_data.push_back(i);
      for (uint i = uplifting_data.size(); i > scene.components.upliftings.size(); --i)
        uplifting_data.pop_back();
      for (uint i = object_data.size(); i < scene.components.objects.size(); ++i)
        object_data.push_back(i);
      for (uint i = object_data.size(); i > scene.components.objects.size(); --i)
        object_data.pop_back();

      // Generate spectral uplifting data and per-object spectral texture
      for (auto &data : uplifting_data)
        data.update(scene);
      for (auto &data : object_data)
        data.update(scene);
    }

    using MetamerBuilder = SceneGLHandler<met::Uplifting>::MetamerBuilder;

    void MetamerBuilder::insert_samples(std::span<const MismatchSample> new_samples) {
      met_trace();
      
      // If old samples exist, these are incrementally discarded,
      // figure out which parts to discard at the front before adding new samples
      if (m_samples_prev > 0) {
        auto reduce_size = std::min({ static_cast<uint>(new_samples.size()),
                                      static_cast<uint>(m_samples.size()),
                                      m_samples_prev });
        m_samples_prev -= reduce_size;
        m_samples.erase(m_samples.begin(), m_samples.begin() + reduce_size);
      }

      // Add new samples to the end of the queue
      rng::copy(new_samples, std::back_inserter(m_samples));

      // Extract point data into range, and determine AABB of this full point set
      auto points = m_samples | vws::transform(&MismatchSample::colr) | view_to<std::vector<Colr>>();
      auto maxb   = rng::fold_left_first(points, [](auto a, auto b) { return a.max(b).eval(); }).value();
      auto minb   = rng::fold_left_first(points, [](auto a, auto b) { return a.min(b).eval(); }).value();

      // Minimum threshold for convex hull generation exceeds simplex size,
      // because QHull can throw a fit on small inputs
      // if (m_colr_samples.size() >= 6 && (maxb - minb).minCoeff() > .005f) {
      if (m_samples.size() >= 6 && (maxb - minb).minCoeff() > .0005f) {
        hull = {{ .data = points }};
      } else {
        hull = { };
      }
    }

    MismatchSample MetamerBuilder::realize(const Scene &scene, uint uplifting_i, uint vertex_i) {
      met_trace();

      // Get handles
      const auto &uplifting = scene.components.upliftings[uplifting_i];
      const auto &vert      = uplifting->verts[vertex_i];

      // Return dead data if the vertex is inactive
      guard(vert.is_active, MismatchSample { Colr(0), Spec(0), Basis::vec_type(0) });

      // First, deal with new mismatch samples
      if (vert.has_mismatching(scene, *uplifting)) {
        // Vertex data supports metamer mismatching;
        // then, if the builder is not converged, generate new samples
        if (m_did_sample = !is_converged(); m_did_sample) {
          auto new_samples = vert.realize_mismatch(scene, *uplifting, m_samples_curr, n_uplifting_mismatch_samples_iter);
          insert_samples(new_samples);
          m_samples_curr += new_samples.size();
        }
      } else {
        // Vertex data does not support metamer mismatching; 
        // clear internal state entirely as the builder should play dead
        hull = { };
        m_samples.clear();
        m_samples_curr = 0;
        m_samples_prev = 0;
        m_did_sample   = true;
      }

      // Next, deal with generating a spectral output
      if (hull.has_delaunay()) {
        // We use the convex hull to quickly find a metamer, instead of doing costly
        // solver runs. Find the best enclosing simplex, and then mix the
        // attached coefficients to generate a spectrum at said position
        auto [bary, elem] = hull.find_enclosing_elem(vert.get_mismatch_position());
        auto coeffs       = elem 
                          | index_into_view(m_samples) 
                          | vws::transform(&MismatchSample::coef)
                          | view_to<std::vector<Basis::vec_type>>();

        // Linear combination reconstructs coefficients for this metamer
        auto coef =(bary[0] * coeffs[0] + bary[1] * coeffs[1]
                  + bary[2] * coeffs[2] + bary[3] * coeffs[3]).cwiseMax(-1.f).cwiseMin(1.f).eval();
        auto spec = scene.resources.bases[uplifting->basis_i].value()(coef);
        auto colr = vert.is_position_shifting()
                  ? scene.csys(*uplifting)(spec)
                  : vert.get_vertex_position();

        // Return all three
        return MismatchSample { colr, spec, coef };
      } else {
        // Fallback; let a solver handle the constraint, potentially
        // outputting a metamer that does not satisfy all constraints. Either
        // there are no constraints, or the constraints conflict somehow
        return vert.realize(scene, *uplifting);
      }
    }

    bool MetamerBuilder::supports_vertex(const Scene &scene, uint uplifting_i, uint vertex_i) {
      met_trace();

      // Forward to vertex
      const auto &uplifting = scene.components.upliftings[uplifting_i];
      return uplifting->verts[vertex_i].has_equal_mismatching(m_cnstr_cache);
    }
    
    void MetamerBuilder::set_vertex(const Scene &scene, uint uplifting_i, uint vertex_i) {
      met_trace();
      
      // Reset the cache for the new vertex
      const auto &uplifting = scene.components.upliftings[uplifting_i];
      m_cnstr_cache  = uplifting->verts[vertex_i].constraint;
      m_samples_prev = m_samples.size();
      m_samples_curr = 0;
      m_did_sample   = true;
    }

    SceneGLHandler<met::Uplifting>::UpliftingData::UpliftingData(uint uplifting_i)
    : m_uplifting_i(uplifting_i), m_is_first_update(true) {
      met_trace_full();

      // Instantiate mapped buffer objects; these'll hold packed barycentric and spectral coefficient
      // data, which is used by ObjectData::update below to bake spectral textures per object
      std::tie(buffer_bary, m_buffer_bary_map) = gl::Buffer::make_flusheable_object<BufferBaryLayout>();
      std::tie(buffer_coef, m_buffer_coef_map) = gl::Buffer::make_flusheable_object<BufferCoefLayout>();
    }

    void SceneGLHandler<met::Uplifting>::UpliftingData::update(const Scene &scene) {
      met_trace_full();

      // Get handles to uplifting and linked resources
      const auto &uplifting  = scene.components.upliftings[m_uplifting_i];
      const auto &basis      = scene.resources.bases[uplifting->basis_i];
      const auto &observer   = scene.resources.observers[uplifting->observer_i];
      const auto &illuminant = scene.resources.illuminants[uplifting->illuminant_i];
      
      // Flag 1; test if color system has, in any way/shape/form, been modified
      bool is_color_system_stale = m_is_first_update
        || uplifting->basis_i      || basis
        || uplifting->observer_i   || observer
        || uplifting->illuminant_i || illuminant;
      
      // Flag 2; test if the tessellation has, in any way/shape/form, been modified;
      //         note that later steps can set this to true if necessary
      bool is_tessellation_stale = m_is_first_update
        || uplifting.state.verts.is_resized() || is_color_system_stale;
      
      // Flag 3; test if a spectrum was changed; set to true if necessary
      bool is_spectrum_stale = m_is_first_update;

      // Step 1; generate a color system boundary; spectra, coefficients, and colors
      if (is_color_system_stale) {
        boundary = uplifting->sample_color_solid(scene, 4, n_uplifting_boundary_samples);
        fmt::print("Uplifting {} sampled {} color system boundary points\n", m_uplifting_i, boundary.size());
      }

      // Step 2; generate the interior spectra, coefficients, and colors for uplifting constraints.
      //         We rely on MetamerBuilder, which gives us both a boundary for the user
      //         in the UI, and simple interpolated interior spectra.
      {
        // Ensure the right data is present
        metamer_builders.resize(uplifting->verts.size());
        interior.resize(uplifting->verts.size());

        // Iterate interior vertioces
        for (int i = 0; i < uplifting->verts.size(); ++i) {
          auto &builder = metamer_builders[i];

          // Test if the builder is due for a reset; either the color system changed, or the
          // vertex did in some important way
          if (is_color_system_stale || !builder.supports_vertex(scene, m_uplifting_i, i))
            builder.set_vertex(scene, m_uplifting_i, i);

          // If the builder has already converged, or the vertex wasn't even touched; exit early
          guard_continue(!builder.is_converged() || uplifting.state.verts[i]);
          is_spectrum_stale = true;

          // Generate a new sample from the builder
          auto new_sample = builder.realize(scene, m_uplifting_i, i);

          // Check if the color output of this sample is different from the previous sample;
          // if so, we denote the tessellation as stale
          auto old_sample = interior[i];
          interior[i] = new_sample;
          if (!old_sample.colr.isApprox(new_sample.colr))
            is_tessellation_stale = true;
        } // for (int i)
      }

      // Step 3; merge boundary and interior spectra, and over this generate an R^3 delaunay tessellation
      boundary_and_interior.resize(boundary.size() + interior.size());
      rng::copy(boundary, boundary_and_interior.begin());
      rng::copy(interior, boundary_and_interior.begin() + boundary.size());
      if (is_tessellation_stale) {
        auto points = boundary_and_interior | vws::transform(&MismatchSample::colr) | view_to<std::vector<Colr>>();
        tessellation = generate_delaunay<AlDelaunay, Colr>(points);
      }

      // Step 4; update GL-side packed data for ObjectData::update() to use later on
      if (is_color_system_stale || is_tessellation_stale || is_spectrum_stale) {
        // Updated buffer size values to the nr. of tetrahedra
        m_buffer_bary_map->size = tessellation.elems.size();
        m_buffer_coef_map->size = tessellation.elems.size();

        // Per tetrahedron, emplace packed matrix representation of vertex barycentric weights
        std::transform(
          std::execution::par_unseq,
          range_iter(tessellation.elems),
          m_buffer_bary_map->data.begin(),
          [&](const eig::Array4u &el) {
            const auto vts = el | index_into_view(tessellation.verts);
            BufferBaryBlock block;
            block.inv.block<3, 3>(0, 0) = (eig::Matrix3f() 
              << vts[0] - vts[3], vts[1] - vts[3], vts[2] - vts[3]
            ).finished().inverse();
            block.sub.head<3>() = vts[3];
            return block;
          });
        
        // Per tetrahedron, emplace packed spectral coefficients of vertex spectra
        std::transform(
          std::execution::par_unseq,
          range_iter(tessellation.elems),
          m_buffer_coef_map->data.begin(),
          [&](const eig::Array4u &el) {
            BufferCoefBlock block;
            for (uint i = 0; i < 4; ++i)
              block.col(i) = boundary_and_interior[el[i]].coef;
            return block;
          });
        
        // Flush buffer up to relevant used range
        buffer_bary.flush(sizeof(uint) + sizeof(BufferBaryBlock) * tessellation.elems.size());
        buffer_coef.flush(sizeof(uint) + sizeof(BufferCoefBlock) * tessellation.elems.size());
      }
    }

    SceneGLHandler<met::Uplifting>::ObjectData::ObjectData(uint object_i)
    : m_object_i(object_i) {
      met_trace();
      // ...
    }

    void SceneGLHandler<met::Uplifting>::ObjectData::update(const Scene &scene) {
      // ...
    }
  } // namespace detail
} // namespace met
