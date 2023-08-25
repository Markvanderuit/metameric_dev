#include <metameric/core/scene_handler.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/tree.hpp>
#include <metameric/core/utility.hpp>
#include <nlohmann/json.hpp>

namespace met {
  void SceneHandler::create() {
    met_trace();

    // Clear out all scene handler state first
    scene      = { };
    save_path  = "";
    save_state = SaveState::eNew;
    clear_mods();

    // Pre-supply some data for an initial scene
    auto loaded_tree = io::load_json("resources/misc/tree.json").get<BasisTreeNode>();
    scene.bases.emplace("Default basis", {
      .mean      = loaded_tree.basis_mean,
      .functions = loaded_tree.basis
    });
    scene.observer_i = { .name = "Default observer", .value = 0 };
    scene.illuminants.push("D65",      models::emitter_cie_d65);
    scene.illuminants.push("E",        models::emitter_cie_e);
    scene.illuminants.push("FL2",      models::emitter_cie_fl2);
    scene.illuminants.push("FL11",     models::emitter_cie_fl11);
    scene.illuminants.push("LED-RGB1", models::emitter_cie_ledrgb1);
    scene.observers.push("CIE XYZ",    models::cmfs_cie_xyz);
    scene.upliftings.emplace("Default uplifting", {
      .type    = Uplifting::Type::eDelaunay,
      .basis_i = 0
    });
    Scene::ColrSystem csys { .observer_i = 0, .illuminant_i = 0, .n_scatters = 0 };
    scene.colr_systems.push(scene.get_csys_name(csys), csys);
  }

  void SceneHandler::save(const fs::path &path) {
    met_trace();

    io::save_scene(path, scene);

    save_path  = io::path_with_ext(path, ".json");
    save_state = SaveState::eSaved;
  }

  void SceneHandler::load(const fs::path &path) {
    met_trace();

    scene      = io::load_scene(path);
    save_path  = io::path_with_ext(path, ".json");
    save_state = SaveState::eSaved;

    clear_mods();
  }

  void SceneHandler::unload() {
    met_trace();

    scene      = { };
    save_path  = "";
    save_state = SaveState::eUnloaded;

    clear_mods();
  }

  void SceneHandler::touch(SceneHandler::SceneMod &&mod) {
    met_trace();

    // Apply change
    mod.redo(scene);

    // Ensure mod list doesn't exceed fixed length
    // and set the current mod to its end
    int n_mods = std::clamp(mod_i + 1, 0, 128);
    mod_i = n_mods;
    mods.resize(mod_i);
    mods.push_back(mod);   
    
    if (save_state == SaveState::eSaved)
      save_state = SaveState::eUnsaved;
  }

  void SceneHandler::redo_mod() {
    met_trace();
    
    guard(mod_i < (int(mods.size()) - 1));
    
    mod_i += 1;
    mods[mod_i].redo(scene);

    if (save_state == SaveState::eSaved)
      save_state = SaveState::eUnsaved;
  }

  void SceneHandler::undo_mod() {
    met_trace();
    
    guard(mod_i >= 0);

    mods[mod_i].undo(scene);
    mod_i -= 1;

    if (save_state == SaveState::eSaved)
      save_state = SaveState::eUnsaved;
  }

  void SceneHandler::clear_mods() {
    met_trace();

    mods  = { };
    mod_i = -1;
  }

  void SceneHandler::export_uplifting(const fs::path &path, uint uplifting_i) const {
    met_trace();

    debug::check_expr(false, "Not implemented!");
  }
} // namespace met