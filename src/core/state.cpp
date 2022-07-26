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
    rgb_gamut = { Color { .75f, .40f, .25f },
                  Color { .68f, .49f, .58f },
                  Color { .50f, .58f, .39f },
                  Color { .35f, .30f, .34f }};
    spec_gamut = { 1.f, 1.f, 1.f, 1.f }; // TODO replace with sensible initialization

    // Instantiate loaeded components with sensible values
    spectral_mappings = {{ "default", SpectralMapping() }};
    loaded_cmfs = {{ "CIE XYZ",         models::cmfs_cie_xyz },
                   { "CIE XYZ -> sRGB", models::cmfs_srgb}};
    loaded_illuminants = {{ "D65", models::emitter_cie_d65 },
                          { "E", models::emitter_cie_e },
                          { "FL2", models::emitter_cie_fl2 },
                          { "FL11", models::emitter_cie_fl11 }};
  }

  void ApplicationData::create(Texture2d3f &&texture) {
    project_state = ProjectState::eNew;
    project_path  = ""; // TBD on first save
    project_data  = ProjectData();
    rgb_texture   = texture;
    mods          = { };
    mod_i         = -1;
  }

  void ApplicationData::create(const fs::path &texture_path) {
    project_state = ProjectState::eNew;
    project_path  = ""; // TBD on first save
    project_data  = ProjectData();
    rgb_texture   = Texture2d3f {{ .path = texture_path }};
    mods          = { };
    mod_i         = -1;
  }
  
  void ApplicationData::save(const fs::path &save_path) {
    project_state = ProjectState::eSaved;
    project_path  = io::path_with_ext(save_path, ".json");
    io::save_project(project_path, project_data);
    io::save_texture2d(io::path_with_ext(project_path, ".bmp"), rgb_texture);
  }

  void ApplicationData::load(const fs::path &load_path) {
    project_state = ProjectState::eSaved;
    project_path  = io::path_with_ext(load_path, ".json");
    project_data  = io::load_project(project_path);
    rgb_texture   = io::load_texture2d<Color>(io::path_with_ext(project_path,".bmp"));
    mods          = { };
    mod_i         = -1;
  }

  void ApplicationData::touch(ProjectMod &&mod) {
    int n_mods = std::clamp(mod_i + 1, 0, 128);
    mod_i = n_mods;

    mod.redo(project_data);
    mods.resize(mod_i);
    mods.push_back(mod);   

    // Ensure mod list doesn't exceed fixed length
    // and set the current mod to its end
    // mods.resize(std::min(mods.size(), 127ull));
    // mods.push_back(mod);   
    // mod_i = int(mods.size()) - 1;

    fmt::print("touch {}\n", mod_i);
    
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
    rgb_texture   = { };
    mods          = { };
    mod_i         = -1;
  }
} // namespace met