#include <metameric/core/data.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
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

  ProjectCreateInfo::ProjectCreateInfo()
  : n_vertices(4),
    illuminants({{ "D65",  models::emitter_cie_d65  },
                 { "E",    models::emitter_cie_e    },
                 { "FL2",  models::emitter_cie_fl2  },
                 { "FL11", models::emitter_cie_fl11 }}),
    cmfs({{ "CIE XYZ", models::cmfs_cie_xyz  }}) { }

  ProjectData::ProjectData() {
    met_trace();
    
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

    // Copy over cmfs/illuminants
    project_data.cmfs        = info.cmfs;
    project_data.illuminants = info.illuminants;
    project_data.mappings.clear();
    for (auto &image : info.images)
      project_data.mappings.push_back({ .cmfs       = image.cmfs, 
                                        .illuminant = image.illuminant });

    // Quick approximation
    loaded_texture = std::move(info.images[0].image);
    auto chull_mesh = generate_convex_hull<HalfedgeMeshTraits, eig::Array3f>(loaded_texture.data());
    auto chull_simp = simplify(chull_mesh, info.n_vertices);
    auto [verts, elems] = generate_data(chull_simp);

    // Store results with approximate values
    project_data.gamut_elems = elems;
    project_data.gamut_verts.resize(verts.size());
    std::ranges::transform(verts, project_data.gamut_verts.begin(), [](Colr c) {
      return ProjectData::Vert { .colr_i = c, .mapp_i = 0, .colr_j = { }, .mapp_j = { } };
    });

    /* 1. Compute a convex hull around the baseline texture */

    /* 2. Pick a random subset of pixels in the texture */

    /* 3. Generate barycentric weights of this subset w.r.t. the hull */

    /* 4. Compute new gamut constraints for each secondary mapping
          w.r.t. the barycentric weights  */
  }

  // Forward loaded texture to ApplicationData::create(Texture2d3f &&)
  void ApplicationData::create(const fs::path &texture_path) {
    met_trace();
    create(io::load_texture2d<Colr>(texture_path, true));
  }

  void ApplicationData::create(Texture2d3f &&texture) {
    met_trace();

    project_save   = SaveFlag::eNew;
    project_path   = ""; // TBD on first save
    project_data   = ProjectData();
    loaded_texture = std::move(texture);

    // Reset undo/redo history
    mods  = { };
    mod_i = -1;

    load_chull_gamut();
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