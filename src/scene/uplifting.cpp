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
    }
  } // namespace detail
} // namespace met
