#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/utility.hpp>

namespace met {
  // Convex hull helper structure, as we typically need to build both
  // the convex hull and a delaunay tesselation of its interior at the
  // same time
  template <typename Vt> 
  struct ConvexHullBase {
    using hull_type = MeshBase<Vt, eig::Array3u>;
    using deln_type = MeshBase<Vt, eig::Array4u>;

    // Primary data; parts can be available
    hull_type hull;
    deln_type deln;

    // Data queries for hull data, available per-vertex
    bool has_hull()     const { return !hull.empty(); }
    bool has_delaunay() const { return !deln.empty(); }

  public: // Constructor
    struct CreateInfo {
      // Input set of points
      std::span<const Vt> data;

      // Build only the enclosing hull, interior tessellation, or both?
      enum class BuildOptions { 
        eHull,     // Build only the enclosing convex hull
        eDelaunay, // Build only the interior tesselation
        eBoth      // Build both
      } options = BuildOptions::eBoth;
    };

    ConvexHullBase() = default;
    ConvexHullBase(CreateInfo info);

  public: // Helper methods for searching or employing the convex hull
    // Find the best enclosing element in the underlying delaunay structure
    std::pair<eig::Array4f, eig::Array4u> find_enclosing_elem(const Vt &v) const;

    // Clip a exterior point to the closest surface in the underlying convex hull
    Vt find_closest_interior(const Vt &v) const;
  };
} // namespace met