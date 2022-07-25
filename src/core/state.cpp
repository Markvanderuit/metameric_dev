#include <metameric/core/state.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/utility.hpp>
#include <nlohmann/json.hpp>

namespace met {
  namespace io {
    ProjectData load_project(const std::filesystem::path &path) {
      json js = load_json(path);
      return js.get<ProjectData>();
    }

    void save_project(const std::filesystem::path &path, const ProjectData &data) {
      json js = data;
      save_json(path, js);
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

  void ApplicationData::load(const std::filesystem::path &path) {
    project_path  = path;
    project_data  = io::load_project(path);
    rgb_texture   = Texture2d3f {{ .path = project_path.replace_extension(".bmp") }};
    project_state = ProjectState::eSaved;
  }
  
  void ApplicationData::save(const std::filesystem::path &path) {
    project_path = path;
    io::save_project(project_path, project_data);
    io::save_texture2d(project_path.replace_extension(".bmp"), rgb_texture);
    project_state = ProjectState::eSaved;
  }

  void ApplicationData::clear() {
    project_state = ProjectState::eUnloaded;
    project_path = "";
    project_data = {};
    rgb_texture = {};
  }
} // namespace met