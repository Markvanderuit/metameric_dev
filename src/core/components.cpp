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

  std::pair<Colr, Spec> Uplifting::Vertex::realize(const Scene &scene, const Uplifting &uplifting) const {
    met_trace();

    // Return zero constraint for inactive vertices
    guard(is_active, { Colr(0), Spec(0) });
    
    // Visit the underlying constraint to generate output data
    return constraint | visit([&](const auto &cstr) { return cstr.realize(scene, uplifting); });
  }

  bool Uplifting::Vertex::has_surface() const {
    met_trace();
    return constraint | visit {
      [](const is_surface_constraint auto &v) { return true; },
      [](const auto &) { return false; },
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

  bool Uplifting::Vertex::has_mismatching() const {
    met_trace();
    return constraint | visit { 
      [](const is_metameric_constraint auto &v) { return v.has_mismatching(); }
    };
  }
} // namespace met