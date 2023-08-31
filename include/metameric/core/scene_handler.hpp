#pragma once

#include <metameric/core/scene.hpp>
#include <functional>

namespace met {
  /* Scene management class.
     Primary class handling create/load/save/export/state of the current scene */
  struct SceneHandler {
    enum class SaveState {
      eUnloaded, // Scene is not currently loaded by application
      eNew,      // Scene has no previous save, is newly created
      eSaved,    // Scene has previous save, and has not been modified
      eUnsaved,  // Scene has previous save, and has been modified
    };

    SaveState save_state = SaveState::eUnloaded; 
    fs::path  save_path  = "";
    Scene     scene      = {};

  public: // Scene loader handling
    void create();                   // Create scene from given object
    void load(const fs::path &path); // Load scene data from path 
    void save(const fs::path &path); // Save scene data to path
    void unload();                   // Clear out scene data

  public: // History (redo/undo) handling
    struct SceneMod {
      std::string name;
      std::function<void(Scene &)> redo, undo;
    };   

    std::vector<SceneMod> mods;       // Stack of data modifications
    int                   mod_i = -1; // Index of last modification for undo/redo 

    void touch(SceneMod &&mod); // Submit scene modification to history
    void redo_mod();            // Step forward one modification
    void undo_mod();            // Step back one modification
    void clear_mods();          // Clear entire modification state       

  public: // I/O handling
    // Export a specific uplifting model from the loaded scene to a texture file
    void export_uplifting(const fs::path &path, uint uplifting_i) const;

    // Import a wavefront .obj file, adding its components into the loaded scene
    void import_wavefront_obj(const fs::path &path);

    // Import an existing scene, adding its components into the loaded scene
    void import_scene(const fs::path &path);
    void import_scene(Scene &&other);
    
  public: // Scene component state handling
    void set_mutated(bool b) {
      met_trace();

      scene.observer_i.state.set_mutated(b);

      scene.components.objects.set_mutated(b);
      scene.components.emitters.set_mutated(b);
      scene.components.materials.set_mutated(b);
      scene.components.upliftings.set_mutated(b);
      scene.components.colr_systems.set_mutated(b);
      
      scene.resources.meshes.set_mutated(b);
      scene.resources.images.set_mutated(b);
      scene.resources.illuminants.set_mutated(b);
      scene.resources.observers.set_mutated(b);
      scene.resources.bases.set_mutated(b);
    }

    bool is_mutated() const {
      met_trace();
      return scene.observer_i.state 
          || scene.components.objects.is_mutated()
          || scene.components.emitters.is_mutated()
          || scene.components.materials.is_mutated()
          || scene.components.upliftings.is_mutated()
          || scene.components.colr_systems.is_mutated()
          || scene.resources.meshes.is_mutated()
          || scene.resources.images.is_mutated()
          || scene.resources.illuminants.is_mutated()
          || scene.resources.observers.is_mutated()
          || scene.resources.bases.is_mutated();
    }
  };
} // namespace met