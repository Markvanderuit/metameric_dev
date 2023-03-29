#include <metameric/core/mesh.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/data.hpp>
#include <omp.h>
#include <algorithm>
#include <chrono>
#include <execution>
#include <mutex>
#include <numbers>
#include <ranges>
#include <random>
#include <unordered_map>

namespace met::detail {
  // Given a random vector in RN bounded to [-1, 1], return a vector
  // distributed over a gaussian distribution
  inline
  auto inv_gaussian_cdf(const auto &x) {
    met_trace();
    auto y = (-(x * x) + 1.f).max(.0001f).log().eval();
    auto z = (0.5f * y + (2.f / std::numbers::pi_v<float>)).eval();
    return (((z * z - y).sqrt() - z).sqrt() * x.sign()).eval();
  }
  
  // Given a random vector in RN bounded to [-1, 1], return a uniformly
  // distributed point on the unit sphere
  inline
  auto inv_unit_sphere_cdf(const auto &x) {
    met_trace();
    return inv_gaussian_cdf(x).matrix().normalized().eval();
  }

  // Generate a set of random, uniformly distributed unit vectors in RN
  template <uint N>
  inline
  std::vector<eig::Array<float, N, 1>> gen_unit_dirs(uint n_interior_samples) {
    met_trace();
    
    using ArrayNf = eig::Array<float, N, 1>;
    using SeedTy = std::random_device::result_type;

    // Generate separate seeds for each thread's rng
    std::random_device rd;
    std::vector<SeedTy> seeds(omp_get_max_threads());
    for (auto &s : seeds) s = rd();

    std::vector<ArrayNf> unit_dirs(n_interior_samples);
    #pragma omp parallel
    {
      // Initialize separate random number generator per thread
      std::mt19937 rng(seeds[omp_get_thread_num()]);
      std::uniform_real_distribution<float> distr(-1.f, 1.f);

      // Draw samples for this thread's range
      #pragma omp for
      for (int i = 0; i < unit_dirs.size(); ++i) {
        ArrayNf v;
        for (auto &f : v) f = distr(rng);
        unit_dirs[i] = detail::inv_unit_sphere_cdf(v);
      }
    }

    return unit_dirs;
  }

  void init_convex_hull(ApplicationData &appl_data, uint n_exterior_samples) {
    met_trace();

    fmt::print("  Generating object color solid boundaries\n");

    // Compute data points on convex hull of object color solid; used for convex hull clipping
    auto ocs = generate_ocs_boundary({ .basis      = appl_data.loaded_basis,
                                       .basis_mean = appl_data.loaded_basis_mean,
                                       .system     = appl_data.project_data.csys(0).finalize_direct(), 
                                       .samples    = detail::gen_unit_dirs<3>(1024) });

    // Generate cleaned mesh from data
    auto ocs_mesh = simplify_edge_length<HalfedgeMeshData>(
      generate_convex_hull<HalfedgeMeshData, eig::Array3f>(ocs), 0.001f);

    fmt::print("  Generating simplified convex hull\n");

    // Generate simplified concave hull fitting texture data, then fit convex hull around this
    auto chull_base = generate_convex_hull<HalfedgeMeshData, eig::Array3f>(appl_data.loaded_texture.data());
    auto chull_mesh = generate_convex_hull<IndexedMeshData, eig::Array3f>(
      simplify_volume<IndexedMeshData>(chull_base, n_exterior_samples, &ocs_mesh).verts
    );

    fmt::print("  Convex hull result: {} vertices, {} elements\n", 
      chull_mesh.verts.size(), chull_mesh.elems.size());

    // Update project data with new convex hull
    appl_data.project_data.elems = chull_mesh.elems;
    appl_data.project_data.verts.resize(chull_mesh.verts.size());
    std::ranges::transform(chull_mesh.verts, appl_data.project_data.verts.begin(), [](Colr c) {
      return ProjectData::Vert { .colr_i = c, .csys_i = 0, .colr_j = { }, .csys_j = { } };
    });
  }

  void init_constraints_convex_hull(ApplicationData &appl_data, uint n_interior_samples,
                                    std::span<const ProjectCreateInfo::ImageData> images) {
    met_trace();

  }
  
  void init_constraints_points(ApplicationData &appl_data, uint n_interior_samples,
                               std::span<const ProjectCreateInfo::ImageData> images) {
    met_trace();

    // Hardcoded settings shared across next steps
    constexpr uint sample_discretization = 256;

    // Data store shared across next steps
    std::vector<uint>              sampleable_indices;
    std::vector<Colr>              sample_colr_i(n_interior_samples);
    std::vector<std::vector<Colr>> sample_colr_j(n_interior_samples);

    { // 1. Build a distribution of unique color values s.t. identical texels are not sampled twice 
      // Instantiate an unordered map storing color/uint pairs
      std::unordered_map<
        eig::Array3u, 
        uint, 
        decltype(eig::detail::matrix_hash<eig::Array3u::value_type>), 
        decltype(eig::detail::matrix_equal)
      > indices_map;

      // Insert indices of discretized image colors into the map, if they do not yet exist
      auto colr_i_span = appl_data.loaded_texture.data();
      for (uint i = 0; i < colr_i_span.size(); ++i)
        indices_map.insert({ (colr_i_span[i] * sample_discretization).cast<uint>(), i });

      // Export resulting set of indices to sampleable_indices
      sampleable_indices.resize(indices_map.size());
      std::transform(std::execution::par_unseq, range_iter(indices_map), sampleable_indices.begin(),
        [](const auto &pair) { return pair.second; });
    } // 1.
    
    { // 2. Sample a random subset of texels and obtain their color values from each texture
      auto colr_i_span = appl_data.loaded_texture.data();
        
      // Define random generator
      std::random_device rd;
      std::mt19937 gen(rd());

      // Draw random, unique indices from sampleable_indices
      std::vector<uint> samples = sampleable_indices;
      std::shuffle(range_iter(samples), gen);
      samples.resize(std::min(static_cast<size_t>(n_interior_samples), samples.size()));

      // Extract colr_i, colr_j from input images at sampled indices
      std::ranges::transform(samples, sample_colr_i.begin(), [&](uint i) { return colr_i_span[i]; });
      for (uint i = 0; i < n_interior_samples; ++i) {
        sample_colr_j[i] = std::vector<Colr>(images.size());
        std::ranges::transform(images, sample_colr_j[i].begin(), 
          [&](const auto &info) { return info.image.data()[samples[i]]; });
      }
    } // 2.
    
    { // 3. Specify constraints based on sampled texels and add to project data
      // Mapping indices [1, ...]
      std::vector<uint> csys_j_data(images.size());
      std::iota(range_iter(csys_j_data), 1);
      
      // Add vertices to project data
      appl_data.project_data.verts.reserve(appl_data.project_data.verts.size() + n_interior_samples);
      for (uint i = 0; i < n_interior_samples; ++i) {
        // Iterate through samples, in case bad samples still exist
        while (i < n_interior_samples) {
          ProjectData::Vert vt = {
            .colr_i = sample_colr_i[i],
            .csys_i = 0,
            .colr_j = sample_colr_j[i],
            .csys_j = csys_j_data
          };

          // Obtain color system spectra for this vertex
          std::vector<CMFS> systems = { appl_data.project_data.csys(vt.csys_i).finalize_direct() };
          std::ranges::transform(vt.csys_j, std::back_inserter(systems), 
            [&](uint j) { return appl_data.project_data.csys(j).finalize_direct(); });

          // Obtain corresponding color signal for each color system
          std::vector<Colr> signals(1 + vt.colr_j.size());
          signals[0] = vt.colr_i;
          std::ranges::copy(vt.colr_j, signals.begin() + 1);

          // Generate new spectrum given the current set of systems+signals as solver constraints
          Spec sd = generate_spectrum({ 
            .basis      = appl_data.loaded_basis,
            .basis_mean = appl_data.loaded_basis_mean,
            .systems    = std::span<CMFS> { systems }, 
            .signals    = std::span<Colr> { signals }
          });

          // Test roundtrip error for generated spectrum, compared to input color signal
          Colr signal_rt = appl_data.project_data.csys(0).apply_color_direct(sd);
          float rt_error = (signal_rt - vt.colr_i).abs().sum();

          // Only add vertex to data if roundtrip error is below epsilon; otherwise this sample
          // has a bad fit (potentially indicating a problem with input data)
          if (rt_error > 0.0001f) {
            i++;
            continue;
          } else {
            appl_data.project_data.verts.push_back(vt);
            break;
          }
        } // while (i < n_interior_samples)
      } // for (uint i < n_interior_samples)
    } // 3.
  }

  void init_constraints(ApplicationData &appl_data, uint n_interior_samples,
                        std::span<const ProjectCreateInfo::ImageData> images) {
    met_trace();
    switch (appl_data.project_data.meshing_type) {
    case ProjectMeshingType::eConvexHull:
      init_constraints_convex_hull(appl_data, n_interior_samples, images);
      break;
    case ProjectMeshingType::ePoints:
      init_constraints_points(appl_data, n_interior_samples, images);
      break;
    }
  }

  
} // namespace met::detail