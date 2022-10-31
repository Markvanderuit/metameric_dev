#include <metameric/core/state.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/utility.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>

namespace met {
  constexpr uint chull_vertex_count = 4;

  namespace io {
    ProjectData load_project(const fs::path &path) {
      return load_json(path).get<ProjectData>();
    }

    void save_project(const fs::path &path, const ProjectData &data) {
      save_json(path, data);
    }
  } // namespace io

  ProjectData::ProjectData() {
    // Provide an initial example gamut for now
    gamut_colr_i = { Colr { .75f, .40f, .25f },
                     Colr { .68f, .49f, .58f },
                     Colr { .50f, .58f, .39f },
                     Colr { .35f, .30f, .34f }};
    gamut_offs_j = { Colr { 0.f, 0.f, 0.f },
                     Colr { 0.f, 0.f, 0.f },
                     Colr { 0.f, 0.f, 0.f },
                     Colr { 0.f, 0.f, 0.f }};
    gamut_mapp_i.fill(0);
    gamut_mapp_j.fill(1);
    gamut_spec.fill(Spec(0.f));

    // Instantiate loaded components with sensible default values
    mappings = {{ "D65", { .cmfs = "CIE XYZ->sRGB", .illuminant = "D65", .n_scatters = 0 }}, 
                { "FL11", { .cmfs = "CIE XYZ->sRGB", .illuminant = "FL11", .n_scatters = 0 }}};
    cmfs = {{ "CIE XYZ",       models::cmfs_cie_xyz },
            { "CIE XYZ->sRGB", models::cmfs_srgb    }};
    illuminants = {{ "D65",  models::emitter_cie_d65  },
                   { "E",    models::emitter_cie_e    },
                   { "FL2",  models::emitter_cie_fl2  },
                   { "FL11", models::emitter_cie_fl11 }};
  }

  void ApplicationData::create(const fs::path &texture_path) {
    // Forward loaded texture to ApplicationData::create(Texture2d3f &&)
    create(Texture2d3f {{ .path = texture_path }});
  }

  void ApplicationData::create(Texture2d3f &&texture) {
    project_state  = ProjectState::eNew;
    project_path   = ""; // TBD on first save
    project_data   = ProjectData();
    loaded_texture = std::move(texture);

    // Reset undo/redo history
    mods  = { };
    mod_i = -1;

    load_mappings();
    load_chull_gamut();
  }
  
  void ApplicationData::save(const fs::path &save_path) {
    project_state = ProjectState::eSaved;
    project_path  = io::path_with_ext(save_path, ".json");
    io::save_project(project_path, project_data);
    io::save_texture2d(io::path_with_ext(project_path, ".bmp"), loaded_texture);
  }

  void ApplicationData::load(const fs::path &load_path) {
    project_state  = ProjectState::eSaved;
    project_path   = io::path_with_ext(load_path, ".json");
    project_data   = io::load_project(project_path);
    loaded_texture = io::load_texture2d<Colr>(io::path_with_ext(project_path,".bmp"));

    // Reset undo/redo history
    mods  = { };
    mod_i = -1;
    
    load_mappings();
  }

  void ApplicationData::touch(ProjectMod &&mod) {
    // Apply change
    mod.redo(project_data);

    // Ensure mod list doesn't exceed fixed length
    // and set the current mod to its end
    int n_mods = std::clamp(mod_i + 1, 0, 128);
    mod_i = n_mods;
    mods.resize(mod_i);
    mods.push_back(mod);   
    
    if (project_state == ProjectState::eSaved) {
      project_state = ProjectState::eUnsaved;
    }
  }

  void ApplicationData::redo() {
    fmt::print("redo {} -> {}\n", mod_i, mod_i + 1);
    
    guard(mod_i < (int(mods.size()) - 1));
    mod_i += 1;
    mods[mod_i].redo(project_data);

    if (project_state == ProjectState::eSaved) {
      project_state = ProjectState::eUnsaved;
    }
  }

  void ApplicationData::undo() {
    fmt::print("undo {} -> {}\n", mod_i, mod_i - 1);

    guard(mod_i >= 0);
    mods[mod_i].undo(project_data);
    mod_i -= 1;

    if (project_state == ProjectState::eSaved) {
      project_state = ProjectState::eUnsaved;
    }
  }

  void ApplicationData::unload() {
    project_state = ProjectState::eUnloaded;
    project_path  = "";
    project_data  = { };

    loaded_texture  = { };
    loaded_mappings = { };
    
    mods  = { };
    mod_i = -1;
  }

  void ApplicationData::load_mappings() {
    loaded_mappings = { };
    std::ranges::transform(project_data.mappings, std::back_inserter(loaded_mappings), 
      [&](auto &p) { return load_mapping(p.first); });
  }
  
  void ApplicationData::load_chull_gamut() {
    // Instantiate approximate convex hull, simplified to a tetrahedron, to place initial 
    // project gamut vertices
    auto chull_mesh = generate_convex_hull<eig::Array3f>(loaded_texture.data());
    auto chull_tetr = simplify_mesh(chull_mesh, chull_vertex_count);
    std::copy(range_iter(chull_tetr.verts()), project_data.gamut_colr_i.begin());
  }

  Spec ApplicationData::load_illuminant(const std::string &key) const {
    const auto it = std::ranges::find_if(project_data.illuminants, 
      [&key](auto &p) { return key == p.first; });
    debug::check_expr_rel(it != project_data.illuminants.end(), 
      fmt::format("Could not load spectrum from project data; name was \"{}\"", key));
    return it->second; 
  }

  CMFS ApplicationData::load_cmfs(const std::string &key) const {
    const auto it = std::ranges::find_if(project_data.cmfs, 
      [&key](auto &p) { return key == p.first; });
    debug::check_expr_rel(it != project_data.cmfs.end(), 
      fmt::format("Could not load color matching functions from project data; name was \"{}\"", key));
    return it->second; 
  }

  Mapp ApplicationData::load_mapping(const std::string &key) const {
    const auto it = std::ranges::find_if(project_data.mappings, 
      [&key](auto &p) { return key == p.first; });
    debug::check_expr_rel(it != project_data.mappings.end(), 
      fmt::format("Could not load spectral mapping from project data; name was \"{}\"", key));
    const auto &mapping = it->second;
    return { .cmfs       = load_cmfs(mapping.cmfs),
             .illuminant = load_illuminant(mapping.illuminant),
             .n_scatters = mapping.n_scatters };
  }
} // namespace met