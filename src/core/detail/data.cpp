#include <metameric/core/mesh.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/data.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/utility.hpp>
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

    // fmt::print("  Generating object color solid boundaries\n");

    // // Compute data points on convex hull of object color solid; used for convex hull clipping
    // auto ocs = generate_ocs_boundary_colr({ .basis      = appl_data.loaded_basis,
    //                                         .system     = appl_data.project_data.csys(0).finalize(), 
    //                                         .samples    = detail::gen_unit_dirs<3>(1024) });

    // // Generate cleaned mesh from data
    // auto ocs_mesh = simplify_edge_length<HalfedgeMeshData>(
    //   generate_convex_hull<HalfedgeMeshData, eig::Array3f>(ocs), 0.001f);

    // fmt::print("  Generating simplified convex hull\n");

    // // Generate simplified concave hull fitting texture data, then fit convex hull around this
    // auto chull_base = generate_convex_hull<HalfedgeMeshData, eig::Array3f>(appl_data.loaded_texture.data());
    // auto chull_mesh = generate_convex_hull<Mesh, eig::Array3f>(
    //   simplify_volume<Mesh>(chull_base, n_exterior_samples, &ocs_mesh).verts
    // );

    // fmt::print("  Convex hull result: {} vertices, {} elements\n", 
    //   chull_mesh.verts.size(), chull_mesh.elems.size());

    // // Update project data with new convex hull
    // appl_data.project_data.elems = chull_mesh.elems;
    // appl_data.project_data.verts.resize(chull_mesh.verts.size());
    // std::ranges::transform(chull_mesh.verts, appl_data.project_data.verts.begin(), [](Colr c) {
    //   return ProjectData::Vert { .colr_i = c, .csys_i = 0, .colr_j = { }, .csys_j = { } };
    // });
  }

  void init_constraints_points(ApplicationData &appl_data, uint n_interior_samples,
                               std::span<const ProjectCreateInfo::ImageData> images) {
    met_trace();

  //   guard(/* !images.empty() && */ n_interior_samples > 0);

  //   // Hardcoded settings shared across next steps
  //   constexpr uint sample_discretization = 256;

  //   // Data shared across next steps
  //   std::vector<uint>              indices;
  //   std::vector<Colr>              sample_colr_i(n_interior_samples);
  //   std::vector<std::vector<Colr>> sample_colr_j(n_interior_samples);

  //   { // 1. Build a distribution of unique color values s.t. identical texels are not sampled twice 
  //     // Instantiate an unordered map storing color/uint pairs
  //     std::unordered_map<
  //       eig::Array3u, 
  //       uint, 
  //       decltype(eig::detail::matrix_hash<eig::Array3u::value_type>), 
  //       decltype(eig::detail::matrix_equal)
  //     > indices_map;

  //     // Insert indices of discretized image colors into the map, if they do not yet exist
  //     auto colr_i_span = appl_data.loaded_texture.data();
  //     for (uint i = 0; i < colr_i_span.size(); ++i)
  //       indices_map.insert({ (colr_i_span[i] * sample_discretization).cast<uint>(), i });

  //     // Export resulting set of indices
  //     indices.resize(indices_map.size());
  //     std::transform(std::execution::par_unseq, range_iter(indices_map), indices.begin(),
  //       [](const auto &pair) { return pair.second; });
  //   } // 1.
    
  //   { // 2. Sample a random subset of texels and obtain their color values from each texture
  //     auto colr_i_span = appl_data.loaded_texture.data();
        
  //     // Define random generator
  //     std::random_device rd;
  //     std::mt19937 gen(rd());

  //     // Draw random, unique indices from indices
  //     std::vector<uint> samples = indices;
  //     std::shuffle(range_iter(samples), gen);
  //     samples.resize(std::min(static_cast<size_t>(n_interior_samples), samples.size()));

  //     // Extract colr_i, colr_j from input images at sampled indices
  //     std::ranges::transform(samples, sample_colr_i.begin(), [&](uint i) { return colr_i_span[i]; });
  //     if (!images.empty()) {
  //       for (uint i = 0; i < n_interior_samples; ++i) {
  //         sample_colr_j[i] = std::vector<Colr>(images.size());
  //         std::ranges::transform(images, sample_colr_j[i].begin(), 
  //           [&](const auto &info) { return info.image.data()[samples[i]]; });
  //       }
  //     }
  //   } // 2.
    
  //   { // 3. Specify constraints based on sampled texels and add to project data
  //     // Mapping indices [1, ...]
  //     std::vector<uint> csys_j_data(images.size());
  //     std::iota(range_iter(csys_j_data), 1);
      
  //     // Add vertices to project data
  //     appl_data.project_data.verts.reserve(appl_data.project_data.verts.size() + n_interior_samples);
  //     for (uint i = 0; i < n_interior_samples; ++i) {
  //       // Iterate through samples, in case bad samples still exist
  //       while (i < n_interior_samples) {
  //         ProjectData::Vert vt = { .colr_i = sample_colr_i[i], .csys_i = 0 };
  //         if (!images.empty()) {
  //           vt.colr_j = sample_colr_j[i];
  //           vt.csys_j = csys_j_data;
  //         }

  //         // Obtain color system spectra for this vertex
  //         std::vector<CMFS> systems = { appl_data.project_data.csys(vt.csys_i).finalize() };
  //         std::ranges::transform(vt.csys_j, std::back_inserter(systems), 
  //           [&](uint j) { return appl_data.project_data.csys(j).finalize(); });

  //         // Obtain corresponding color signal for each color system
  //         std::vector<Colr> signals(1 + vt.colr_j.size());
  //         signals[0] = vt.colr_i;
  //         std::ranges::copy(vt.colr_j, signals.begin() + 1);

  //         // Generate new spectrum given the current set of systems+signals as solver constraints
  //         Spec sd = generate_spectrum({ 
  //           .basis      = appl_data.loaded_basis,
  //           .systems    = std::span<CMFS> { systems }, 
  //           .signals    = std::span<Colr> { signals }
  //         });

  //         // Test roundtrip error for generated spectrum, compared to input color signal
  //         Colr signal_rt = appl_data.project_data.csys(0).apply(sd);
  //         float rt_error = (signal_rt - vt.colr_i).abs().sum();

  //         // Only add vertex to data if roundtrip error is below epsilon; otherwise this sample
  //         // has a bad fit (potentially indicating a problem with input data)
  //         if (rt_error > 0.0001f) {
  //           i++;
  //           continue;
  //         } else {
  //           appl_data.project_data.verts.push_back(vt);
  //           break;
  //         }
  //       } // while (i < n_interior_samples)
  //     } // for (uint i < n_interior_samples)
  //   } // 3.
  }

  void init_constraints_convex_hull(ApplicationData &appl_data, uint n_interior_samples,
                                    std::span<const ProjectCreateInfo::ImageData> images) {
  //   met_trace();

  //   guard(!images.empty() && n_interior_samples > 0);

  //   // Hardcoded settings shared across next steps
  //   constexpr uint sample_discretization = 256;
  //   constexpr uint sample_attemps        = 32;

  //   // Actual samples per image
  //   const uint n_samples = n_interior_samples / (images.size() + 1);

  //   // Data store shared across next steps
  //   std::vector<uint> indices;
  //   std::vector<Bary> bary_weights;
  //   // std::vector<uint> bary_indices;
  //   std::mutex solver_mutex;
  //   float solver_error = std::numeric_limits<float>::max();
    
  //   // Get current set of vertices
  //   std::vector<Colr> verts(appl_data.project_data.verts.size());
  //   std::ranges::transform(appl_data.project_data.verts, 
  //     verts.begin(), [](const auto &v) { return v.colr_i; });

  //   { // 1. Build a distribution of unique color values s.t. identical texels are not sampled twice 
  //     // Instantiate an unordered map storing color/uint pairs
  //     std::unordered_map<
  //       eig::Array3u, 
  //       uint, 
  //       decltype(eig::detail::matrix_hash<eig::Array3u::value_type>), 
  //       decltype(eig::detail::matrix_equal)
  //     > indices_map;

  //     // Insert indices of discretized image colors into the map, if they do not yet exist
  //     auto colr_i_span = appl_data.loaded_texture.data();
  //     for (uint i = 0; i < colr_i_span.size(); ++i)
  //       indices_map.insert({ (colr_i_span[i] * sample_discretization).cast<uint>(), i });

  //     // Export resulting set of indices
  //     indices.resize(indices_map.size());
  //     std::transform(std::execution::par_unseq, range_iter(indices_map), indices.begin(),
  //       [](const auto &pair) { return pair.second; });
  //   } // 1.

  //   { // 2. Generate generalized weights for the convex hull, w.r.t. the primary loaded image
  //     //    (we quickly hack-reuse generalized weight shader code for this step)

  //     const uint dispatch_n    = appl_data.loaded_texture.size().prod();
  //     const uint dispatch_ndiv = ceil_div(dispatch_n, 256u);
      
  //     // Initialize necessary objects for compute dispatch
  //     gl::Program program {{ .type       = gl::ShaderType::eCompute,
  //                            .spirv_path = "resources/shaders/pipeline/gen_generalized_weights.comp.spv",
  //                            .cross_path = "resources/shaders/pipeline/gen_generalized_weights.comp.json" }};
  //     gl::ComputeInfo dispatch { .groups_x         = dispatch_ndiv,
  //                                .bindable_program = &program };

  //     // Initialize uniform buffer layout
  //     struct UniformBuffer { uint n, n_verts, n_elems; } uniform_buffer {
  //       .n = dispatch_n,
  //       .n_verts = static_cast<uint>(verts.size()),
  //       .n_elems = static_cast<uint>(appl_data.project_data.elems.size())
  //     };

  //     // Create relevant buffer objects containing properly aligned data
  //     auto al_verts = std::vector<eig::AlArray3f>(range_iter(verts));
  //     auto al_elems = std::vector<eig::AlArray3u>(range_iter(appl_data.project_data.elems));
  //     gl::Buffer unif_buffer = {{ .data = obj_span<const std::byte>(uniform_buffer) }};
  //     gl::Buffer vert_buffer = {{ .data = cnt_span<const std::byte>(al_verts) }};
  //     gl::Buffer elem_buffer = {{ .data = cnt_span<const std::byte>(al_elems) }};
  //     gl::Buffer colr_buffer = {{ .data = cast_span<const std::byte>(io::as_aligned(appl_data.loaded_texture).data()) }};
  //     gl::Buffer wght_buffer = {{ .size = dispatch_n * sizeof(Bary), .flags = gl::BufferCreateFlags::eStorageDynamic }};

  //     // Bind required resources
  //     program.bind("b_unif", unif_buffer);
  //     program.bind("b_vert", vert_buffer);
  //     program.bind("b_elem", elem_buffer);
  //     program.bind("b_colr", colr_buffer);
  //     program.bind("b_bary", wght_buffer);

  //     // Dispatch shader to generate generalized barycentric weights
  //     gl::dispatch_compute(dispatch);
      
  //     // Recover computed barycentric weights
  //     bary_weights.resize(dispatch_n);
  //     wght_buffer.get_as(cnt_span<Bary>(bary_weights));
  //   } // 2.

  //   #pragma omp parallel for
  //   for (int _i = 0; _i < sample_attemps; ++_i) {
  //     auto colr_i_span = appl_data.loaded_texture.data();

  //     // Data store shared across next steps for current solve attempt
  //     std::vector<uint> sample_indices(n_samples);
  //     std::vector<Bary> sample_bary(n_samples);
  //     std::vector<Colr> sample_colr_i(n_samples);
  //     std::vector<std::vector<Colr>> sample_colr_j(images.size());
  //     std::vector<Spec>              gamut_spec;
  //     std::vector<ProjectData::Vert> gamut_verts;
  //     float roundtrip_error = 0.f;

  //     { // 1. Sample a random subset of texels and obtain their color values from each texture
  //       auto colr_i_span = appl_data.loaded_texture.data();
          
  //       // Define random generator
  //       std::random_device rd;
  //       std::mt19937 gen(rd());

  //       // Draw random, unique indices from indices
  //       std::vector<uint> samples = indices;
  //       std::shuffle(range_iter(samples), gen);
  //       samples.resize(std::min(static_cast<size_t>(n_samples), samples.size()));

  //       // Extract colr_i, colr_j from input images at sampled indices
  //       std::ranges::transform(samples, sample_colr_i.begin(), [&](uint i) { return colr_i_span[i]; });
  //       std::ranges::transform(samples, sample_bary.begin(), [&](uint i) { return bary_weights[i]; });
  //       for (uint i = 0; i < images.size(); ++i) {
  //         auto colr_j_span = images[i].image.data();
  //         sample_colr_j[i] = std::vector<Colr>(n_samples);
  //         std::ranges::transform(samples, sample_colr_j[i].begin(), [&](uint i) { return colr_j_span[i]; });
  //       }
  //     } // 1.

  //     { // 2. Solve for a spectral gamut which satisfies the provided input
  //       // Solve using image constraints directly
  //       GenerateGamutInfo info = {
  //         .basis      = appl_data.loaded_basis,
  //         .gamut      = verts,
  //         .systems    = std::vector<CMFS>(appl_data.project_data.color_systems.size()),
  //         .signals    = std::vector<GenerateGamutInfo::Signal>(n_samples)
  //       };

  //       // Transform mappings
  //       for (uint i = 0; i < appl_data.project_data.color_systems.size(); ++i)
  //         info.systems[i] = appl_data.project_data.csys(i).finalize();

  //       // Add baseline samples
  //       for (uint i = 0; i < n_samples; ++i)
  //         info.signals[i] = { .colr_v = sample_colr_i[i],
  //                             .bary_v = sample_bary[i],
  //                             .syst_i = 0 };

  //       // Add constraint samples
  //       for (uint i = 0; i < sample_colr_j.size(); ++i) {
  //         const auto &values = sample_colr_j[i];
  //         for (uint j = 0; j < n_samples; ++j) {
  //           info.signals.push_back({
  //             .colr_v = values[j],
  //             .bary_v = sample_bary[j],
  //             .syst_i = i + 1
  //           });
  //         }
  //       }

  //       // Fire solver and cross fingers
  //       gamut_spec = generate_gamut(info);
  //       gamut_spec.resize(verts.size());
  //     } // 2.

  //     { // 3. Obtain verts and constraints from spectral gamut, by applying known color systems
  //       for (uint i = 0; i < gamut_spec.size(); ++i) {
  //         const Spec &sd = gamut_spec[i];
  //         ProjectData::Vert vert;

  //         // Define vertex settings
  //         vert.colr_i = appl_data.project_data.verts[i].colr_i;
  //         vert.csys_i = 0;

  //         // Define constraint settings
  //         for (uint j = 1; j < appl_data.project_data.color_systems.size(); ++j) {
  //           vert.colr_j.push_back(appl_data.project_data.csys(j)(sd));
  //           vert.csys_j.push_back(j);
  //         }

  //         // Clip constraints to validity
  //         {
  //           std::vector<CMFS> systems = { appl_data.project_data.csys(vert.csys_i).finalize() };
  //           std::vector<Colr> signals = { vert.colr_i };
  //           for (uint j = 0; j < vert.colr_j.size(); ++j) {
  //             systems.push_back(appl_data.project_data.csys(vert.csys_j[j]).finalize());
  //             signals.push_back(vert.colr_j[j]);
  //           }
            
  //           Spec valid_spec = generate_spectrum({
  //             .basis      = appl_data.loaded_basis,
  //             .systems    = systems, 
  //             .signals    = signals
  //           });

  //           for (uint j = 0; j < vert.colr_j.size(); ++j) {
  //             vert.colr_i = appl_data.project_data.csys(vert.csys_i)(valid_spec);
  //             vert.colr_j[j] = appl_data.project_data.csys(vert.csys_j[j])(valid_spec);
  //           }
  //         }

  //         gamut_verts.push_back(vert);
  //       }
  //     } // 3.

  //     { // 4. Compute roundtrip error for the different inputs
  //       // Squared error based on offsets to the convex hull vertices
  //       /* for (uint i = 0; i < gamut_verts.size(); ++i)
  //         roundtrip_error += (gamut_verts[i].colr_i - verts[i]).pow(2.f).sum(); */

  //       // Add squared error based on sample roundtrip
  //       for (uint i = 0; i < n_samples; ++i) {
  //         // Recover spectrum at sample position
  //         Bary w = sample_bary[i];
  //         Spec s = 0.f;
  //         for (uint j = 0; j < gamut_spec.size(); ++j)
  //           s += w[j] * gamut_spec[j];
          
  //         // Add baseline sample error
  //         Colr colr_i = appl_data.project_data.csys(0)(s);
  //         roundtrip_error += (sample_colr_i[i] - colr_i).pow(2.f).sum();

  //         // Add constraint sample error
  //         for (uint j = 0; j < sample_colr_j.size(); ++j) {
  //           Colr colr_j = appl_data.project_data.csys(j + 1)(s);
  //           roundtrip_error += (sample_colr_j[j][i] - colr_j).pow(2.f).sum();
  //         }
  //       }
  //     } // 4.
      
  //     { // 5. Compare and apply results
  //       std::lock_guard<std::mutex> lock(solver_mutex);
  //       if (roundtrip_error < solver_error) {
  //         appl_data.project_data.verts = gamut_verts;
  //         solver_error = roundtrip_error;
  //         fmt::print("  Best error: {}\n", solver_error);
  //       }
  //     } // 5.
  //   } // for (_i < sample_attempts)
  }

  void init_constraints(ApplicationData &appl_data, uint n_interior_samples,
                        std::span<const ProjectCreateInfo::ImageData> images) {
    met_trace();
    switch (appl_data.project_data.meshing_type) {
    case ProjectMeshingType::eConvexHull:
      init_constraints_convex_hull(appl_data, n_interior_samples, images);
      break;
    case ProjectMeshingType::eDelaunay:
      init_constraints_points(appl_data, n_interior_samples, images);
      break;
    }
  }  
} // namespace met::detail