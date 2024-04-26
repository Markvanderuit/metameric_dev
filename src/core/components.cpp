#include <metameric/core/components.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/matching.hpp>
#include <metameric/core/scene.hpp>

namespace met {
  namespace detail {
    // Helper for surface() calls; invalid return for non-surface constraints
    static SurfaceInfo invalid_visitor_return_si = SurfaceInfo::invalid();
  } // namespace detail
 
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
    return std::tie(csys_i, basis_i) == std::tie(o.csys_i, o.basis_i) && rng::equal(verts, o.verts);
  }

  MismatchSample Uplifting::Vertex::realize(const Scene &scene, const Uplifting &uplifting) const {
    met_trace();

    // Return zero constraint for inactive vertices
    guard(is_active, { Colr(0), Spec(0), Basis::vec_type(0) });
    
    // Visit the underlying constraint to generate output data
    return constraint | visit([&](const auto &cstr) -> MismatchSample { 
      auto [s, c] = cstr.realize(scene, uplifting);
      return { cstr.position(scene, uplifting), s, c  }; 
    });
  }

  std::vector<MismatchSample> Uplifting::Vertex::realize_mismatch(const Scene     &scene, 
                                                                                              const Uplifting &uplifting,
                                                                                              uint seed,
                                                                                              uint samples) const {
    met_trace();

    // Return zero constraint for inactive vertices or those without mismatching
    guard(is_active && has_mismatching(scene, uplifting), { });

    // Otherwise, visit the underlying constraint to generate output data
    return constraint | visit([&](const auto &cstr) { return cstr.realize_mismatch(scene, uplifting, seed, samples); });
  }

  void Uplifting::Vertex::set_mismatch_position(const Colr &c) {
    met_trace();
    constraint | visit { 
      [c](is_colr_constraint auto &cstr)   { cstr.cstr_j.back().colr_j = c; }, 
      [c](IndirectSurfaceConstraint &cstr) { cstr.colr = c; },
      [](const auto &cstr) {}
    };
  }

  Colr Uplifting::Vertex::get_mismatch_position() const {
    met_trace();
    return constraint | visit {
      [](const is_colr_constraint auto &v)   { return v.cstr_j.back().colr_j; },
      [](const IndirectSurfaceConstraint &v) { return v.colr; },
      [](const auto &)                       { return Colr(0); },
    };
  }
  
  bool Uplifting::Vertex::has_equal_mismatching(const cnstr_type &other_v) const {
    met_trace();
    guard(constraint.index() == other_v.index(), false);
    return constraint | visit {
      [&](const DirectColorConstraint &cstr) {
        const auto &other = std::get<std::decay_t<decltype(cstr)>>(other_v);
        guard(cstr.colr_i.isApprox(other.colr_i), false);
        guard(cstr.cstr_j.size() == other.cstr_j.size(), false);
        if (!cstr.cstr_j.empty()) {
          // The "known" connstraints should be identical
          guard(rng::equal(cstr.cstr_j  | vws::take(cstr.cstr_j.size() - 1),
                           other.cstr_j | vws::take(cstr.cstr_j.size() - 1)), false);
          
          // The "free variable" should be identical outside of the specified color value
          guard(cstr.cstr_j.back().is_similar(other.cstr_j.back()), false);
        }
        return true; // only the free variable differs
      },
      [&](const DirectSurfaceConstraint &cstr) {
        const auto &other = std::get<std::decay_t<decltype(cstr)>>(other_v);
        guard(cstr.colr_i.isApprox(other.colr_i), false);
        guard(cstr.cstr_j.size() == other.cstr_j.size(), false);
        if (!cstr.cstr_j.empty()) {
          // The "known" connstraints should be identical
          guard(rng::equal(cstr.cstr_j  | vws::take(cstr.cstr_j.size() - 1),
                           other.cstr_j | vws::take(cstr.cstr_j.size() - 1)), false);
          
          // The "free variable" constraint should be identical outside of the specified color value
          guard(cstr.cstr_j.back().is_similar(other.cstr_j.back()), false);
        }
        return true; // only the free variable differs
      },
      [&](const IndirectSurfaceConstraint &cstr) {
        const auto &other = std::get<std::decay_t<decltype(cstr)>>(other_v);
        guard(rng::equal(cstr.powers, other.powers, eig::safe_approx_compare<Spec>), false);
        return true; // only the constraint value differs, same MMV
      },
      [](const auto &) { return true; },
    };
  }

  bool Uplifting::Vertex::has_surface() const {
    met_trace();
    return constraint | visit {
      [](const is_surface_constraint auto &) { return true;  },
      [](const auto &)                       { return false; },
    };
  }

  SurfaceInfo &Uplifting::Vertex::surface() {
    met_trace();
    return constraint | visit {
      [](is_surface_constraint auto &c) -> SurfaceInfo & { return c.surface; },
      [&](auto &) -> SurfaceInfo & { return detail::invalid_visitor_return_si; }
    };
  }

  const SurfaceInfo &Uplifting::Vertex::surface() const {
    met_trace();
    return constraint | visit {
      [](const is_surface_constraint auto &c) -> const SurfaceInfo & { return c.surface; },
      [&](const auto &) -> const SurfaceInfo & { return detail::invalid_visitor_return_si; }
    };
  }

  bool Uplifting::Vertex::has_mismatching(const Scene &scene, const Uplifting &uplifting) const {
    met_trace();
    return constraint | visit { 
      [&](const is_metameric_constraint auto &v) { return v.has_mismatching(scene, uplifting); }
    };
  }
} // namespace met