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
#include <mutex>
#include <numbers>
#include <ranges>
#include <random>
#include <unordered_map>

namespace met {
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
      project_data.color_systems.push_back({ .cmfs = image.cmfs, 
                                             .illuminant = image.illuminant, 
                                             .n_scatters = 1 });

    // Move first texture into application - not project - data; later stored in separate file
    loaded_texture_f32 = std::move(info.images[0].image);
    info.images.erase(info.images.begin());

    // Generate initial geometric data structure; if extra input images are provided,
    // start fitting constraints
    gen_convex_hull(info.n_vertices);
    if (!info.images.empty())
      gen_constraints_from_images(info.images);
  }
  
  void ApplicationData::save(const fs::path &path) {
    met_trace();

    project_save = SaveFlag::eSaved;
    project_path = io::path_with_ext(path, ".json");

    io::save_json(project_path, project_data);
    io::save_texture2d(io::path_with_ext(project_path, ".exr"), loaded_texture_f32, true);
  }

  void ApplicationData::load(const fs::path &path) {
    met_trace();

    project_save   = SaveFlag::eSaved;
    project_path   = io::path_with_ext(path, ".json");
    project_data   = io::load_json(path).get<ProjectData>();
    loaded_texture_f32 = io::load_texture2d<Colr>(io::path_with_ext(project_path,".exr"), true);

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

    // Generate temporary OCS for convex hull clipping
    fmt::print("  Generating object color solid boundaries\n");
    auto ocs = generate_ocs_boundary({ .basis     = loaded_basis,
                                       .basis_avg = loaded_basis_mean,
                                       .system    = project_data.csys(0).finalize_direct(), 
                                       .samples   = detail::gen_unit_dirs<3>(1024) });
    auto ocs_mesh = simplify_edge_length<HalfedgeMeshData>(
      generate_convex_hull<HalfedgeMeshData, eig::Array3f>(ocs), 0.001f);

    // Generate simplified concave hull fitting texture data, then fit convex hull around this
    fmt::print("  Generating simplified convex hull\n");
    auto chull_base = generate_convex_hull<HalfedgeMeshData, eig::Array3f>(loaded_texture_f32.data());
    auto chull_mesh = generate_convex_hull<IndexedMeshData, eig::Array3f>(
      simplify_volume<IndexedMeshData>(chull_base, n_vertices, &ocs_mesh).verts
    );

    fmt::print("  Convex hull result: {} vertices\n", chull_mesh.verts.size());

    // Update project data with new convex hull
    // project_data.gamut_elems = elems;
    project_data.vertices.resize(chull_mesh.verts.size());
    std::ranges::transform(chull_mesh.verts, project_data.vertices.begin(), [](Colr c) {
      return ProjectData::Vert { .colr_i = c, .csys_i = 0, .colr_j = { }, .csys_j = { } };
    });
  }

  void ApplicationData::gen_constraints_from_images(std::span<const ProjectCreateInfo::ImageData> images) {
    met_trace_full();

    // Relevant settings for the following section
    // TODO expose parameter to users in input view
    const uint n_samples    = 164;
    const uint sample_discr = 256;

    // Data store for next steps
    std::vector<uint>              sampleable_indices;
    std::vector<uint>              samples;
    std::vector<Colr>              sample_colr_i(n_samples);
    std::vector<std::vector<Colr>> sample_colr_j(n_samples);

    /* 0. Build a distribution of unique color values s.t. identical texels are not sampled twice  */
    {
      // Instantiate an unordered map storing color/uint pairs
      using MapValue = eig::Array3u;
      std::unordered_map<
        MapValue, 
        uint, 
        decltype(eig::detail::matrix_hash<MapValue::value_type>), 
        decltype(eig::detail::matrix_equal)
      > indices_map;

      // Insert indices of discretized image colors into the map, if they do not yet exist
      auto colr_i_span = loaded_texture_f32.data();
      for (uint i = 0; i < colr_i_span.size(); ++i)
        indices_map.insert({ (colr_i_span[i] * sample_discr).cast<uint>(), i });

      // Export resulting set of indices to sampleable_indices
      sampleable_indices.resize(indices_map.size());
      std::transform(std::execution::par_unseq, range_iter(indices_map), sampleable_indices.begin(),
        [](const auto &pair) { return pair.second; });

      fmt::print("Sampleable set: {} -> {}\n", colr_i_span.size(), sampleable_indices.size());
    }

    /* 1. Sample a random subset of texels and obtain their color values from each texture */
    {
      auto colr_i_span = loaded_texture_f32.data();
        
      // Define random generator
      std::random_device rd;
      std::mt19937 gen(rd());

      // Draw random, unique indices from sampleable_indices
      samples = sampleable_indices;
      std::shuffle(range_iter(samples), gen);
      samples.resize(std::min(static_cast<size_t>(n_samples), samples.size()));

      fmt::print("Samples size: {}\n", samples.size());

      // Extract colr_i, colr_j from input images at sampled indices
      std::ranges::transform(samples, sample_colr_i.begin(), [&](uint i) { return colr_i_span[i]; });
      for (uint i = 0; i < n_samples; ++i) {
        sample_colr_j[i] = std::vector<Colr>(images.size());
        std::ranges::transform(images, sample_colr_j[i].begin(), 
          [&](const auto &info) { return info.image.data()[samples[i]]; });
      }
    }

    /* 2. Specify constraints based on sampled pixels and add to project data */
    {
      // Mapping indices [1, ...]
      std::vector<uint> csys_j_data(images.size());
      std::iota(range_iter(csys_j_data), 1);
      
      // Add vertices to project data
      project_data.vertices.reserve(project_data.vertices.size() + n_samples);
      for (uint i = 0; i < n_samples; ++i) {
        // Iterate through samples, in case bad samples still exist
        while (i < n_samples) {
          ProjectData::Vert vt = {
            .colr_i = sample_colr_i[i],
            .csys_i = 0,
            .colr_j = sample_colr_j[i],
            .csys_j = csys_j_data
          };

          // Obtain color system spectra for this vertex
          std::vector<CMFS> systems = { project_data.csys(vt.csys_i).finalize_direct() };
          std::ranges::transform(vt.csys_j, std::back_inserter(systems), [&](uint j) { return project_data.csys(j).finalize_direct(); });

          // Obtain corresponding color signal for each color system
          std::vector<Colr> signals(1 + vt.colr_j.size());
          signals[0] = vt.colr_i;
          std::ranges::copy(vt.colr_j, signals.begin() + 1);

          // Generate new spectrum given the current set of systems+signals as solver constraints
          Spec sd = generate_spectrum({ 
            .basis      = loaded_basis,
            .basis_mean = loaded_basis_mean,
            .systems    = std::span<CMFS> { systems }, 
            .signals    = std::span<Colr> { signals },
            .reduce_basis_count = false
          });

          // Test roundtrip error for generated spectrum, compared to input color signal
          Colr signal_rt = project_data.csys(0).apply_color_direct(sd);
          float rt_error = (signal_rt - vt.colr_i).abs().sum();

          // Only add vertex to data if roundtrip error is below epsilon; otherwise this sample
          // has a bad fit (potentially indicating a problem with input data)
          if (rt_error > 0.0001f) {
            i++;
            continue;
          } else {
            project_data.vertices.push_back(vt);
            break;
          }
        }
      }
    }
  }
} // namespace met