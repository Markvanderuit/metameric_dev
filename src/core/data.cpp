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

    uint n_samples = 16;

    std::vector<Wght> weights;
    std::vector<Colr> samples;

    // Store results with approximate values
    project_data.gamut_elems = elems;
    project_data.gamut_verts.resize(verts.size());
    
    std::ranges::transform(verts, project_data.gamut_verts.begin(), [](Colr c) {
      return ProjectData::Vert { .colr_i = c, .mapp_i = 0, .colr_j = { }, .mapp_j = { } };
    });

    // Continue only with the below solver steps if there are additional constraints
    guard(!info.images.empty());

    /* 2. Pick a random subset of pixels in the texture, obtain color values for each texture */
    std::vector<uint> subset_indices(n_samples);
    std::vector<Colr> subset_values(n_samples);
    std::vector<std::vector<Colr>> constr_values(info.images.size());
    {
      auto img_0_span = loaded_texture.data();

      std::random_device rd;
      std::mt19937 eng(rd());
      std::uniform_int_distribution<uint> distr(0, img_0_span.size() - 1);
      std::ranges::generate(subset_indices, [&]{ return distr(eng); });

      std::ranges::transform(subset_indices, subset_values.begin(), 
        [&](uint i) { return img_0_span[i]; });

      for (uint i = 0; i < constr_values.size(); ++i) {
        auto img_span = info.images[i].image.data();
        constr_values[i] = std::vector<Colr>(n_samples);
        std::ranges::transform(subset_indices, constr_values[i].begin(), [&](uint i) { return img_span[i]; });
      }
    }

    /* 3. Generate barycentric weights of this subset w.r.t. the hull; 
          we quick hack reuse shader code from the uplifting pipelien */
    std::vector<eig::Array<float, barycentric_weights, 1>> aligned_weights(n_samples);
    {
      // Obtain mesh data in aligned format
      std::vector<eig::AlArray3f> aligned_verts(verts.size());
      std::vector<eig::AlArray3u> aligned_elems(elems.size());
      std::vector<AlColr>         aligned_colrs(subset_values.size());
      std::ranges::copy(verts, aligned_verts.begin());
      std::ranges::copy(elems, aligned_elems.begin());
      std::ranges::copy(subset_values, aligned_colrs.begin());

      // Define uniform buffer layout and data
      struct UniformBufferLayout { uint n, n_verts, n_elems; } unif_layout { 
        .n = n_samples, .n_verts = static_cast<uint>(verts.size()), .n_elems = static_cast<uint>(elems.size()) 
      };

      // Create buffer objects
      gl::Buffer vert_buffer = {{ .data = cnt_span<const std::byte>(aligned_verts) }};
      gl::Buffer elem_buffer = {{ .data = cnt_span<const std::byte>(aligned_elems) }};
      gl::Buffer colr_buffer = {{ .data = cnt_span<const std::byte>(aligned_colrs) }};
      gl::Buffer bary_buffer = {{ .size = barycentric_weights * n_samples * sizeof(float),
                                  .flags = gl::BufferCreateFlags::eStorageDynamic }};
      gl::Buffer unif_buffer = {{ .data = obj_span<const std::byte>(unif_layout) }};

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
      gl::dispatch_compute({ .groups_x = ceil_div(n_samples, 256u), .bindable_program = &program });
      bary_buffer.get(cnt_span<std::byte>(aligned_weights));      
    }

    /* 4. Solve for spectra for each constraint */
    std::vector<Spec> image_spectra;
    {
      auto basis = loaded_basis.rightCols(wavelength_bases);

      // Gather color system spectra for all mappings
      std::vector<CMFS> systems(project_data.mappings.size());
      for (uint i = 0; i < systems.size(); ++i)
        systems[i] = project_data.mapping_data(i).finalize();

      image_spectra.resize(n_samples);
      for (uint i = 0; i < n_samples; ++i) {
        // Gather color signals spectra for all mappings
        std::vector<Colr> signals = { subset_values[i] };
        for (uint j = 0; j < constr_values.size(); ++j)
          signals.push_back(constr_values[j][i]);

        // Perform solve step
        image_spectra[i] = generate(basis, systems, signals).min(1.f).max(0.f).eval();
        Colr c = project_data.mapping_data(0).apply_color(image_spectra[i]);
        fmt::print("{} -> {}\n", subset_values[i], c);
      }
    }

    /* 5. Solve for spectra at the gamut vertex positions based on
          these samples. */
    /* std::vector<Spec> gamut_spectra;
    {
      // Copy over weights to correct format (fix this later)
      std::vector<WSpec> weights(aligned_weights.size());
      std::ranges::copy(aligned_weights, weights.begin());

      // Assemble info struct
      GenerateSpectralGamutInfo info = {
        .basis   = loaded_basis.rightCols(wavelength_bases),
        .system  = project_data.mapping_data(0).finalize(),
        .gamut   = verts,
        .weights = weights,
        .samples = image_spectra
      };

      // Perform solver step
      gamut_spectra = generate_gamut(info);
      gamut_spectra.resize(verts.size());
    } */

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
      for (uint i = 0; i < n_samples; ++i)
        info.signals[i] = { .colr_v = subset_values[i],
                            .bary_v = aligned_weights[i],
                            .syst_i = 0 };

      // Add constraint samples
      for (uint i = 0; i < constr_values.size(); ++i) {
        const auto &values = constr_values[i];
        for (uint j = 0; j < n_samples; ++j)
          info.signals.push_back({
            .colr_v = values[j],
            .bary_v = aligned_weights[j],
            .syst_i = i + 1
          });
      }

      gamut_spectra = generate_gamut(info);
      gamut_spectra.resize(verts.size());
    }

    fmt::print("gamut_spectra :\n");
    for (auto &s : gamut_spectra)
      fmt::print("\t{}\n", s);
    
    /* 6. Obtain constraint color offsets from these spectra by simply
          applying each color system */
    {
      for (uint i = 0; i < gamut_spectra.size(); ++i) {
        const Spec &sd = gamut_spectra[i];
        auto &vert     = project_data.gamut_verts[i];

        Colr colr_new = project_data.mapping_data(project_data.mappings[0]).apply_color(sd);
        fmt::print("{} -> {}\n", vert.colr_i, colr_new);
        vert.colr_i = colr_new;
        for (uint j = 1; j < project_data.mappings.size(); ++j) {
          vert.colr_j.push_back(project_data.mapping_data(project_data.mappings[j]).apply_color(sd));
          vert.mapp_j.push_back(j);
        }
      }
    }

    fmt::print("Bye!\n");
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