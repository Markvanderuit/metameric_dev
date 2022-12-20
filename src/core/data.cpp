#include <metameric/core/data.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/utility.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <ranges>

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

  ProjectData::ProjectData() {
    // Provide an initial example gamut for now
    gamut_elems = { Elem { 2, 0, 1 },
                    Elem { 0, 3, 1 },
                    Elem { 2, 1, 3 },
                    Elem { 0, 2, 3 }};
    gamut_verts = { Vert { .colr_i = { .75f, .40f, .25f }, .mapp_i = 0, .colr_j = { Colr { .75f, .40f, .25f } }, .mapp_j = { 1 } },
                    Vert { .colr_i = { .68f, .49f, .58f }, .mapp_i = 0, .colr_j = { Colr { .68f, .49f, .58f } }, .mapp_j = { 1 } },
                    Vert { .colr_i = { .50f, .58f, .39f }, .mapp_i = 0, .colr_j = { Colr { .50f, .58f, .39f } }, .mapp_j = { 1 } },
                    Vert { .colr_i = { .35f, .30f, .34f }, .mapp_i = 0, .colr_j = { Colr { .35f, .30f, .34f } }, .mapp_j = { 1 } } };

    // Instantiate loaded components with sensible default values
    mappings    = {{ .cmfs = 0, .illuminant = 0 }, 
                   { .cmfs = 0, .illuminant = 1 }};
    cmfs        = {{ "CIE XYZ", models::cmfs_cie_xyz  }};
    illuminants = {{ "D65",  models::emitter_cie_d65  },
                   { "E",    models::emitter_cie_e    },
                   { "FL2",  models::emitter_cie_fl2  },
                   { "FL11", models::emitter_cie_fl11 }};
  }

  Mapp ProjectData::mapping_data(uint i) const {
    return mapping_data(mappings[i]);
  }

  Mapp ProjectData::mapping_data(Mapp m) const {
    return { .cmfs = cmfs[m.cmfs].second,
             .illuminant = illuminants[m.illuminant].second };
  }

  void ApplicationData::create(const fs::path &texture_path) {
    // Forward loaded texture to ApplicationData::create(Texture2d3f &&)
    create(io::load_texture2d<Colr>(texture_path, true));
  }

  void ApplicationData::create(Texture2d3f &&texture) {
    project_save  = SaveFlag::eNew;
    project_path   = ""; // TBD on first save
    project_data   = ProjectData();
    loaded_texture = std::move(texture);

    // Reset undo/redo history
    mods  = { };
    mod_i = -1;

    load_chull_gamut();
  }
  
  void ApplicationData::save(const fs::path &path) {
    project_save = SaveFlag::eSaved;
    project_path  = io::path_with_ext(path, ".json");
    io::save_json(project_path, project_data);
    io::save_texture2d(io::path_with_ext(project_path, ".bmp"), loaded_texture, true);
  }

  void ApplicationData::load(const fs::path &path) {
    project_save  = SaveFlag::eSaved;
    project_path   = io::path_with_ext(path, ".json");
    project_data   = io::load_json(path).get<ProjectData>();
    loaded_texture = io::load_texture2d<Colr>(io::path_with_ext(project_path,".bmp"), true);

    // Reset undo/redo history
    mods  = { };
    mod_i = -1;
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
    
    if (project_save == SaveFlag::eSaved) {
      project_save = SaveFlag::eUnsaved;
    }
  }

  void ApplicationData::redo() {
    fmt::print("redo {} -> {}\n", mod_i, mod_i + 1);
    
    guard(mod_i < (int(mods.size()) - 1));
    mod_i += 1;
    mods[mod_i].redo(project_data);

    if (project_save == SaveFlag::eSaved) {
      project_save = SaveFlag::eUnsaved;
    }
  }

  void ApplicationData::undo() {
    fmt::print("undo {} -> {}\n", mod_i, mod_i - 1);

    guard(mod_i >= 0);
    mods[mod_i].undo(project_data);
    mod_i -= 1;

    if (project_save == SaveFlag::eSaved) {
      project_save = SaveFlag::eUnsaved;
    }
  }

  void ApplicationData::unload() {
    project_save = SaveFlag::eUnloaded;
    project_path  = "";
    project_data  = { };

    loaded_texture  = { };
    
    mods  = { };
    mod_i = -1;
  }

 /*  namespace detail {
    Spec load_illuminant(const ProjectData &data, std::string_view key) {
      const auto it = std::ranges::find_if(data.illuminants, 
        [&key](auto &p) { return key == p.first; });
      debug::check_expr_rel(it != data.illuminants.end(), 
        fmt::format("Could not load spectrum from project data; name was \"{}\"", key));
      return it->second; 
    }

    CMFS load_cmfs(const ProjectData &data, std::string_view key) {
      const auto it = std::ranges::find_if(data.cmfs, 
        [&key](auto &p) { return key == p.first; });
      debug::check_expr_rel(it != data.cmfs.end(), 
        fmt::format("Could not load spectrum from project data; name was \"{}\"", key));
      return it->second; 
    }

    Mapp load_mapping(const ProjectData &data, ProjectData::Mapp mapping) {
      return { .cmfs      = load_cmfs(data, mapping.cmfs),
              .illuminant = load_illuminant(data, mapping.illuminant) };
    }
  } // namespace detail */
  
  void ApplicationData::load_chull_gamut() {
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