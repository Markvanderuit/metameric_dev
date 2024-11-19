#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/record.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/scene/resources.hpp>
#include <metameric/scene/emitter.hpp>
#include <metameric/scene/uplifting.hpp>
#include <metameric/scene/object.hpp>
#include <metameric/scene/settings.hpp>
#include <metameric/scene/view.hpp>
#include <metameric/scene/detail/components.hpp>
#include <metameric/scene/detail/resources.hpp>

namespace met {
  // Scene data layout.
  // Simple indexed scene; no graph, just vectors of objects and resource dependencies. 
  // Saves/loads most program data, handles update to GL before
  // render and view pipeline at frame start, and handles fine-grained state tracking
  // of user edits from view, so we can push updated data only.
  struct Scene {
    // Scene components, directly visible or influential in the scene (stored in json on disk)
    struct {
      detail::ComponentVector<Emitter>   emitters;     // Scene emitters
      detail::ComponentVector<Object>    objects;      // Scene objects
      detail::ComponentVector<Uplifting> upliftings;   // Uplifting structures used by objects to uplift albedo
      detail::ComponentVector<View>      views;        // Scene cameras for rendering output
      detail::Component<Settings>        settings;     // Miscellaneous settings; e.g. texture size
    } components;

    // Scene resources, primarily referred to by components in the scene (stored in binary zlib on disk)
    struct {
      detail::ResourceVector<Mesh>  meshes;      // Loaded mesh data
      detail::ResourceVector<Image> images;      // Loaded texture data
      detail::ResourceVector<Spec>  illuminants; // Loaded spectral power distributions
      detail::ResourceVector<CMFS>  observers;   // Loaded observer distributions
      detail::ResourceVector<Basis> bases;       // Loaded basis function data
    } resources;

  public: // Save state and IO handling
    // Scene is either not loaded, has no previous save, or is saved/modified
    enum class SaveState { 
      eUnloaded, eNew, eSaved, eUnsaved 
    } save_state = SaveState::eUnloaded; 

    // Current scene path, only set if SaveState is ::eSaved or ::eUnsaved
    fs::path save_path  = "";

    // Constructor creates empty scene, and sets handle to program shader cache
    Scene(ResourceHandle cache_handle);

    // Manage scene state
    void create();                    // Load, set to a default scene
    void load(const fs::path &path);  // Load scene data from path
    void save(const fs::path &path);  // Save scene data to path
    void unload();                    // Reset to an empty scene

    // Import an existing scene, adding its components into the loaded scene
    void import_scene(const fs::path &path);
    void import_scene(Scene &&other);

    // Run update of state tracking and gl-side data
    void update();

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
    // Realize the spectral data of a certain color system (or a pretty-printed name)
    ColrSystem csys(const Uplifting &uplifting) const;
    std::string csys_name(const Uplifting &uplifting) const;
    ColrSystem csys(uint cmfs_i, uint illm_i) const;
    std::string csys_name(uint cmfs_i, uint illm_i) const;

    // Realize the spectral data of a certain emitter
    Spec emitter_spd(uint i)    const;
    Spec emitter_spd(Emitter e) const;
    
    // Realize the observer data of a certain view
    CMFS primary_observer() const;
    CMFS view_observer(uint i) const;
    CMFS view_observer(View i) const;

    // Extract a specific uplifting vertex, given indices;
    // added here given the common cumbersomeness of deep access
    const Uplifting::Vertex &uplifting_vertex(ConstraintRecord cs) const;
    Uplifting::Vertex &uplifting_vertex(ConstraintRecord cs);

  private: 
    // Handle to program shader cache, accessible to gl-side data builders;
    mutable ResourceHandle m_cache_handle;
    friend class detail::SceneGLHandler<Uplifting>::ObjectData;
    friend class detail::SceneGLHandler<Object>::ObjectData;
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
    using value_type = typename Ty::value_type;
    if      constexpr (std::is_same_v<value_type, Emitter>)   return scene.components.emitters;
    else if constexpr (std::is_same_v<value_type, Object>)    return scene.components.objects;
    else if constexpr (std::is_same_v<value_type, Uplifting>) return scene.components.upliftings;
    else if constexpr (std::is_same_v<value_type, View>)      return scene.components.views;
    else if constexpr (std::is_same_v<value_type, Mesh>)      return scene.resources.meshes;
    else if constexpr (std::is_same_v<value_type, Image>)     return scene.resources.images;
    else if constexpr (std::is_same_v<value_type, CMFS>)      return scene.resources.observers;
    else if constexpr (std::is_same_v<value_type, Spec>)      return scene.resources.illuminants;
    else debug::check_expr(false, "scene_data_by_type<Ty> exhausted implemented options");
  }

  // Forward to appropriate scene components or resources based on type
  template <typename Ty> requires (is_scene_data<Ty>) 
  constexpr
  const auto & scene_data_by_type(const Scene &scene) {
    using value_type = typename Ty::value_type;
    if      constexpr (std::is_same_v<value_type, Emitter>)   return scene.components.emitters;
    else if constexpr (std::is_same_v<value_type, Object>)    return scene.components.objects;
    else if constexpr (std::is_same_v<value_type, Uplifting>) return scene.components.upliftings;
    else if constexpr (std::is_same_v<value_type, View>)      return scene.components.views;
    else if constexpr (std::is_same_v<value_type, Mesh>)      return scene.resources.meshes;
    else if constexpr (std::is_same_v<value_type, Image>)     return scene.resources.images;
    else if constexpr (std::is_same_v<value_type, CMFS>)      return scene.resources.observers;
    else if constexpr (std::is_same_v<value_type, Spec>)      return scene.resources.illuminants;
    else debug::check_expr(false, "scene_data_by_type<Ty> exhausted implemented options");
  }
} // namespace met