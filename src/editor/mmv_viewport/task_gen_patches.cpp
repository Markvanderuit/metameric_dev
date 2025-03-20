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

#include <metameric/core/distribution.hpp>
#include <metameric/core/convex.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/editor/mmv_viewport/task_gen_patches.hpp>
#include <algorithm>
#include <execution>

namespace met {
  // Nr. of color patches to sample
  constexpr uint n_samples = 32;

  namespace detail {
    uint expand_bits_10(uint i) {
      i = (i * 0x00010001u) & 0xFF0000FFu;
      i = (i * 0x00000101u) & 0x0F00F00Fu;
      i = (i * 0x00000011u) & 0xC30C30C3u;
      i = (i * 0x00000005u) & 0x49249249u;
      return i;
    }
    
    uint morton_code(eig::Array3f v) {
      v = (v * 1024.f).cwiseMax(0.f).cwiseMin(1023).eval();
      return expand_bits_10(uint(v.x())) * 4u + 
             expand_bits_10(uint(v.y())) * 2u + 
             expand_bits_10(uint(v.z()));
    }
  } // namespace detail
  
  bool GenPatchesTask::is_active(SchedulerHandle &info) {
    met_trace();
    
    // Get shared resources
    const auto &e_scene = info.global("scene").getr<Scene>();
    const auto &e_cs    = info.parent()("selection").getr<ConstraintRecord>();
    auto gizmo_active   = info.relative("viewport_guizmo")("is_active").getr<bool>();

    // Obtain the generated convex hull for this uplifting/vertex combination
    const auto &chull = e_scene.components.upliftings.gl
                               .uplifting_data[e_cs.uplifting_i]
                               .metamer_builders[e_cs.vertex_i]
                               .hull;
    
    return chull.has_delaunay() && !gizmo_active;
  }

  void GenPatchesTask::init(SchedulerHandle &info) {
    met_trace();
    info("patches").set(std::vector<Colr>()); // Make shared resource available
  }
  
  // Helper function; given a convex hull with delaunay interior, sample n_samples positions
  // in the hull interior
  std::vector<Colr> sample_hull_interior_positions(const ConvexHull &chull, uint n_samples) {
    met_trace();

    // Declare uniform sampler, seeded by fair die roll
    UniformSampler sampler = 4;

    // Helper closure; 
    // compute volume of a (tetrahedral) element in the convex hull's delaunay interior
    auto func_compute_elem_volume = [&](const eig::Array4u &el) -> float {
      // Get vertex positions for this element
      auto p = el | index_into_view(chull.deln.verts);

      // Compute edge vectors
      auto v30 = (p[0] - p[3]).matrix();
      auto v31 = (p[1] - p[3]).matrix();
      auto v32 = (p[2] - p[3]).matrix();

      // Compute tetrahedral volume
      return std::abs(v30.dot(v31.cross(v32))) / 6.f;
    };

    // Compute volume of each element in chull's delaunay interior
    std::vector<float> volumes(chull.deln.elems.size());
    std::transform(
      std::execution::par_unseq, 
      range_iter(chull.deln.elems), 
      volumes.begin(), 
      func_compute_elem_volume
    );

    // Set up 1D sampling distribution over element volumes
    Distribution distr(volumes);

    // Helper closure; 
    // sample a random position in the chull's interior
    auto func_sample_position = [&]() -> Colr {
      // First, sample barycentric weights uniformly
      auto x = sampler.next_nd<3>();
      if (x.head<2>().sum() > 1.f) {
        x.head<2>() = 1.f - x.head<2>();
      }
      if (x.tail<2>().sum() > 1.f) {
        float t = x[2];
        x[2] = 1.f - x.head<2>().sum();
        x[1] = 1.f - t;
      } else if (x.sum() > 1.f) {
        float t = x[2];
        x[2] = x.sum() - 1.f;
        x[0] = 1.f - x[1] - t;
      }

      // Next, sample a tetrahedron uniformly based on volume, and grab its vertices
      auto el = chull.deln.elems[distr.sample_discrete(sampler.next_1d())];
      auto p = el | index_into_view(chull.deln.verts);
        
      // Then, recover position inside hull using the generated barycentric coordinates
      return p[0] * (1.f - x.sum()) + p[1] * x.x()  + p[2] * x.y() + p[3] * x.z();
    };

    // Sample random positions in the delaunay delaunay, which equate random 
    // colors inside the convex hull structure
    std::vector<Colr> samples(n_samples);
    rng::generate(samples, func_sample_position);
    return samples;
  }

  // Helper function; sort color values by morton order; color sorting is a bit of a fun
  // problem, but this gives a generally good output
  void sort_hull_interior_samples(std::vector<Colr> &samples) {
    met_trace();

    // Declare comparator over morton code representation of 3d color position
    auto maxc = rng::max(samples, {}, [](const Colr &c) { return c.maxCoeff(); });
    auto minc = rng::min(samples, {}, [](const Colr &c) { return c.minCoeff(); });
    auto func_compare_morton = [minc, rcp = 1.f / (maxc - minc)](const Colr &c) { 
      return detail::morton_code((c - minc) * rcp);
    };
    
    // Finally, sort sample output
    rng::sort(samples, {}, func_compare_morton);
  }

  void GenPatchesTask::eval(SchedulerHandle &info) {
    met_trace();

    // Get shared resources
    const auto &e_scene = info.global("scene").getr<Scene>();
    const auto &e_cs    = info.parent()("selection").getr<ConstraintRecord>();
    auto gizmo_active   = info.relative("viewport_guizmo")("is_active").getr<bool>();
    auto &i_patches     = info("patches").getw<std::vector<Colr>>();
    
    // Obtain the generated convex hull for this uplifting/vertex combination
    const auto &chull_builder 
      = e_scene.components.upliftings.gl.uplifting_data[e_cs.uplifting_i]
               .metamer_builders[e_cs.vertex_i];
    const auto &chull = chull_builder.hull;

    // Do not output any patches until the structure is in a valid state
    // with delaunay interior
    if (!chull.has_delaunay()) {
      i_patches.clear();
      return;
    }

    // Exit early unless inputs have changed somehow
    guard(is_first_eval() || chull_builder.did_sample());
    
    // Generate samples and output in sorted order for visualization
    i_patches = sample_hull_interior_positions(chull, n_samples);
    sort_hull_interior_samples(i_patches);
  }
} // namespace met