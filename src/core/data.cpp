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

namespace met {
  constexpr uint chull_vertex_count = 5;

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

    // Generate a simplified convex hull over texture data
    auto chull_mesh = generate_convex_hull<HalfedgeMeshTraits, eig::Array3f>(loaded_texture.data());
    auto chull_simp = simplify(chull_mesh, info.n_vertices);
    auto [verts, elems] = generate_data(chull_simp);

    uint n_samples = 32;

    std::vector<Wght> weights;
    std::vector<Colr> samples;

    // Store results with approximate values
    project_data.gamut_elems = elems;
    project_data.gamut_verts.resize(verts.size());
    std::ranges::transform(verts, project_data.gamut_verts.begin(), [](Colr c) {
      return ProjectData::Vert { .colr_i = c, .mapp_i = 0, .colr_j = { }, .mapp_j = { } };
    });

    // Continue only with the below solver steps if there are additional input images to serve as constraints
    guard(!info.images.empty());

    /* 1. Generate barycentric weights for the convex hull, given the input image;
          we quick hack reuse shader code from the rendering pipeline */
    std::vector<eig::Array<float, barycentric_weights, 1>> img_weights;
    {
      // Obtain image and mesh data in aligned format
      auto aligned_colrs = io::as_aligned(loaded_texture);
      std::vector<eig::AlArray3f> aligned_verts(verts.size());
      std::vector<eig::AlArray3u> aligned_elems(elems.size());
      std::ranges::copy(verts, aligned_verts.begin());
      std::ranges::copy(elems, aligned_elems.begin());

      // Define uniform buffer layout and data
      struct UniformBufferLayout { uint n, n_verts, n_elems; } unif_layout { 
        .n       = aligned_colrs.size().prod(), 
        .n_verts = static_cast<uint>(verts.size()), 
        .n_elems = static_cast<uint>(elems.size()) 
      };

      // Create relevant buffer objects
      gl::Buffer vert_buffer = {{ .data = cnt_span<const std::byte>(aligned_verts) }};
      gl::Buffer elem_buffer = {{ .data = cnt_span<const std::byte>(aligned_elems) }};
      gl::Buffer colr_buffer = {{ .data = cast_span<const std::byte>(aligned_colrs.data()) }};
      gl::Buffer bary_buffer = {{ .size = unif_layout.n * barycentric_weights * sizeof(float),
                                  .flags = gl::BufferCreateFlags::eStorageDynamic }};
      gl::Buffer unif_buffer = {{ .data = obj_span<const std::byte>(unif_layout) }};

      // Clear out unused data early
      aligned_colrs = { };
      aligned_verts = { };
      aligned_elems = { };

      // Create program object
      gl::Program program = {{ .type = gl::ShaderType::eCompute,
                               .path = "resources/shaders/gen_barycentric_weights/gen_barycentric_weights.comp.spv_opt",
                               .is_spirv_binary = true }};
      
      // Bind resources to buffer targets for upcoming shader dispatch
      vert_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
      elem_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);
      colr_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 2);
      bary_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 3);
      unif_buffer.bind_to(gl::BufferTargetType::eUniform,       0);
      
      // Dispatch shader call for 256-sized workgroups and copy results to aligned_weights
      gl::dispatch_compute({ .groups_x = ceil_div(unif_layout.n, 256u), .bindable_program = &program });
      
      // Copy results to host
      img_weights.resize(unif_layout.n);
      bary_buffer.get(cnt_span<std::byte>(img_weights));
    }

    /* 2. Pick a random subset of pixels in the texture, obtain color values for each texture */
    std::vector<uint> subset_indices(n_samples);
    std::vector<Colr> subset_values(n_samples);
    std::vector<eig::Array<float, barycentric_weights, 1>> 
                      subset_weights(n_samples);
    std::vector<std::vector<Colr>> constr_values(info.images.size());
    {
      auto img_0_span = loaded_texture.data();

      // Obtain mask over indices of non-negative weights;
      // This is in case the convex hull does not provide a perfect fit, as its implementation
      // is admittedly a bit meh
      std::vector<uint> index_full(img_weights.size());
      std::vector<uint> index_mask;
      std::iota(range_iter(index_full), 0);
      std::copy_if(range_iter(index_full), std::back_inserter(index_mask),
        [&img_weights](uint i) { return (img_weights[i] >= 0).all(); });

      // Define random distribution to sample non-negative weight indices
      std::random_device rd;
      std::mt19937 eng(rd());
      std::uniform_int_distribution<uint> distr(0, index_mask.size() - 1);

      // Draw random samples from said distribution
      std::vector<uint> sample_indices(n_samples);
      std::ranges::generate(sample_indices, [&]{ return distr(eng); });

      // Extract sampled data
      std::ranges::transform(sample_indices, subset_indices.begin(), [&](uint i) { return index_mask[i]; });
      std::ranges::transform(subset_indices, subset_values.begin(), [&](uint i) { return img_0_span[i]; });
      std::ranges::transform(subset_indices, subset_weights.begin(), [&](uint i) { return img_weights[i]; });
      for (uint i = 0; i < constr_values.size(); ++i) {
        auto img_span = info.images[i].image.data();
        constr_values[i] = std::vector<Colr>(n_samples);
        std::ranges::transform(subset_indices, constr_values[i].begin(), [&](uint i) { return img_span[i]; });
      }
    }

    for (auto wght : subset_weights) {
      if ((wght < 0).any())
        fmt::print("NEG!");
      if (wght.isNaN().any())
        fmt::print("NAN!");
    }
    fmt::print("\n");

    /* 5. Solve for spectra at the gamut vertex positions based on
          these samples. */
    std::vector<Spec> gamut_spectra;
    {
      GenerateGamutInfo info = {
        .basis   = loaded_basis.rightCols(wavelength_bases),
        .gamut   = verts,
        .systems = std::vector<CMFS>(project_data.mappings.size()),
        .signals = std::vector<GenerateGamutInfo::Signal>(n_samples)
      };

      // Transform mappings
      for (uint i = 0; i < project_data.mappings.size(); ++i)
        info.systems[i] = project_data.mapping_data(i).finalize();

      // Add baseline samples
      for (uint i = 0; i < n_samples; ++i) {
        guard_continue((subset_weights[i] >= 0).all());
        info.signals[i] = { .colr_v = subset_values[i],
                            .bary_v = subset_weights[i],
                            .syst_i = 0 };
      }

      // Add constraint samples
      for (uint i = 0; i < constr_values.size(); ++i) {
        const auto &values = constr_values[i];
        for (uint j = 0; j < n_samples; ++j) {
          guard_continue((subset_weights[j] >= 0).all());
          info.signals.push_back({
            .colr_v = values[j],
            .bary_v = subset_weights[j],
            .syst_i = i + 1
          });
        }
      }

      // Search for a set of viable constraints until minimal error is achieved
      struct GenerateResult {
        float                          error;
        std::vector<ProjectData::Vert> verts;
      };

      constexpr float cutoff_err = 0.001f;
      constexpr int   max_iters = 10;

      std::vector<GenerateResult> results(max_iters);
      std::for_each(std::execution::par_unseq, range_iter(results), [&](GenerateResult &result) {
        std::vector<Spec> gamut = generate_gamut(info);
        gamut.resize(verts.size());

        result = { .error = 0.f };
        for (uint i = 0; i < verts.size(); ++i) {
          const Spec &sd = gamut[i];

          ProjectData::Vert vert;
          vert.colr_i = project_data.mapping_data(project_data.mappings[0]).apply_color(sd);
          vert.mapp_i = 0;
          for (uint j = 1; j < project_data.mappings.size(); ++j) {
            vert.colr_j.push_back(project_data.mapping_data(project_data.mappings[j]).apply_color(sd));
            vert.mapp_j.push_back(j);
          }
          
          result.verts.push_back(vert);
          result.error += (vert.colr_i - verts[i]).pow(2.f).sum();
        }

        fmt::print("Error: {}\n", result.error);
      });


      GenerateResult result = *std::min_element(range_iter(results),
        [](const auto &a, const auto &b) { return a.error < b.error; });
      project_data.gamut_verts = result.verts;

      // gamut_spectra = generate_gamut(info);
      // gamut_spectra.resize(verts.size());
    }

    // fmt::print("gamut_spectra :\n");
    // for (auto &s : gamut_spectra)
    //   fmt::print("\t{}\n", s);
    
    /* 6. Obtain constraint color offsets from these spectra by simply
          applying each color system */
   /*  {
      for (uint i = 0; i < gamut_spectra.size(); ++i) {
        const Spec &sd = gamut_spectra[i];
        auto &vert     = project_data.gamut_verts[i];

        Colr colr_new = project_data.mapping_data(project_data.mappings[0]).apply_color(sd);
        fmt::print("{} -> {} : err is {}\n", vert.colr_i, colr_new, (colr_new - vert.colr_i).eval());
        // auto dist = vert.colr_i - colr_new;
        vert.colr_i = colr_new;
        for (uint j = 1; j < project_data.mappings.size(); ++j) {
          vert.colr_j.push_back(project_data.mapping_data(project_data.mappings[j]).apply_color(sd));
          vert.mapp_j.push_back(j);
        }
      }
    } */

    // fmt::print("Bye!\n");
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
  
  void ApplicationData::load_chull_gamut() {
    met_trace();

    // Instantiate decimated approximate convex hull to place initial project gamut vertices
    auto chull_mesh = generate_convex_hull<HalfedgeMeshTraits, eig::Array3f>(loaded_texture.data());
    auto chull_simp = simplify(chull_mesh, chull_vertex_count);
    auto [verts, elems] = generate_data(chull_simp);

    // Assign new default gamut matching the convex hull
    project_data.gamut_elems  = elems;
    project_data.gamut_verts.resize(verts.size());
    std::ranges::transform(verts, project_data.gamut_verts.begin(), [](Colr c) {
      return ProjectData::Vert { .colr_i = c, .mapp_i = 0, .colr_j = { c }, .mapp_j = { 1 } };
    });
  }
} // namespace met