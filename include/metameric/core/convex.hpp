#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <execution>
#include <span>
#include <vector>
#include <utility>

namespace met {
  // Convex hull helper structure, as we typically need to build both
  // the convex hull and a delaunay tesselation of its interior at the
  // same time
  template <typename Vt> 
  struct ConvexHullBase {
    using vert_type = Vt;
    using hull_type = MeshBase<Vt, eig::Array3u>;
    using deln_type = MeshBase<Vt, eig::Array4u>;

    // Primary data; parts can be available
    hull_type hull;
    deln_type deln;

    // Data queries for hull data, available per-vertex
    bool has_hull()     const { return !hull.empty(); }
    bool has_delaunay() const { return !deln.empty(); }

  public: // Helper methods for searching or employing the convex hull
    // Find the best enclosing element in the underlying delaunay structure
    template <typename Vector>
    std::pair<eig::Array4f, typename deln_type::elem_type> find_enclosing_elem(const Vector &v) const {
      met_trace();
      debug::check_expr(has_delaunay());
      
      // We find best closest enclosing simplex through minimized error in barycentric coordinates;
      // if there is no enclosing simplex (as the point is exterior), this finds the closest. Otherwise,
      // error is zero for the correct simplex
      float        result_err = std::numeric_limits<float>::max();
      uint         result_i   = 0;
      eig::Array4f result_bry = 0;

      // Iterate all elements
      for (const auto &[i, el] : enumerate_view(deln.elems)) {
        const auto &vts = el | index_into_view(deln.verts);
        
        auto xyz = ((eig::Matrix3f() << vts[0] - vts[3], 
                                        vts[1] - vts[3], 
                                        vts[2] - vts[3]).finished().inverse()
                 * (v - vts[3]).matrix()).eval();
        auto bry = (eig::Array4f() << xyz, 1.f - xyz.sum()).finished();

        // Check if this simplex is a better candidate
        float err = (bry - bry.cwiseMax(0.f).cwiseMin(1.f)).matrix().squaredNorm();
        guard_continue(err < result_err);

        result_err = err;
        result_i   = i;
        result_bry = bry;

        // Check if this simplex is the enclosing simplex, meaning we can stop iterating
        guard_break(result_err > 0.f);
      }

      return { result_bry, deln.elems[result_i] };
    }

    // Clip a exterior point to the closest surface in the underlying convex hull
    template <typename Vector>
    auto find_closest_interior(const Vector &v) const {
      met_trace();
      debug::check_expr(has_hull());
      
      // Produce candidate points projeccted onto each triangle of the surface
      std::vector<Vector> candidates(hull.elems.size());
      std::transform(std::execution::par_unseq,
                     range_iter(hull.elems),
                     candidates.begin(),
                     [&](const auto &el) {
        const auto &vts = el | index_into_view(hull.verts);

        // Compute relevant plane data from edge vectors
        eig::Array3f center = (vts[0] + vts[1] + vts[2]) / 3.f;
        eig::Vector3f ab = vts[1] - vts[0], bc = vts[2] - vts[1],
                      ca = vts[0] - vts[2], ac = vts[2] - vts[0];
        eig::Vector3f n  = ab.cross(ac).normalized();

        // If point lies behind plane, return bad value; otherwise
        // project onto plane as v_
        float n_diff = n.dot((v - center).matrix());
        guard(n_diff > 0.f, Vector(std::numeric_limits<float>::infinity()));
        Vector v_ = (v - (n * n_diff).array());

        // Compute barycentrics of point on plane, then fit
        eig::Vector3f av = v_ - vts[0];
        float a_tri = std::abs(.5f * ac.cross(ab).norm());
        float a_ab  = std::abs(.5f * av.cross(ab).norm());
        float a_ac  = std::abs(.5f * ac.cross(av).norm());
        float a_bc  = std::abs(.5f * (vts[2] - v_).matrix().cross((vts[1] - v_).matrix()).norm());
        auto bary = (eig::Array3f(a_bc, a_ac, a_ab) / a_tri).eval();
             bary /= bary.abs().sum();
        v_ = bary.x() * vts[0] + bary.y() * vts[1] + bary.z() * vts[2];
        
        // If necessary, clamp barycentrics to a specific edge...
        if (bary.x() < 0.f) { // bc
          float t = std::clamp((v_ - vts[1]).matrix().dot(bc) / bc.squaredNorm(), 0.f, 1.f);
          v_ = vts[1] + t * bc.array();
        } else if (bary.y() < 0.f) { // ca
          float t = std::clamp((v_ - vts[2]).matrix().dot(ca) / ca.squaredNorm(), 0.f, 1.f);
          v_ = vts[2] + t * ca.array();
        } else if (bary.z() < 0.f) { // ab
          float t = std::clamp((v_ - vts[0]).matrix().dot(ab) / ab.squaredNorm(), 0.f, 1.f);
          v_ = vts[0] + t * ab.array();
        }

        return v_;
      });

      // If candidates were found, find the closest candidate
      guard(!candidates.empty(), v);
      auto it = rng::min_element(
        candidates, 
        {}, 
        [v](const Vector &v_) { return (v.array() - v_.array()).matrix().norm(); }); 
      
      // If a valid candidate remains, return it
      return it->array().isInf().any() ? v : *it;
    }
    
  public: // Helper methods for building 
    enum class BuildOptions { 
      eHull,     // Build only the enclosing convex hull
      eDelaunay, // Build only the interior tesselation
      eBoth      // Build both
    };

    static ConvexHullBase<vert_type> build(std::span<const vert_type> data, 
                                           BuildOptions               options      = BuildOptions::eBoth) {
      met_trace();
      ConvexHullBase<vert_type> ch;
      switch (options) {
        case BuildOptions::eHull:
          ch.hull = generate_convex_hull<MeshBase<vert_type, typename hull_type::elem_type>, vert_type>(data);
          break;
        case BuildOptions::eDelaunay:
          ch.deln = generate_delaunay<MeshBase<vert_type, typename deln_type::elem_type>, vert_type>(data);
          break;
        case BuildOptions::eBoth:
          ch.hull = generate_convex_hull<MeshBase<vert_type, typename hull_type::elem_type>, vert_type>(data);
          ch.deln = generate_delaunay<MeshBase<vert_type, typename deln_type::elem_type>, vert_type>(data);
          break;
      }
      return ch;
    }
  };

  using ConvexHull   = ConvexHullBase<eig::Array3f>;
  using AlConvexHull = ConvexHullBase<eig::AlArray3f>;
} // namespace met