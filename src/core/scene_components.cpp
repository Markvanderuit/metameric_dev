#include <metameric/core/ranges.hpp>
#include <metameric/core/scene_components.hpp>

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

  bool Uplifting::Vertex::has_surface() const {
    return std::visit(overloaded {
      [](const SurfaceConstraint auto &c) { return true; },
      [&](const auto &) { return false; }
    }, constraint);
  }

  bool Uplifting::Vertex::has_mismatching() const {
    return std::visit(overloaded {
      [](const DirectColorConstraint &c)     { return c.has_mismatching(); },
      [](const DirectSurfaceConstraint &c)   { return c.has_mismatching(); },
      [](const IndirectSurfaceConstraint &c) { return c.has_mismatching(); },
      [&](const auto &)                      { return false; }
    }, constraint);
  }

  
  bool Uplifting::operator==(const Uplifting &o) const {
    return std::tie(csys_i, basis_i) == std::tie(o.csys_i, o.basis_i) && rng::equal(verts, o.verts);
  }

  SurfaceInfo &Uplifting::Vertex::surface() {
    return std::visit(overloaded {
      [](SurfaceConstraint auto &c) -> SurfaceInfo & { return c.surface; },
      [&](auto &) -> SurfaceInfo & { return detail::invalid_visitor_return_si; }
    }, constraint);
  }

  const SurfaceInfo &Uplifting::Vertex::surface() const {
    return std::visit(overloaded {
      [](const SurfaceConstraint auto &c) -> const SurfaceInfo & { return c.surface; },
      [&](const auto &) -> const SurfaceInfo & { return detail::invalid_visitor_return_si; }
    }, constraint);
  }
} // namespace met