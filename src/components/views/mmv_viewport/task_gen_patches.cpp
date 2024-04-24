#include <metameric/core/distribution.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/mmv_viewport/task_gen_patches.hpp>
#include <algorithm>
#include <execution>

namespace met {
  constexpr uint n_samples = 16;
  
  bool GenPatchesTask::is_active(SchedulerHandle &info) {
    met_trace();
    auto chull_handle = info.relative("viewport_gen_mmv")("chull");
    auto gizmo_active = info.relative("viewport_guizmo")("is_active").getr<bool>();
    return chull_handle.is_mutated() 
      && !chull_handle.getr<AlMesh>().empty() 
      && !gizmo_active;
  }

  void GenPatchesTask::init(SchedulerHandle &info) {
    met_trace();
    info("patches").set(std::vector<Colr>()); // Make shared resource available
  }
  
  void GenPatchesTask::eval(SchedulerHandle &info) {
    met_trace();

    // Get shared resources
    const auto &e_chull = info.relative("viewport_gen_mmv")("chull").getr<AlMesh>();
    auto &i_patches     = info("patches").getw<std::vector<Colr>>();    guard(!e_chull.empty());
    
    // Do not output any patches until the convex hull is in a converged state;
    if (e_chull.verts.size() < 8) {
      i_patches.clear();
      return;
    }

    // Generate delaunay tesselation for uniform sampling of hull interior
    auto delaunay = generate_delaunay<Delaunay, eig::AlArray3f>(e_chull.verts);
    
    // Compute volume of each tetrahedron in delaunay
    std::vector<float> volumes(delaunay.elems.size());
    std::transform(std::execution::par_unseq, range_iter(delaunay.elems), volumes.begin(),  [&](const eig::Array4u &el) {
      // Get vertex positions for this tetrahedron
      std::array<eig::Vector3f, 4> p;
      rng::transform(el, p.begin(), [&](uint i) { return delaunay.verts[i]; });

      // Compute tetrahedral volume
      return std::abs((p[0] - p[3]).dot((p[1] - p[3]).cross(p[2] - p[3]))) / 6.f;
    });

    // Prepare for uniform sampling of the delaunay structure
    UniformSampler sampler(4);
    Distribution distr(volumes);

    // Generate patches by sampling random positions inside delaunay, which equate random 
    // colors inside the metameric mismatch volume
    i_patches.resize(n_samples);
    rng::generate(i_patches, [&]() -> Colr {
      // First, sample barycentric weights uniformly inside a tetrahedron 
      // (https://vcg.isti.cnr.it/jgt/tetra.htm)
      auto x = sampler.next_nd<3>();
      if (x.head<2>().sum() > 1.f)
        x.head<2>() = 1.f - x.head<2>();
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
      std::array<eig::Vector3f, 4> p;
      rng::transform(delaunay.elems[distr.sample_discrete(sampler.next_1d())], p.begin(),
        [&](uint i) { return delaunay.verts[i]; });
        
      // Then, recover position inside hull using the generated barycentric coordinates
      return p[0] * (1.f - x.sum()) + p[1] * x.x() + p[2] * x.y() + p[3] * x.z();
    });

    // Finally, sort patches by luminance
    rng::sort(i_patches, {}, luminance);
  }
} // namespace met