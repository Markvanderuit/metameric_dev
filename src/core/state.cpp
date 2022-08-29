#include <metameric/core/state.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/utility.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>

namespace met {
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
    rgb_gamut = { Colr { .75f, .40f, .25f },
                  Colr { .68f, .49f, .58f },
                  Colr { .50f, .58f, .39f },
                  Colr { .35f, .30f, .34f }};
    spec_gamut = { 1.f, 1.f, 1.f, 1.f }; // TODO replace with sensible initialization

    // Instantiate loaded components with sensible default values
    mappings = {{" default", { 
      .cmfs       = "CIE XYZ->sRGB",
      .illuminant = "D65",
      .n_scatters = 0 
    }}};
    cmfs = {{ "CIE XYZ",       models::cmfs_cie_xyz },
            { "CIE XYZ->sRGB", models::cmfs_srgb    }};
    illuminants = {{ "D65",  models::emitter_cie_d65  },
                   { "E",    models::emitter_cie_e    },
                   { "FL2",  models::emitter_cie_fl2  },
                   { "FL11", models::emitter_cie_fl11 }};
  }

  Spec ProjectData::load_illuminant(const std::string &key) const {
    const auto pred = [&key](auto &p) { return key == p.first; };
    if (auto i = std::ranges::find_if(illuminants, pred); i != illuminants.end()) {
      return i->second;
    } else {
      // TODO: output a warning or something?
      return models::emitter_cie_d65;
    }
  }

  CMFS ProjectData::load_cmfs(const std::string &key) const {
    const auto pred = [&key](auto &p) { return key == p.first; };
    if (auto i = std::ranges::find_if(cmfs, pred); i != cmfs.end()) {
      return i->second;
    } else {
      // TODO: output a warning or something?
      return models::cmfs_srgb;
    }
  }

  SpectralMapping ProjectData::load_mapping(const std::string &key) const {
    const auto pred = [&key](auto &p) { return key == p.first; };
    if (auto i = std::ranges::find_if(mappings, pred); i != mappings.end()) {
      auto &mapping = i->second;
      return { .cmfs       = load_cmfs(mapping.cmfs),
               .illuminant = load_illuminant(mapping.illuminant),
               .n_scatters = mapping.n_scatters };
    } else {
      // TODO: output a warning or something?
      return SpectralMapping();
    }
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
    mods  = { };
    mod_i = -1;

    load_mappings();
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
      [&](auto &p) { return project_data.load_mapping(p.first); });
  }
} // namespace met