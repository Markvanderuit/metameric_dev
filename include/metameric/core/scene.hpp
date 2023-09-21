#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/image.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/scene_components.hpp>
#include <variant>

namespace met {
  /* Scene data layout.
     Simple indexed scene; no graph, just a library of objects and 
     their dependencies; responsible for most program data, as well
     as tracking of dependency modifications */
  struct Scene {
  public: // Scene data
    // Scene components, directly visible or influential in the scene
    // On-disk, these are stored in json format
    struct {
      detail::Components<ColorSystem> colr_systems;
      detail::Components<Emitter>     emitters;
      detail::Components<Object>      objects;
      detail::Components<Uplifting>   upliftings;
      detail::Component<Settings>     settings;   // Miscellaneous settings; e.g. texture size
      detail::Component<uint>         observer_i; // Primary observer index; simple enough for now
    } components;

    // Scene resources, primarily referred to by components in the scene
    // On-disk, these are stored in zlib-compressed binary format
    struct {
      detail::Resources<AlMesh> meshes;
      detail::Resources<Image>  images;
      detail::Resources<Spec>   illuminants;
      detail::Resources<CMFS>   observers;
      detail::Resources<Basis>  bases;
    } resources;
    
  public: // Save state and IO handling
    enum class SaveState {
      eUnloaded, // Scene is not currently loaded by application
      eNew,      // Scene has no previous save, is newly created
      eSaved,    // Scene has previous save, and has not been modified
      eUnsaved,  // Scene has previous save, and has been modified
    };

    SaveState save_state = SaveState::eUnloaded; 
    fs::path  save_path  = "";

    void create();                   // Create new, empty scene
    void load(const fs::path &path); // Load scene data from path 
    void save(const fs::path &path); // Save scene data to path
    void unload();                   // Clear out scene data

    // Export a specific uplifting model from the loaded scene to a texture file
    void export_uplifting(const fs::path &path, uint uplifting_i) const;

    // Import a wavefront .obj file, adding its components into the loaded scene
    void import_wavefront_obj(const fs::path &path);

    // Import an existing scene, adding its components into the loaded scene
    void import_scene(const fs::path &path);
    void import_scene(Scene &&other);

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

  public: // Scene data helper functions
    // Obtain a pretty-printed name of a certain color system
    std::string get_csys_name(uint i)                const;
    std::string get_csys_name(ColorSystem c) const;

    // Obtain the spectral data of a certain color system
    met::ColrSystem get_csys(uint i)                const;
    met::ColrSystem get_csys(ColorSystem c) const;

    // Obtain the spectral data of a certain emitter
    met::Spec get_emitter_spd(uint i)             const;
    met::Spec get_emitter_spd(Emitter e) const;
    
  public: // Serialization
    void to_stream(std::ostream &str) const;
    void fr_stream(std::istream &str);
  };
} // namespace met