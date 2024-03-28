#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/image.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/record.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/components.hpp>
#include <metameric/core/detail/scene_components_gl.hpp>
#include <metameric/core/detail/scene_components_state.hpp>
#include <variant>

namespace met {
  /* Define maximum nr. of supported components for some types
     These aren't device limits, but mostly exist so some sizes
     can be hardcoded shader-side */
  constexpr static uint max_supported_meshes     = MET_SUPPORTED_MESHES;
  constexpr static uint max_supported_objects    = MET_SUPPORTED_OBJECTS;
  constexpr static uint max_supported_emitters   = MET_SUPPORTED_EMITTERS;
  constexpr static uint max_supported_upliftings = MET_SUPPORTED_UPLIFTINGS;
  constexpr static uint max_supported_textures   = MET_SUPPORTED_TEXTURES;

  /* Scene data layout.
     Simple indexed scene; no graph, just a library of objects and 
     their dependencies; responsible for most program data, as well
     as tracking of dependency modifications */
  struct Scene {
    // Scene components, directly visible or influential in the scene (stored as json on disk)
    struct SceneComponents {
      detail::Components<ColorSystem> colr_systems;
      detail::Components<Emitter>     emitters;
      detail::Components<Object>      objects;
      detail::Components<Uplifting>   upliftings;
      detail::Component<Settings>     settings;   // Miscellaneous settings; e.g. texture size
      detail::Component<uint>         observer_i; // Primary observer index; simple enough for now
    
      // Check if any components were changed/added/removed
      constexpr bool is_mutated() const { return colr_systems || emitters || objects || upliftings || settings || observer_i; }
      constexpr operator bool() const { return is_mutated(); };
    } components;

    // Scene resources, primarily referred to by components in the scene (stored in zlib on disk)
    struct SceneResources {
      detail::Resources<Mesh>  meshes;
      detail::Resources<Image> images;
      detail::Resources<Spec>  illuminants;
      detail::Resources<CMFS>  observers;
      detail::Resources<Basis> bases;

      // Check if any resources were changed/added/removed
      constexpr bool is_mutated() const { return meshes || images || illuminants || observers || bases; }
      constexpr operator bool() const { return is_mutated(); };
    } resources;

    // Check if any components/resources were changed/added/removed
    constexpr bool is_mutated() const { return resources || components; }
    constexpr operator bool() const { return is_mutated(); };
    
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
    // Realize a pretty-printed name of a certain color system
    std::string csys_name(uint i)                   const;
    std::string csys_name(uint cmfs_i, uint illm_i) const;
    std::string csys_name(ColorSystem c)            const;

    // Realize the spectral data of a certain color system
    ColrSystem csys(uint i)                   const;
    ColrSystem csys(uint cmfs_i, uint illm_i) const;
    ColrSystem csys(ColorSystem c)            const;

    // Realize the spectral data of a certain emitter
    Spec emitter_spd(uint i)    const;
    Spec emitter_spd(Emitter e) const;
    
    // Extract a specific uplifting vertex, given indices;
    // supplied here given the common cumbersomeness of deep access
    const Uplifting::Vertex &uplifting_vertex(ConstraintSelection cs) const {
      return components.upliftings[cs.uplifting_i].value.verts[cs.vertex_i];
    }
    Uplifting::Vertex &uplifting_vertex(ConstraintSelection cs) {
      return components.upliftings[cs.uplifting_i].value.verts[cs.vertex_i];
    }

    // Helper object 
    struct SceneSurfaceInfo : public SurfaceInfo {
      // Direct references to related underlying objects
      const Object    &object;
      const Uplifting &uplifting;
    };

    // Given a RayRecord, recover underlying SurfaceInfo
    SceneSurfaceInfo get_surface_info(const eig::Array3f &p, const SurfaceRecord &rc) const;

  public: // Serialization
    void to_stream(std::ostream &str) const;
    void fr_stream(std::istream &str);
  };
} // namespace met