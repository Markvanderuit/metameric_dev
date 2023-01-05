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
#include <algorithm>
#include <execution>
#include <ranges>
#include <random>
#include <mutex>

namespace met {
  constexpr uint chull_vertex_count = 5;

  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;

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
    illuminants({{ "D65",  models::emitter_cie_d65  },
                 { "E",    models::emitter_cie_e    },
                 { "FL2",  models::emitter_cie_fl2  },
                 { "FL11", models::emitter_cie_fl11 }}),
    cmfs({{ "CIE XYZ", models::cmfs_cie_xyz  }}) { }

  Mapp ProjectData::mapping_data(uint i) const {
    met_trace();
    return mapping_data(mappings[i]);
  }

  Mapp ProjectData::mapping_data(Mapp m) const {
    met_trace();
    return { .cmfs = cmfs[m.cmfs].second,
             .illuminant = illuminants[m.illuminant].second };
  }
  
  void ApplicationData::create(ProjectCreateInfo &&info) {
    met_trace();

    debug::check_expr_rel(!info.images.empty(), "ProjectCreateInfo::images must not be empty");

    // Reset project data
    project_save = SaveFlag::eNew;
    project_path = ""; // TBD on first save
    project_data = ProjectData();

    // Reset undo/redo history
    mods  = { };
    mod_i = -1;

    // Copy over cmfs/illuminants and assign mappings in image order
    project_data.cmfs        = info.cmfs;
    project_data.illuminants = info.illuminants;
    project_data.mappings.clear();
    for (auto &image : info.images)
      project_data.mappings.push_back({ .cmfs       = image.cmfs, 
                                        .illuminant = image.illuminant });

    // Move texture into application - not project - data; stored in separate file
    loaded_texture = std::move(info.images[0].image);
    info.images.erase(info.images.begin()); // Youch

    fmt::print("Project generation\n");
    fmt::print("  Generating simplified convex hull\n");

    // Generate a simplified convex hull over texture data
    auto chull_mesh = generate_convex_hull<HalfedgeMeshTraits, eig::Array3f>(loaded_texture.data());
    auto chull_simp = simplify(chull_mesh, info.n_vertices);
    auto [verts, elems] = generate_data(chull_simp);

    // Store results with approximate values
    project_data.gamut_elems = elems;
    project_data.gamut_verts.resize(verts.size());
    std::ranges::transform(verts, project_data.gamut_verts.begin(), [](Colr c) {
      return ProjectData::Vert { .colr_i = c, .mapp_i = 0, .colr_j = { }, .mapp_j = { } };
    });

    // Continue only with the below solver steps if there are additional input images to serve as constraints
    guard(!info.images.empty());

    // Intermediate storage for computed barycentric weights and indices to positive weights
    std::vector<eig::Array<float, barycentric_weights, 1>> img_weights;
    std::vector<uint> img_indices;

    /* 1. Generate barycentric weights for the convex hull, given the input image;
          we quick hack reuse shader code from the rendering pipeline */
    {
      fmt::print("  Generating barycentric weights\n");

      const uint n = loaded_texture.size().prod();
      const uint n_div = ceil_div(n, 256u);

      // Create program object, reusing shader from gen_barycentric_weights
      gl::Program bary_program = {{ .type = gl::ShaderType::eCompute,
                                    .path = "resources/shaders/gen_barycentric_weights/gen_barycentric_weights.comp.spv_opt",
                                    .is_spirv_binary = true }};

      // Initialize uniform buffer layout
      struct UniformBuffer { uint n, n_verts, n_elems; } uniform_buffer = {
        .n = n,
        .n_verts = static_cast<uint>(verts.size()),
        .n_elems = static_cast<uint>(elems.size())
      };

      // Create relevant buffer objects containing properly aligned data
      auto al_verts = std::vector<eig::AlArray3f>(range_iter(verts));
      auto al_elems = std::vector<eig::AlArray3u>(range_iter(elems));
      gl::Buffer bary_vert_buffer = {{ .data = cnt_span<const std::byte>(al_verts) }};
      gl::Buffer bary_elem_buffer = {{ .data = cnt_span<const std::byte>(al_elems) }};
      gl::Buffer bary_unif_buffer  = {{ .data = obj_span<const std::byte>(uniform_buffer) }};
      gl::Buffer bary_colr_buffer = {{ .data = cast_span<const std::byte>(io::as_aligned(loaded_texture).data()) }};
      gl::Buffer bary_wght_buffer = {{ .size = loaded_texture.size().prod() * barycentric_weights * sizeof(float),
                                      .flags = gl::BufferCreateFlags::eStorageDynamic }};

      // Bind resources to buffer targets for upcoming shader dispatch
      bary_vert_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
      bary_elem_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);
      bary_colr_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 2);
      bary_wght_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 3);
      bary_unif_buffer.bind_to(gl::BufferTargetType::eUniform,       0);
      
      // Dispatch shader call and copy results to host memory
      gl::dispatch_compute({ .groups_x = n_div, .bindable_program = &bary_program });
      img_weights.resize(n);
      bary_wght_buffer.get(cnt_span<std::byte>(img_weights));

      // Obtain mask over indices of non-negative barycentric weights; in case the convex hull
      // estimation does not provide a perfect fit, as the decimation implementation is a bit wonky
      img_indices.clear();
      std::vector<uint> index_full(n);
      std::iota(range_iter(index_full), 0);
      std::copy_if(range_iter(index_full), std::back_inserter(img_indices),
        [&img_weights](uint i) { return (img_weights[i] >= 0).all(); });

      float positive_ratio = static_cast<float>(img_indices.size()) / static_cast<float>(n);
      fmt::print("  Barycentric weight fit: {}\n", positive_ratio);
    }

    // Relevant settings for the following section
    constexpr uint n_samples  = 8;
    constexpr uint n_attempts = 16;

    // Mutex for parallel solver runs
    std::mutex solver_mutex;
    float solver_error = std::numeric_limits<float>::max();
    fmt::print("  Starting solver runs\n");

    #pragma omp parallel for
    for (int _i = 0; _i < n_attempts; ++_i) {
      using Wght = eig::Matrix<float, barycentric_weights, 1>;

      // Data storage for the current attempt's random samples
      std::vector<uint> sample_indices(n_samples);
      std::vector<Wght> sample_bary(n_samples);
      std::vector<Colr> sample_colr_i(n_samples);
      std::vector<std::vector<Colr>> sample_colr_j(info.images.size());

      /* 1. Sample a random subset of pixels in the texture and obtain their color values */
      {
        auto colr_i_span = loaded_texture.data();

        // Define random distribution to sample non-negative weight indices
        std::random_device rd;
        std::mt19937 eng(rd());
        std::uniform_int_distribution<uint> distr(0, img_indices.size() - 1);

        // Draw random samples from said distribution
        std::vector<uint> samples(n_samples);
        std::ranges::generate(samples, [&]{ return distr(eng); });

        // Extract sampled data
        std::ranges::transform(samples, sample_indices.begin(), [&](uint i) { return img_indices[i]; });
        std::ranges::transform(sample_indices, sample_colr_i.begin(), [&](uint i) { return colr_i_span[i]; });
        std::ranges::transform(sample_indices, sample_bary.begin(), [&](uint i) { return img_weights[i]; });
        for (uint i = 0; i < info.images.size(); ++i) {
          auto colr_j_span = info.images[i].image.data();
          sample_colr_j[i] = std::vector<Colr>(n_samples);
          std::ranges::transform(sample_indices, sample_colr_j[i].begin(), [&](uint i) { return colr_j_span[i]; });
        }
      }

      // Intermediate storage for solved spectral gamut
      std::vector<Spec> gamut_spec;

      /* 2. Solve for a spectral gamut which satisfies the provided input*/
      bool solve_using_constraints = true;
      BBasis basis = loaded_basis.rightCols(wavelength_bases);
      if (solve_using_constraints) {
        // Solve using image constraints directly
        GenerateGamutConstraintInfo info = {
          .basis   = basis,
          .gamut   = verts,
          .systems = std::vector<CMFS>(project_data.mappings.size()),
          .signals = std::vector<GenerateGamutConstraintInfo::Signal>(n_samples)
        };

        // Transform mappings
        for (uint i = 0; i < project_data.mappings.size(); ++i)
          info.systems[i] = project_data.mapping_data(i).finalize();

        // Add baseline samples
        for (uint i = 0; i < n_samples; ++i)
          info.signals[i] = { .colr_v = sample_colr_i[i],
                              .bary_v = sample_bary[i],
                              .syst_i = 0 };

        // Add constraint samples
        for (uint i = 0; i < sample_colr_j.size(); ++i) {
          const auto &values = sample_colr_j[i];
          for (uint j = 0; j < n_samples; ++j) {
            info.signals.push_back({
              .colr_v = values[j],
              .bary_v = sample_bary[j],
              .syst_i = i + 1
            });
          }
        }

        // Fire solver and cross fingers
        gamut_spec = generate_gamut(info);
        gamut_spec.resize(verts.size());
      } else {
        // Generate spectral distributions for each sample
        std::vector<Spec> sample_spec(n_samples);
        std::vector<CMFS> sample_cmfs(project_data.mappings.size());
        for (uint i = 0; i < sample_cmfs.size(); ++i)
          sample_cmfs[i] = project_data.mapping_data(i).finalize();
        for (uint i = 0; i < n_samples; ++i) {
          std::vector<Colr> sample_signals = { sample_colr_i[i] };
          for (uint j = 0; j < sample_colr_j.size(); ++j)
            sample_signals.push_back(sample_colr_j[j][i]);
          sample_spec[i] = generate(basis, sample_cmfs, sample_signals);
        }
        
        // Solve using spectra generated from image constraints
        GenerateGamutSpectrumInfo info = {
          .basis   = basis,
          .system  = project_data.mapping_data(0).finalize(),
          .gamut   = verts,
          .weights = sample_bary,
          .samples = sample_spec
        };

        // Fire solver and cross fingers
        gamut_spec = generate_gamut(info);
        gamut_spec.resize(verts.size());
      }

      // Intermediate storage for vertices and constraints
      std::vector<ProjectData::Vert> gamut_verts;

      /* 3. Obtain vertices and constraints from spectral gamut, by applying known color systems */
      {
        for (uint i = 0; i < gamut_spec.size(); ++i) {
          const Spec &sd = gamut_spec[i];
          ProjectData::Vert vert;

          // Define vertex settings
          vert.colr_i = project_data.mapping_data(project_data.mappings[0]).apply_color(sd);
          vert.mapp_i = 0;

          // Define constraint settings
          for (uint j = 1; j < project_data.mappings.size(); ++j) {
            vert.colr_j.push_back(project_data.mapping_data(project_data.mappings[j]).apply_color(sd));
            vert.mapp_j.push_back(j);
          }

          gamut_verts.push_back(vert);
        }
      }

      // Intermediate error storage
      float roundtrip_error = 0.f;

      /* 4. Compute roundtrip error for the different inputs */
      {
        // Squared error based on offsets to the convex hull vertices
        for (uint i = 0; i < gamut_verts.size(); ++i)
          roundtrip_error += (gamut_verts[i].colr_i - verts[i]).pow(2.f).sum();

        // Squared error based on sample roundtrip
        /* for (uint i = 0; i < n_samples; ++i) {
          // Recover spectrum at sample position
          Wght w = sample_bary[i];
          Spec s = 0.f;
          for (uint j = 0; j < gamut_spec.size(); ++j)
            s += w[j] * gamut_spec[j];
          
          // Add baseline sample error
          Colr colr_i = project_data.mapping_data(0).apply_color(s);
          roundtrip_error += (sample_colr_i[i] - colr_i).pow(2.f).sum();

          // Add constraint sample error
          for (uint j = 0; j < sample_colr_j.size(); ++j) {
            Colr colr_j = project_data.mapping_data(j + 1).apply_color(s);
            roundtrip_error += (sample_colr_j[j][i] - colr_j).pow(2.f).sum();
          }
        } */
      }

      /* 5. Apply results */
      {
        std::lock_guard<std::mutex> lock(solver_mutex);
        if (roundtrip_error < solver_error) {
          project_data.gamut_verts = gamut_verts;
          solver_error = roundtrip_error;
          fmt::print("  Best error: {}\n", solver_error);
        }
      }
    }
  }
  
  void ApplicationData::save(const fs::path &path) {
    met_trace();

    project_save = SaveFlag::eSaved;
    project_path = io::path_with_ext(path, ".json");

    io::save_json(project_path, project_data);
    io::save_texture2d(io::path_with_ext(project_path, ".bmp"), loaded_texture, true);
  }

  void ApplicationData::load(const fs::path &path) {
    met_trace();

    project_save   = SaveFlag::eSaved;
    project_path   = io::path_with_ext(path, ".json");
    project_data   = io::load_json(path).get<ProjectData>();
    loaded_texture = io::load_texture2d<Colr>(io::path_with_ext(project_path,".bmp"), true);

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

    loaded_texture  = { };
    
    mods  = { };
    mod_i = -1;
  }  
} // namespace met