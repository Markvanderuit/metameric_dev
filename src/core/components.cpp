#include <metameric/core/components.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/scene.hpp>
#include <tuple>

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

  bool View::operator==(const View &o) const {
    guard(film_size.isApprox(o.film_size), false);
    return std::tie(observer_i, camera_trf, camera_fov_y)
        == std::tie(o.observer_i, o.camera_trf, o.camera_fov_y);
  }
 
  bool Object::operator==(const Object &o) const {
    guard(std::tie(is_active, transform, mesh_i, uplifting_i) == 
          std::tie(o.is_active, o.transform, o.mesh_i, o.uplifting_i), false);
    /* guard(std::tie(roughness, metallic, opacity) == 
          std::tie(o.roughness, o.metallic, o.opacity), false); */
    guard(diffuse.index() == o.diffuse.index() /* && 
          normals.index() == o.normals.index() */, false);
    switch (diffuse.index()) {
      case 0: guard(std::get<Colr>(diffuse).isApprox(std::get<Colr>(o.diffuse)), false); break;
      case 1: guard(std::get<uint>(diffuse) == std::get<uint>(o.diffuse), false); break;
    }
    /* switch (normals.index()) {
      case 0: guard(std::get<Colr>(normals).isApprox(std::get<Colr>(o.normals)), false); break;
      case 1: guard(std::get<uint>(normals) == std::get<uint>(o.normals), false); break;
    } */
    return true;
  }

  bool Emitter::operator==(const Emitter &o) const {
    return std::tie(type, is_active, transform, illuminant_i, illuminant_scale) 
        == std::tie(o.type, o.is_active, o.transform, o.illuminant_i, o.illuminant_scale);
  }
  
  bool Uplifting::operator==(const Uplifting &o) const {
    return std::tie(observer_i, illuminant_i, basis_i)
      == std::tie(o.observer_i, o.illuminant_i, o.basis_i) 
      && rng::equal(verts, o.verts);
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

  std::vector<SurfaceInfo> Uplifting::Vertex::surfaces() const {
    met_trace();
    return constraint | visit {
      [](const DirectSurfaceConstraint &c) { 
        return std::vector<SurfaceInfo> { c.surface };
      },
      [](const IndirectSurfaceConstraint &c) { 
        return vws::zip(c.cstr_j, c.surfaces)
          | vws::filter([](const auto &p) { return std::get<0>(p).is_active; })
          | vws::values
          | rng::to<std::vector>();
      },
      [&](const auto &) { return std::vector<SurfaceInfo>(); }
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
                  | rng::to<std::vector>();
        cstr.push_back(std::pair { uplifting.observer_i, uplifting.illuminant_i });

        // Mismatching only occurs if there are two or more color systems, and all are unique
        return cstr.size() > 1 && !detail::has_duplicates(cstr);
      },
      [&](const DirectSurfaceConstraint &c) {
        // Merge all known color system data
        auto cstr = c.cstr_j
                  | vws::filter(&LinearConstraint::is_active)
                  | vws::transform([](const auto &v) { return std::pair { v.cmfs_j, v.illm_j }; })
                  | rng::to<std::vector>();
        cstr.push_back(std::pair { uplifting.observer_i, uplifting.illuminant_i });
        
        // Mismatching only occurs if there are two or more color systems, and all are unique
        return cstr.size() > 1 && !detail::has_duplicates(cstr);
      },
      [&](const IndirectSurfaceConstraint &c) {
        auto cstr = c.cstr_j
                  | vws::filter(&NLinearConstraint::is_active)
                  | rng::to<std::vector>();
        return !cstr.empty() 
            && !detail::has_duplicates(cstr) 
            && !cstr.back().powr_j.empty();
      },
      [&](const MeasurementConstraint &v) { 
        return false;
      }
    };
  }
} // namespace met