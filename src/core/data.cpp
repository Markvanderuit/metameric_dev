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
    project_save   = SaveFlag::eNew;
    project_path   = ""; // TBD on first save
    project_data   = ProjectData();

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

    // Move b texture into application - not project - data; stored in separate file
    loaded_texture = std::move(info.images[0].image);
    info.images.erase(info.images.begin()); // Youch

    // Generate a simplified convex hull over texture data
    auto chull_mesh = generate_convex_hull<HalfedgeMeshTraits, eig::Array3f>(loaded_texture.data());
    auto chull_simp = simplify(chull_mesh, info.n_vertices);
    auto [verts, elems] = generate_data(chull_simp);

    uint n_samples = 64;

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
        std::ranges::transform(subset_indices, constr_values[i].begin(), 
          [&](uint i) { return img_span[i]; });
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

    /* 4. Compute new gamut constraints for each secondary mapping
          w.r.t. the barycentric weights  */
    {
      // Obtain unaligned weights
      std::vector<std::vector<float>> weights;
      std::ranges::transform(aligned_weights, std::back_inserter(weights), [&](const auto &v) {
        std::vector<float> w(verts.size());
        std::copy(v.begin(), v.begin() + w.size(), w.begin());
        return w;
      });

      fmt::print("weights {}\n", weights);

      // For each set of constraints
      for (uint i = 0; i < constr_values.size(); ++i) {
        fmt::print("constr_values {} = {}\n", i + 1, constr_values[i]);

        // Solve for constrained gamut
        std::vector<Colr> constraint_gamut = generate_gamut(weights, constr_values[i]);
        fmt::print("gamut {} = {}\n", i + 1, constraint_gamut);

        // Assign data to each vertex
        for (uint j = 0; j < constraint_gamut.size(); ++j) {
          auto &vert = project_data.gamut_verts[j];
          vert.colr_j.push_back(constraint_gamut[j]);
          vert.mapp_j.push_back(i + 1); // 0 is base image, which was removed from the list
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