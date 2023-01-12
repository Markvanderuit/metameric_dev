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
#include <chrono>

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

  ColrSystem ProjectData::csys(uint i) const {
    met_trace();
    return csys(color_systems[i]);
  }

  ColrSystem ProjectData::csys(CSys m) const {
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
    project_data.color_systems.clear();
    for (auto &image : info.images)
      project_data.color_systems.push_back({ .cmfs       = image.cmfs, 
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
      return ProjectData::Vert { .colr_i = c, .csys_i = 0, .colr_j = { }, .csys_j = { } };
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
    constexpr uint n_samples  = 16;
    constexpr uint n_attempts = 16;

    // Mutex for safe solver sections
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
      if (solve_using_constraints) {
        auto t_start = std::chrono::steady_clock::now();

        // Solve using image constraints directly
        GenerateGamutConstraintInfo info = {
          .basis   = loaded_basis,
          .gamut   = verts,
          .systems = std::vector<CMFS>(project_data.color_systems.size()),
          .signals = std::vector<GenerateGamutConstraintInfo::Signal>(n_samples)
        };

        // Transform mappings
        for (uint i = 0; i < project_data.color_systems.size(); ++i)
          info.systems[i] = project_data.csys(i).finalize();

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

        auto t_end = std::chrono::steady_clock::now();
        auto t_duration = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start);

        fmt::print("  Solve time: {}ms\n", t_duration.count());
      } else {
        // Generate spectral distributions for each sample
        std::vector<Spec> sample_spec(n_samples);
        std::vector<CMFS> sample_cmfs(project_data.color_systems.size());
        for (uint i = 0; i < sample_cmfs.size(); ++i)
          sample_cmfs[i] = project_data.csys(i).finalize();
        for (uint i = 0; i < n_samples; ++i) {
          std::vector<Colr> sample_signals = { sample_colr_i[i] };
          for (uint j = 0; j < sample_colr_j.size(); ++j)
            sample_signals.push_back(sample_colr_j[j][i]);
          sample_spec[i] = generate_spectrum({
            .basis   = loaded_basis, 
            .systems = sample_cmfs, 
            .signals = sample_signals
          });
        }
        
        // Solve using spectra generated from image constraints
        GenerateGamutSpectrumInfo info = {
          .basis   = loaded_basis,
          .system  = project_data.csys(0).finalize(),
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
          vert.colr_i = project_data.csys(project_data.color_systems[0]).apply_color(sd);
          vert.csys_i = 0;

          // Define constraint settings
          for (uint j = 1; j < project_data.color_systems.size(); ++j) {
            vert.colr_j.push_back(project_data.csys(project_data.color_systems[j]).apply_color(sd));
            vert.csys_j.push_back(j);
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
          Colr colr_i = project_data.csys(0).apply_color(s);
          roundtrip_error += (sample_colr_i[i] - colr_i).pow(2.f).sum();

          // Add constraint sample error
          for (uint j = 0; j < sample_colr_j.size(); ++j) {
            Colr colr_j = project_data.csys(j + 1).apply_color(s);
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

  void ApplicationData::refit_convex_hull() {
    auto chull_mesh = generate_convex_hull<HalfedgeMeshTraits, eig::Array3f>(loaded_texture.data());
    auto chull_simp = simplify(chull_mesh, project_data.gamut_verts.size());
    auto [verts, elems] = generate_data(chull_simp);

    // Store results with approximate values
    project_data.gamut_elems = elems;
    project_data.gamut_verts.resize(verts.size());
    std::ranges::transform(verts, project_data.gamut_verts.begin(), [](Colr c) {
      return ProjectData::Vert { .colr_i = c, .csys_i = 0, .colr_j = { }, .csys_j = { } };
    });
  }

  void ApplicationData::solve_samples() {
    using Wght = eig::Matrix<float, barycentric_weights, 1>;

    guard(!project_data.sample_verts.empty());

    std::vector<Wght> sample_weights;      // Storage for sample barycentric_weights
    std::vector<Spec> gamut_spectra;       // Storage for generated gamut spectra
    std::vector<uint> sample_indices_safe; // Indices of samples which are deemed safe
    bool solve_using_constraints = true;   // Solve over color constraints, or alternatively spectra

    /* 0. Generate additional samples */
    {

    }

    /* 1. Generate barycentric weights for the given samples */
    {
      fmt::print("  Generating barycentric weights\n");

      const uint n = project_data.sample_verts.size();
      const uint n_div = ceil_div(n, 256u);

      // Create program object, reusing shader code from gen_barycentric_weights
      gl::Program bary_program = {{ .type = gl::ShaderType::eCompute,
                                    .path = "resources/shaders/gen_barycentric_weights/gen_barycentric_weights.comp.spv_opt",
                                    .is_spirv_binary = true }};
      
      // Initialize uniform buffer layout
      struct UniformBuffer { uint n, n_verts, n_elems; } uniform_buffer = {
        .n = n,
        .n_verts = static_cast<uint>(project_data.gamut_verts.size()),
        .n_elems = static_cast<uint>(project_data.gamut_elems.size())
      };

      // Obtain aligned gamut data
      auto al_elems = std::vector<eig::AlArray3u>(range_iter(project_data.gamut_elems));
      auto al_verts = std::vector<eig::AlArray3f>(project_data.gamut_verts.size());
      auto al_colrs = std::vector<eig::AlArray3f>(project_data.sample_verts.size());
      std::ranges::transform(project_data.gamut_verts, al_verts.begin(), [](auto &v) { return v.colr_i; });
      std::ranges::transform(project_data.sample_verts, al_colrs.begin(), [](auto &v) { return v.colr_i; });

      fmt::print("al_verts : {}\n", al_verts);
      fmt::print("al_colrs : {}\n", al_colrs);
      fmt::print("al_elems : {}\n", al_elems);

      // Create relevant buffer objects containing properly aligned data
      gl::Buffer bary_vert_buffer = {{ .data = cnt_span<const std::byte>(al_verts) }};
      gl::Buffer bary_elem_buffer = {{ .data = cnt_span<const std::byte>(al_elems) }};
      gl::Buffer bary_colr_buffer = {{ .data = cnt_span<const std::byte>(al_colrs) }};
      gl::Buffer bary_unif_buffer = {{ .data = obj_span<const std::byte>(uniform_buffer) }};
      gl::Buffer bary_wght_buffer = {{ .size = project_data.sample_verts.size() * barycentric_weights * sizeof(float),
                                      .flags = gl::BufferCreateFlags::eStorageDynamic }};
      
      // Bind resources to buffer targets for upcoming shader dispatch
      bary_vert_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
      bary_elem_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);
      bary_colr_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 2);
      bary_wght_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 3);
      bary_unif_buffer.bind_to(gl::BufferTargetType::eUniform,       0);
      
      // Dispatch shader call
      gl::dispatch_compute({ .groups_x = n_div, .bindable_program = &bary_program });

      // Copy computed barycentric weights back to host memory
      sample_weights.resize(n);
      bary_wght_buffer.get(cnt_span<std::byte>(sample_weights));
      
      fmt::print("Sample weights: {}\n", sample_weights);
    }

    /* 1.5 Verify barycentric weight correctness */
    {
      for (uint i = 0; i < sample_weights.size(); ++i) {
        Colr colr_a = project_data.sample_verts[i].colr_i;
        Colr colr_b = 0;
        for (uint j = 0; j < project_data.gamut_verts.size(); ++j)
          colr_b += project_data.gamut_verts[j].colr_i * sample_weights[i][j];

        fmt::print("Sample recovery: {} -> {}, error {}\n", colr_a, colr_b, (colr_b - colr_a).pow(2.f).sum());
      }
    }

    /* 2. Determine which samples are safe */
    {
      // Obtain mask over indices of non-negative barycentric weights; in case either the convex hull
      // estimation does not provide a perfect fit (the decimation implementation is wonky), or the
      // user has specified a non-fitting convex hull intentionally (or specified samples outside of the hull)
      sample_indices_safe.clear();
      std::vector<uint> indices_iota(sample_weights.size());
      std::iota(range_iter(indices_iota), 0);
      std::copy_if(range_iter(indices_iota), std::back_inserter(sample_indices_safe),
        [&sample_weights](uint i) { return (sample_weights[i].array() >= 0).all(); });
      
      // TODO: remove
      float positive_ratio = static_cast<float>(sample_indices_safe.size()) 
                           / static_cast<float>(sample_weights.size());
      fmt::print("Barycentric weight fit: {}\n", positive_ratio);
    }
    
    /* 3. Solve for a spectral gamut which satisfies the safe samples */
    if (solve_using_constraints) {
      GenerateGamutConstraintInfo info = {
        .basis   = loaded_basis,
        .gamut   = std::vector<Colr>(project_data.gamut_verts.size()), // project_data.gamut_verts,
        .systems = std::vector<CMFS>(project_data.color_systems.size())
      };

      // Add color systems and gamut data
      std::ranges::transform(project_data.color_systems, info.systems.begin(),
        [&](auto m) { return project_data.csys(m).finalize(); });
      std::ranges::transform(project_data.gamut_verts, info.gamut.begin(),
        [](auto &v) { return v.colr_i; });
      
      // Add sample data
      for (uint i = 0; i < project_data.sample_verts.size(); ++i) {
        const auto &v = project_data.sample_verts[i];
        info.signals.push_back({ .colr_v = v.colr_i, .bary_v = sample_weights[i], .syst_i = v.csys_i });
        for (uint j = 0; j < v.colr_j.size(); ++j)
          info.signals.push_back({ .colr_v = v.colr_j[j], .bary_v = sample_weights[i], .syst_i = v.csys_j[j] });
      }

      // Fire solver and cross fingers
      gamut_spectra = generate_gamut(info);
      gamut_spectra.resize(project_data.gamut_verts.size());
    } else {
      // ...
    }

    /* 4. Resolve constraints and store in gamut */
    for (uint i = 0; i < gamut_spectra.size(); ++i) {
      const Spec &sd = gamut_spectra[i];
      ProjectData::Vert vert;

      // Define vertex settings
      vert.colr_i = project_data.csys(project_data.color_systems[0]).apply_color(sd);
      vert.csys_i = 0;

      // Define constraint settings
      for (uint j = 1; j < project_data.color_systems.size(); ++j) {
        vert.colr_j.push_back(project_data.csys(project_data.color_systems[j]).apply_color(sd));
        vert.csys_j.push_back(j);
      }

      project_data.gamut_verts[i] = vert;
    }

    /* 3-4-alt. Solve for simplified constraints and store in gamut */
    /* {
      // Assumption; all samples have the same nr. and order of maps
      // TODO: fix this, do a gather step of mapping data instead
      uint n = project_data.sample_verts[0].csys_j.size();

      // Clear constraints from gamut
      for (auto &v : project_data.gamut_verts) {
        v.colr_j.clear();
        v.csys_j.clear();
      }
      
      // Resolve constraints and apply
      for (uint i = 0; i < n; ++i) {
        GenerateGamutSimpleInfo info = { .bary_weights = static_cast<uint>(project_data.gamut_verts.size()), 
                                         .weights = sample_weights };
        for (uint j = 0; j < project_data.sample_verts.size(); ++j)
          info.samples.push_back(project_data.sample_verts[j].colr_j[i]);
        
        std::vector<Colr> result = generate_gamut(info);
        result.resize(project_data.gamut_verts.size());
        for (uint j = 0; j < project_data.gamut_verts.size(); ++j) {
          project_data.gamut_verts[j].colr_j.push_back(result[j]);
          project_data.gamut_verts[j].csys_j.push_back(i + 1);
        }

        fmt::print("Result {}\n", result);
      }
    } */

    /* 5. Report sample recovery error */
    {
      for (uint i = 0; i < project_data.sample_verts.size(); ++i) {
        float roundtrip_error = 0.f;

        auto sample = project_data.sample_verts[i];
        auto weight = sample_weights[i];

        /* aut
        
        for (uint j = 0; j < sample.colr_j.size(); ++j) {
          Colr colr_j = sample.colr_j[j];

          Colr colr_j_offs = 0;
          for (uint k = 0; k < project_data.gamut_verts.size(); ++k) {
            colr_j_offs += weight[k] * project_data.gamut_verts[k].colr_j[j];
          }
          fmt::print("{} -> {}\n", colr_j, colr_j_offs);

          roundtrip_error += (colr_j - colr_j_offs).pow(2.f).sum();
        } */

        // Recover spectrum using barycentric weights
        Wght w = sample_weights[i];
        Spec s = 0.f;
        for (uint j = 0; j < gamut_spectra.size(); ++j)
          s += w[j] * gamut_spectra[j];
        
        // Add baseline sample error
        Colr colr_i = project_data.csys(sample.csys_i).apply_color(s);
        roundtrip_error += (sample.colr_i - colr_i).pow(2.f).sum();
        for (uint j = 0; j < sample.colr_j.size(); ++j) {
          Colr colr_j = project_data.csys(sample.csys_j[j]).apply_color(s);
          roundtrip_error += (sample.colr_j[j] - colr_j).pow(2.f).sum();
        }

        fmt::print("Roundtrip {} : {}\n", i, roundtrip_error);
      }
    }
  }
} // namespace met