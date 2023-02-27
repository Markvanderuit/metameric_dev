#include <metameric/core/data.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/utility.hpp>
#include <nlohmann/json.hpp>
#include <omp.h>
#include <algorithm>
#include <chrono>
#include <execution>
#include <numbers>
#include <ranges>
#include <random>
#include <mutex>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;

  namespace detail {
    // Given a random vector in RN bounded to [-1, 1], return a vector
    // distributed over a gaussian distribution
    auto inv_gaussian_cdf(const auto &x) {
      met_trace();
      auto y = (-(x * x) + 1.f).max(.0001f).log().eval();
      auto z = (0.5f * y + (2.f / std::numbers::pi_v<float>)).eval();
      return (((z * z - y).sqrt() - z).sqrt() * x.sign()).eval();
    }
    
    // Given a random vector in RN bounded to [-1, 1], return a uniformly
    // distributed point on the unit sphere
    auto inv_unit_sphere_cdf(const auto &x) {
      met_trace();
      return inv_gaussian_cdf(x).matrix().normalized().eval();
    }

    // Generate a set of random, uniformly distributed unit vectors in RN
    template <uint N>
    std::vector<eig::Array<float, N, 1>> gen_unit_dirs(uint n_samples) {
      met_trace();
      
      using ArrayNf = eig::Array<float, N, 1>;
      using SeedTy = std::random_device::result_type;

      // Generate separate seeds for each thread's rng
      std::random_device rd;
      std::vector<SeedTy> seeds(omp_get_max_threads());
      for (auto &s : seeds) s = rd();

      std::vector<ArrayNf> unit_dirs(n_samples);
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
  } // namespace detail

  namespace io {
    ProjectData load_project(const fs::path &path) {
      return load_json(path).get<ProjectData>();
    }

    void save_project(const fs::path &path, const ProjectData &data) {
      save_json(path, data);
    }
  } // namespace io

  ProjectCreateInfo::ProjectCreateInfo()
  : n_vertices(4),
    illuminants({{ "D65",  models::emitter_cie_d65        },
                 { "E",    models::emitter_cie_e          },
                 { "FL2",  models::emitter_cie_fl2        },
                 { "FL11", models::emitter_cie_fl11       },
                 { "LED-RGB1", models::emitter_cie_ledrgb1} }),
    cmfs({{ "CIE XYZ", models::cmfs_cie_xyz  }}) { }

  ColrSystem ProjectData::csys(uint i) const {
    met_trace();
    return csys(color_systems[i]);
  }

  ColrSystem ProjectData::csys(CSys m) const {
    met_trace();
    return { .cmfs = cmfs[m.cmfs].second,
             .illuminant = illuminants[m.illuminant].second,
             .n_scatters = m.n_scatters };
  }
  
  void ApplicationData::create(ProjectCreateInfo &&info) {
    met_trace();

    fmt::print("Project generation\n");
    debug::check_expr_rel(!info.images.empty(), "ProjectCreateInfo::images must not be empty");

    // Reset project data
    project_save = SaveFlag::eNew;
    project_path = ""; // TBD on first save
    project_data = ProjectData();

    // Reset undo/redo history
    mods  = { };
    mod_i = -1;

    // Copy over cmfs/illuminants and assign mappings in image order
    project_data.cmfs = info.cmfs;
    project_data.illuminants = info.illuminants;
    project_data.color_systems.clear();
    for (auto &image : info.images)
      project_data.color_systems.push_back({ .cmfs = image.cmfs, .illuminant = image.illuminant, .n_scatters = 1 });

    // Move texture into application - not project - data; stored in separate file
    loaded_texture_f32 = std::move(info.images[0].image);
    info.images.erase(info.images.begin()); // Youch
    
    gen_convex_hull(info.n_vertices);

    // If additional input images are provided, fit constraints
    if (!info.images.empty())
      gen_constraints_from_images(info.images);
  }
  
  void ApplicationData::save(const fs::path &path) {
    met_trace();

    project_save = SaveFlag::eSaved;
    project_path = io::path_with_ext(path, ".json");

    io::save_json(project_path, project_data);
    io::save_texture2d(io::path_with_ext(project_path, ".bmp"), loaded_texture_f32, true);
  }

  void ApplicationData::load(const fs::path &path) {
    met_trace();

    project_save   = SaveFlag::eSaved;
    project_path   = io::path_with_ext(path, ".json");
    project_data   = io::load_json(path).get<ProjectData>();
    loaded_texture_f32 = io::load_texture2d<Colr>(io::path_with_ext(project_path,".bmp"), true);

    // Reset undo/redo history
    mods  = { };
    mod_i = -1;
  }

  void ApplicationData::touch(ProjectMod &&mod) {
    met_trace();

    // Apply change
    mod.redo(project_data);

    // Ensure mod list doesn't exceed fixed length
    // and set the current mod to its end
    int n_mods = std::clamp(mod_i + 1, 0, 128);
    mod_i = n_mods;
    mods.resize(mod_i);
    mods.push_back(mod);   
    
    if (project_save == SaveFlag::eSaved) {
      project_save = SaveFlag::eUnsaved;
    }
  }

  void ApplicationData::redo() {
    met_trace();
    
    guard(mod_i < (int(mods.size()) - 1));
    
    mod_i += 1;
    mods[mod_i].redo(project_data);

    if (project_save == SaveFlag::eSaved) {
      project_save = SaveFlag::eUnsaved;
    }
  }

  void ApplicationData::undo() {
    met_trace();
    
    guard(mod_i >= 0);

    mods[mod_i].undo(project_data);
    mod_i -= 1;

    if (project_save == SaveFlag::eSaved) {
      project_save = SaveFlag::eUnsaved;
    }
  }

  void ApplicationData::unload() {
    met_trace();

    project_save = SaveFlag::eUnloaded;
    project_path  = "";
    project_data  = { };

    loaded_texture_f32  = { };
    
    mods  = { };
    mod_i = -1;
  }  

  void ApplicationData::gen_convex_hull(uint n_vertices) {
    met_trace_full();

    /* // Generate temporary OCS for convex hull clipping
    fmt::print("  Generating object color solid boundaries\n");
    auto ocs = generate_ocs_boundary({ .basis     = loaded_basis,
                                       .basis_avg = loaded_basis_mean,
                                       .system    = project_data.csys(0).finalize_direct(), 
                                       .samples   = detail::gen_unit_dirs<3>(1024) });
    auto ocs_mesh = simplify_edge_length<HalfedgeMeshData>(
      generate_convex_hull<HalfedgeMeshData, eig::Array3f>(ocs), 0.001f);

    // Generate simplified concave hull fitting texture data, then fit convex hull around this
    fmt::print("  Generating simplified convex hull\n");
    auto chull_mesh = generate_convex_hull<HalfedgeMeshData, eig::Array3f>(loaded_texture_f32.data());
    auto [verts, elems] = generate_convex_hull<IndexedMeshData, eig::Array3f>(
      simplify_volume<IndexedMeshData>(chull_mesh, n_vertices, &ocs_mesh).verts
    );
    

    fmt::print("  Convex hull result: {} vertices, {} faces\n", verts.size(), elems.size());

    // Update project data with new convex hull
    project_data.gamut_elems = elems;
    project_data.gamut_verts.resize(verts.size());
    std::ranges::transform(verts, project_data.gamut_verts.begin(), [](Colr c) {
      return ProjectData::Vert { .colr_i = c, .csys_i = 0, .colr_j = { }, .csys_j = { } };
    }); */
  }

  void ApplicationData::gen_constraints_from_images(std::span<const ProjectCreateInfo::ImageData> images) {
    met_trace_full();
  
    // // Relevant settings for the following section
    // const     uint n_samples  = 48;
    // constexpr uint n_attempts = 32;
    // constexpr uint n_runs     = 1;

    // // Get current set of vertices
    // std::vector<Colr> verts(project_data.gamut_verts.size());
    // std::ranges::transform(project_data.gamut_verts, verts.begin(), [](const auto &v) { return v.colr_i; });
    
    // // Intermediate storage for computed barycentric weights and indices to positive weights
    // std::vector<eig::Array<float, barycentric_weights, 1>> img_weights;
    // std::vector<uint> img_indices;

    // /* 1. Generate barycentric weights for the convex hull, given the input image;
    //       we quick hack reuse shader code from the rendering pipeline */
    // {
    //   fmt::print("  Generating barycentric weights\n");

    //   const uint n = loaded_texture_f32.size().prod();
    //   const uint n_div = ceil_div(n, 256u);

    //   // Create program object, reusing shader from gen_barycentric_weights
    //   gl::Program bary_program = {{ .type = gl::ShaderType::eCompute,
    //                                 .path = "resources/shaders/gen_barycentric_weights/gen_barycentric_weights.comp.spv_opt",
    //                                 .is_spirv_binary = true }};

    //   // Initialize uniform buffer layout
    //   struct UniformBuffer { uint n, n_verts, n_elems; } uniform_buffer = {
    //     .n = n,
    //     .n_verts = static_cast<uint>(verts.size()),
    //     .n_elems = static_cast<uint>(project_data.gamut_elems.size())
    //   };

    //   // Create relevant buffer objects containing properly aligned data
    //   auto al_verts = std::vector<eig::AlArray3f>(range_iter(verts));
    //   auto al_elems = std::vector<eig::AlArray3u>(range_iter(project_data.gamut_elems));
    //   gl::Buffer bary_vert_buffer = {{ .data = cnt_span<const std::byte>(al_verts) }};
    //   gl::Buffer bary_elem_buffer = {{ .data = cnt_span<const std::byte>(al_elems) }};
    //   gl::Buffer bary_unif_buffer = {{ .data = obj_span<const std::byte>(uniform_buffer) }};
    //   gl::Buffer bary_colr_buffer = {{ .data = cast_span<const std::byte>(io::as_aligned(loaded_texture_f32).data()) }};
    //   gl::Buffer bary_wght_buffer = {{ .size = loaded_texture_f32.size().prod() * barycentric_weights * sizeof(float),
    //                                   .flags = gl::BufferCreateFlags::eStorageDynamic }};

    //   // Bind resources to buffer targets for upcoming shader dispatch
    //   bary_vert_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    //   bary_elem_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    //   bary_colr_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 2);
    //   bary_wght_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 3);
    //   bary_unif_buffer.bind_to(gl::BufferTargetType::eUniform,       0);
      
    //   // Dispatch shader call and copy results to host memory
    //   gl::dispatch_compute({ .groups_x = n_div, .bindable_program = &bary_program });
    //   img_weights.resize(n);
    //   bary_wght_buffer.get(cnt_span<std::byte>(img_weights));

    //   // Obtain mask over indices of non-negative barycentric weights; in case the convex hull
    //   // estimation does not provide a perfect fit, as the decimation implementation is a bit wonky
    //   img_indices.clear();
    //   std::vector<uint> index_full(n);
    //   std::iota(range_iter(index_full), 0);
    //   std::copy_if(range_iter(index_full), std::back_inserter(img_indices),
    //     [&img_weights](uint i) { return (img_weights[i] >= 0).all(); });

    //   float positive_ratio = static_cast<float>(img_indices.size()) / static_cast<float>(n);
    //   fmt::print("  Barycentric weight fit: {}\n", positive_ratio);
    // }

    // // Mutex for safe solver sections
    // std::mutex solver_mutex;
    // float solver_error = std::numeric_limits<float>::max();

    // // Begin solver runs
    // fmt::print("  Starting solver, {} samples, {} attempts\n", n_samples, n_attempts);
    // #pragma omp parallel for
    // for (int _i = 0; _i < n_attempts; ++_i) {
    //   using Wght = eig::Matrix<float, barycentric_weights, 1>;

    //   // Data storage for the current attempt's random samples
    //   std::vector<uint> sample_indices(n_samples);
    //   std::vector<Wght> sample_bary(n_samples);
    //   std::vector<Colr> sample_colr_i(n_samples);
    //   std::vector<std::vector<Colr>> sample_colr_j(images.size());

    //   /* 1. Sample a random subset of pixels in the texture and obtain their color values */
    //   {
    //     auto colr_i_span = loaded_texture_f32.data();

    //     // Define random distribution to sample non-negative weight indices
    //     std::random_device rd;
    //     std::mt19937 eng(rd());
    //     std::uniform_int_distribution<uint> distr(0, img_indices.size() - 1);

    //     // Draw random samples from said distribution
    //     std::vector<uint> samples(n_samples);
    //     std::ranges::generate(samples, [&]{ return distr(eng); });

    //     // Extract sampled data
    //     std::ranges::transform(samples, sample_indices.begin(), [&](uint i) { return img_indices[i]; });
    //     std::ranges::transform(sample_indices, sample_colr_i.begin(), [&](uint i) { return colr_i_span[i]; });
    //     std::ranges::transform(sample_indices, sample_bary.begin(), [&](uint i) { return img_weights[i]; });
    //     for (uint i = 0; i < images.size(); ++i) {
    //       auto colr_j_span = images[i].image.data();
    //       sample_colr_j[i] = std::vector<Colr>(n_samples);
    //       std::ranges::transform(sample_indices, sample_colr_j[i].begin(), [&](uint i) { return colr_j_span[i]; });
    //     }
    //   }

    //   // Intermediate storage for solved spectral gamut
    //   std::vector<Spec> gamut_spec;

    //   /* 2. Solve for a spectral gamut which satisfies the provided input*/
    //   {
    //     // Solve using image constraints directly
    //     GenerateGamutInfo info = {
    //       .basis     = loaded_basis,
    //       .basis_avg = loaded_basis_mean,
    //       .gamut     = verts,
    //       .systems   = std::vector<CMFS>(project_data.color_systems.size()),
    //       .signals   = std::vector<GenerateGamutInfo::Signal>(n_samples)
    //     };

    //     // Transform mappings
    //     for (uint i = 0; i < project_data.color_systems.size(); ++i)
    //       info.systems[i] = project_data.csys(i).finalize_direct();

    //     // Add baseline samples
    //     for (uint i = 0; i < n_samples; ++i)
    //       info.signals[i] = { .colr_v = sample_colr_i[i],
    //                           .bary_v = sample_bary[i],
    //                           .syst_i = 0 };

    //     // Add constraint samples
    //     for (uint i = 0; i < sample_colr_j.size(); ++i) {
    //       const auto &values = sample_colr_j[i];
    //       for (uint j = 0; j < n_samples; ++j) {
    //         info.signals.push_back({
    //           .colr_v = values[j],
    //           .bary_v = sample_bary[j],
    //           .syst_i = i + 1
    //         });
    //       }
    //     }

    //     // Fire solver and cross fingers
    //     gamut_spec = generate_gamut(info);
    //     gamut_spec.resize(verts.size());
    //   }

    //   // Intermediate storage for vertices and constraints
    //   std::vector<ProjectData::Vert> gamut_verts;

    //   /* 3. Obtain vertices and constraints from spectral gamut, by applying known color systems */
    //   {
    //     for (uint i = 0; i < gamut_spec.size(); ++i) {
    //       const Spec &sd = gamut_spec[i];
    //       ProjectData::Vert vert;

    //       // Define vertex settings
    //       vert.colr_i = project_data.gamut_verts[i].colr_i;
    //       vert.csys_i = 0;

    //       // Define constraint settings
    //       for (uint j = 1; j < project_data.color_systems.size(); ++j) {
    //         vert.colr_j.push_back(project_data.csys(project_data.color_systems[j])(sd));
    //         vert.csys_j.push_back(j);
    //       }

    //       // Clip constraints to validity
    //       {
    //         std::vector<CMFS> systems = { project_data.csys(vert.csys_i).finalize_direct() };
    //         std::vector<Colr> signals = { vert.colr_i };
    //         for (uint j = 0; j < vert.colr_j.size(); ++j) {
    //           systems.push_back(project_data.csys(vert.csys_j[j]).finalize_direct());
    //           signals.push_back(vert.colr_j[j]);
    //         }
            
    //         Spec valid_spec = generate_spectrum({
    //           .basis      = loaded_basis,
    //           .basis_mean = loaded_basis_mean,
    //           .systems    = systems, 
    //           .signals    = signals
    //         });

    //         for (uint j = 0; j < vert.colr_j.size(); ++j) {
    //           vert.colr_i = project_data.csys(vert.csys_i)(valid_spec);
    //           vert.colr_j[j] = project_data.csys(vert.csys_j[j])(valid_spec);
    //         }
    //       }

    //       gamut_verts.push_back(vert);
    //     }
    //   }

    //   // Intermediate error storage
    //   float roundtrip_error = 0.f;

    //   /* 4. Compute roundtrip error for the different inputs */
    //   {
    //     // Squared error based on offsets to the convex hull vertices
    //     /* for (uint i = 0; i < gamut_verts.size(); ++i)
    //       roundtrip_error += (gamut_verts[i].colr_i - verts[i]).pow(2.f).sum(); */

    //     // Add squared error based on sample roundtrip
    //     for (uint i = 0; i < n_samples; ++i) {
    //       // Recover spectrum at sample position
    //       Wght w = sample_bary[i];
    //       Spec s = 0.f;
    //       for (uint j = 0; j < gamut_spec.size(); ++j)
    //         s += w[j] * gamut_spec[j];
          
    //       // Add baseline sample error
    //       Colr colr_i = project_data.csys(0)(s);
    //       roundtrip_error += (sample_colr_i[i] - colr_i).pow(2.f).sum();

    //       // Add constraint sample error
    //       for (uint j = 0; j < sample_colr_j.size(); ++j) {
    //         Colr colr_j = project_data.csys(j + 1)(s);
    //         roundtrip_error += (sample_colr_j[j][i] - colr_j).pow(2.f).sum();
    //       }
    //     }
    //   }

    //   /* 5. Apply results */
    //   {
    //     std::lock_guard<std::mutex> lock(solver_mutex);
    //     if (roundtrip_error < solver_error) {
    //       project_data.gamut_verts = gamut_verts;
    //       solver_error = roundtrip_error;
    //       fmt::print("  Best error: {}\n", solver_error);
    //     }
    //   }
    // }
  }
} // namespace met