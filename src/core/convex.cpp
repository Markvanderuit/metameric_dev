// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <metameric/core/convex.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/ranges.hpp>
#include <execution>

namespace met {
  template <typename Vt>
  std::pair<eig::Array4f, eig::Array4u> ConvexHullBase<Vt>::find_enclosing_elem(const Vt &v) const {
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
    }
    
    return { result_bry, deln.elems[result_i] };
  }

  template <typename Vt>
  Vt ConvexHullBase<Vt>::find_closest_interior(const Vt &v) const {
    met_trace();
    debug::check_expr(has_hull());
    
    // Produce candidate points projeccted onto each triangle of the surface
    std::vector<Vt> candidates(hull.elems.size());
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
      guard(n_diff > 0.f, Vt(std::numeric_limits<float>::infinity()));
      Vt v_ = (v - (n * n_diff).array());

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
      [v](const Vt &v_) { return (v.array() - v_.array()).matrix().norm(); }); 
    
    // If a valid candidate remains, return it
    return it->array().isInf().any() ? v : *it;
  }

  template <typename Vt>
  ConvexHullBase<Vt>::ConvexHullBase(CreateInfo info) {
    met_trace();
    switch (info.options) {
      case CreateInfo::BuildOptions::eHull:
        hull = generate_convex_hull<MeshBase<Vt, typename hull_type::elem_type>, Vt>(info.data);
        break;
      case CreateInfo::BuildOptions::eDelaunay:
        deln = generate_delaunay<MeshBase<Vt, typename deln_type::elem_type>, Vt>(info.data);
        break;
      case CreateInfo::BuildOptions::eBoth:
        hull = generate_convex_hull<MeshBase<Vt, typename hull_type::elem_type>, Vt>(info.data);
        deln = generate_delaunay<MeshBase<Vt, typename deln_type::elem_type>, Vt>(info.data);
        break;
    }
  }

  // Explicit template class instantiations
  template class ConvexHullBase<eig::Array3f>;
  template class ConvexHullBase<eig::AlArray3f>;
} // namespace met