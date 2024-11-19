#include <metameric/core/distribution.hpp>
#include <metameric/core/convex.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/mmv_viewport/task_gen_patches.hpp>
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
  
  void GenPatchesTask::eval(SchedulerHandle &info) {
    met_trace();

    // Get shared resources
    const auto &e_scene = info.global("scene").getr<Scene>();
    const auto &e_cs    = info.parent()("selection").getr<ConstraintRecord>();
    auto gizmo_active   = info.relative("viewport_guizmo")("is_active").getr<bool>();
    auto &i_patches     = info("patches").getw<std::vector<Colr>>();
    
    // Obtain the generated convex hull for this uplifting/vertex combination
    const auto &chull_builder = e_scene.components.upliftings.gl
                                       .uplifting_data[e_cs.uplifting_i]
                                       .metamer_builders[e_cs.vertex_i];
    const auto &chull = chull_builder.hull;

    // Do not output any patches until the convex hull is in a converged state;
    if (!chull.has_delaunay()) {
      i_patches.clear();
      return;
    }

    // Exit early unless inputs have changed somehow
    guard(is_first_eval() || chull_builder.did_sample());
    
    // Compute volume of each tetrahedron in delaunay
    std::vector<float> volumes(chull.deln.elems.size());
    std::transform(std::execution::par_unseq, range_iter(chull.deln.elems), volumes.begin(),
    [&](const eig::Array4u &el) {
      // Get vertex positions for this tetrahedron
      auto p = el | index_into_view(chull.deln.verts);

      // Compute tetrahedral volume
      return std::abs((p[0] - p[3]).matrix().dot((p[1] - p[3]).matrix().cross((p[2] - p[3]).matrix()))) / 6.f;
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
      auto el = chull.deln.elems[distr.sample_discrete(sampler.next_1d())];
      auto p = el | index_into_view(chull.deln.verts);
        
      // Then, recover position inside hull using the generated barycentric coordinates
      return p[0] * (1.f - x.sum()) 
           + p[1] * x.x() 
           + p[2] * x.y() 
           + p[3] * x.z();
    });

    // Finally, sort patches
    // Hack; we sort by a morton order lol
    auto maxc = rng::max(i_patches, {}, [](const Colr &c) { return c.maxCoeff(); });
    auto minc = rng::min(i_patches, {}, [](const Colr &c) { return c.minCoeff(); });
    rng::sort(i_patches, {}, [minc, mdiv = 1.f / (maxc - minc)](const Colr &c) {
      return detail::morton_code((c - minc) * mdiv);
    });
  }
} // namespace met