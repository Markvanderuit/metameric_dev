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

  public: // Scene management
    void create();                   // Create scene from given object
    void load(const fs::path &path); // Load scene data from path 
    void save(const fs::path &path); // Save scene data to path
    void unload();                   // Clear out scene data

  public: // History (redo/undo) management
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

  public: // Miscellaneous
    void export_uplifting(const fs::path &path, uint uplifting_i) const;
  };
} // namespace met