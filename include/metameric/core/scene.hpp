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
  /* Scene data layout.
     Simple indexed scene; no graph, just a library of objects and 
     their dependencies; responsible for most program data, as well
     as tracking of dependency modifications */
  struct Scene {
    // Scene components, directly visible or influential in the scene (stored as json on disk)
    struct SceneComponents {
      detail::Components<ColorSystem>  colr_systems;
      detail::Components<Emitter>      emitters;
      detail::Components<Object>       objects;
      detail::Components<Uplifting>    upliftings;
      detail::Components<ViewSettings> views;
      detail::Component<Settings>      settings;   // Miscellaneous settings; e.g. texture size
      detail::Component<uint>          observer_i; // Primary observer index; simple enough for now
    
      // Check if any components were changed/added/removed
      constexpr bool is_mutated() const { return colr_systems || emitters || objects || upliftings || settings || views || observer_i; }
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
    void overwrite_wavefront_obj(const fs::path &path);

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
    
  public:
    // Extract a specific uplifting vertex, given indices;
    // added here given the common cumbersomeness of deep access
    const Uplifting::Vertex &uplifting_vertex(ConstraintRecord cs) const {
      return components.upliftings[cs.uplifting_i].value.verts[cs.vertex_i];
    }
    Uplifting::Vertex &uplifting_vertex(ConstraintRecord cs) {
      return components.upliftings[cs.uplifting_i].value.verts[cs.vertex_i];
    }

  public: // Serialization
    void to_stream(std::ostream &str) const;
    void fr_stream(std::istream &str);
  };

  // Component/Resource test helpers; check if Ty instantiates Component<>/Resource<>
  template <typename Ty>
  concept is_component = requires(Ty ty) { { detail::Component { ty } } -> std::same_as<Ty>; };  
  template <typename Ty>
  concept is_resource = requires(Ty ty) { { detail::Resource { ty } } -> std::same_as<Ty>; };  
  template <typename Ty>
  concept is_scene_data = is_component<Ty> || is_resource<Ty>;

  // Forward to appropriate scene components or resources based on type
  template <typename Ty> requires (is_scene_data<Ty>) 
  constexpr
  auto & scene_data_by_type(Scene &scene) {
    using VTy = typename Ty::value_type;
    if constexpr (is_component<Ty>) {
      if      constexpr (std::is_same_v<VTy, ColorSystem>)  return scene.components.colr_systems;
      else if constexpr (std::is_same_v<VTy, Emitter>)      return scene.components.emitters;
      else if constexpr (std::is_same_v<VTy, Object>)       return scene.components.objects;
      else if constexpr (std::is_same_v<VTy, Uplifting>)    return scene.components.upliftings;
      else if constexpr (std::is_same_v<VTy, ViewSettings>) return scene.components.views;
      else debug::check_expr(false, "components_by_type<Ty> exhausted its implemented options"); 
    } else {
      if      constexpr (std::is_same_v<VTy, Mesh>)  return scene.resources.meshes;
      else if constexpr (std::is_same_v<VTy, Image>) return scene.resources.images;
      else if constexpr (std::is_same_v<VTy, CMFS>)  return scene.resources.observers;
      else if constexpr (std::is_same_v<VTy, Spec>)  return scene.resources.illuminants;
      else debug::check_expr(false, "resources_by_type<Ty> exhausted its implemented options"); 
    };
  }

  // Forward to appropriate scene components or resources based on type
  template <typename Ty> requires (is_scene_data<Ty>) 
  constexpr
  const auto & scene_data_by_type(const Scene &scene) {
    using VTy = typename Ty::value_type;
    if constexpr (is_component<Ty>) {
      if      constexpr (std::is_same_v<VTy, ColorSystem>)  return scene.components.colr_systems;
      else if constexpr (std::is_same_v<VTy, Emitter>)      return scene.components.emitters;
      else if constexpr (std::is_same_v<VTy, Object>)       return scene.components.objects;
      else if constexpr (std::is_same_v<VTy, Uplifting>)    return scene.components.upliftings;
      else if constexpr (std::is_same_v<VTy, ViewSettings>) return scene.components.views;
      else debug::check_expr(false, "components_by_type<Ty> exhausted its implemented options"); 
    } else {
      if      constexpr (std::is_same_v<VTy, Mesh>)  return scene.resources.meshes;
      else if constexpr (std::is_same_v<VTy, Image>) return scene.resources.images;
      else if constexpr (std::is_same_v<VTy, CMFS>)  return scene.resources.observers;
      else if constexpr (std::is_same_v<VTy, Spec>)  return scene.resources.illuminants;
      else debug::check_expr(false, "resources_by_type<Ty> exhausted its implemented options"); 
    };
  }
} // namespace met