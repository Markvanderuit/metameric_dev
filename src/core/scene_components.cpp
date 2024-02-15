#include <metameric/core/scene_components.hpp>

namespace met {
  namespace detail {
    // Helper for surface() calls; invalid return for non-surface constraints
    static SurfaceInfo invalid_visitor_return_si = SurfaceInfo::invalid();
  } // namespace detail

  bool Uplifting::Vertex::has_surface() const {
    return std::visit(overloaded {
      [](const SurfaceConstraint auto &c) { return true; },
      [&](const auto &) { return false; }
    }, constraint);
  }

  bool Uplifting::Vertex::has_mismatching() const {
    return std::visit(overloaded {
      [](const DirectColorConstraint &c) {  return !c.csys_j.empty(); },
      [](const DirectSurfaceConstraint &c) {  return !c.csys_j.empty(); },
      [](const IndirectSurfaceConstraint &c) { return true; },
      [&](const auto &) { return false; }
    }, constraint);
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