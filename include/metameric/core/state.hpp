#pragma once

#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <filesystem>
#include <functional>
#include <vector>
#include <utility>

namespace met {
  /* Save states in which project data can exist */
  enum class SaveFlag {
    eUnloaded, // Project is not currently loaded
    eNew,      // Project has no previous save, is newly created
    eSaved,    // Project has previous save, and has not been modified
    eUnsaved,  // Project has previous save, and has been modified
  };

  /* Cache states in which project data values*/
  enum class CacheFlag : uint {
    eStale = 0, // Data is modified, pipeline should recompute dependent values
    eFresh = 1  // Data is up to date
  };
  met_declare_bitflag(CacheFlag);

  /* Wrapper object to hold saveable project data */
  struct ProjectData {
  public: /* internal data structures */
    // Set of keys of cmfs/illuminants together describing a stored color system
    struct Mapp {
      std::string cmfs, illuminant;
      uint        n_scatters; // stored directly
    };

    // Data structure for a single vertex of the project's convex hull mesh
    struct Vert {
      Colr colr_i; // The expected vertex color under a primary color system
      uint mapp_i; // Index of the selected primary color system
      std::vector<Colr> colr_j; // The expected vertex colors under secondary color systems
      std::vector<uint> mapp_j; // Indices of the selected primary color systemss
    };

    // Data structure for a triangle element of the project's convex hull mesh
    using Elem = eig::Array3u;
    
  public: /* public data */
    // Convex hull data structure used for rgb->spectral uplifting
    std::vector<Elem> gamut_elems;  // Triangle connections describing a convex hull
    std::vector<Vert> gamut_verts;  // Gamut vertex values under specified color system   

    // List of named user-loaded or program-provided mappings, illuminants, and cmfs
    std::vector<std::pair<std::string, Spec>> illuminants;
    std::vector<std::pair<std::string, CMFS>> cmfs;
    std::vector<std::pair<std::string, Mapp>> mappings;

  public: /* public methods */
    // Default constr. provides sensible default values
    ProjectData();
  };

  struct ProjectState {
    struct CacheVert {
      CacheFlag any;
      CacheFlag any_colr_j;
      CacheFlag any_mapp_j;

      CacheFlag colr_i;
      CacheFlag mapp_i;

      std::vector<CacheFlag> colr_j;
      std::vector<CacheFlag> mapp_j;
    };

  public:
    CacheFlag any;
    CacheFlag any_verts;
    CacheFlag any_elems;
    CacheFlag any_mapps;

    std::vector<CacheVert> verts;
    std::vector<CacheFlag> elems;
    std::vector<CacheFlag> mapps;
  };

  /* Wrapper object to hold a modification to project data */
  struct ProjectMod {
    // Short name of performed action for undo/redo view
    std::string name;

    // Applied modification (and its reverse), stored in a function capture
    std::function<void(ProjectData &)> redo, undo;
  };

  /* Wrapper to hold all major application data */
  struct ApplicationData {
    /* Project (saved) components */

    fs::path     project_path;
    SaveFlag     project_save = SaveFlag::eUnloaded; 
    ProjectData  project_data;
    ProjectState project_state;

    /* History modification components */

    std::vector<ProjectMod> mods;       // Stack of project data modifications
    int                     mod_i = -1; // Index of current last modification for undo/redo 

    /* Loaded (non-saved) components */

    Texture2d3f         loaded_texture;  // RGB texture image extracted from project data
    std::vector<Mapp>   loaded_mappings; // Spectral mappings extracted from project data

    /* Project data handling */
    
    // Create a new project and set state to ''new''
    void create(Texture2d3f &&texture);
    void create(const fs::path &texture_path);

    // Load a project from file and set state to 'saved'
    void load(const fs::path &load_path);

    // Save the current project to file and set state to 'saved'
    void save(const fs::path &save_path);

    // Unload the current project and set state to 'unloaded'
    void unload();

    // Apply a modification to project data and set state to 'unsaved'
    void touch(ProjectMod &&mod);

    // Step through the modification at mod_i
    void redo();
    void undo();

    // Reload all relevant mappings/spectra/color systems from underlying project data
    void load_mappings();

    // Reload gamut project data based on a convex hull approximation of the input texture
    void load_chull_gamut();

  private:
    // Given a string key, extract mapping/spectrum/color system data from underlying project data
    Mapp load_mapping(const std::string &key) const;
    CMFS load_cmfs(const std::string &key) const;
    Spec load_illuminant(const std::string &key) const;
  };

  namespace io {
    ProjectData load_project(const fs::path &path);
    void        save_project(const fs::path &path, const ProjectData &data);
  } // namespace io
} // namespace met