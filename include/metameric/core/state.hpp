#pragma once

#include <metameric/core/knn.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <filesystem>
#include <functional>
#include <utility>

namespace met {
  /* Save states in which project data can exist */
  enum class ProjectState {
    eUnloaded, // Project is not currently loaded
    eNew,      // Project has no previous save, is newly created
    eSaved,    // Project has previous save, and has not been modified
    eUnsaved,  // Project has previous save, and has been modified
  };

  /* Cache states in which project data values can exist throughout the program pipeline */
  enum class CacheState {
    eFresh, // Data is up to date
    eStale  // Data is stale, pipeline should recompute dependent values
  };

  struct GamutVertex {
    Colr position, offset;
    uint mapping_i, mapping_j;
  };

  struct GamutData {
    std::array<GamutVertex, 4> vertices;
    std::array<Spec, 4>        spectra;
  };

  /* Wrapper object to hold saveable project data */
  struct ProjectData {
    /* Set of keys of cmfs/illuminants that together describe a stored spectral mapping */
    struct Mapp {
      std::string cmfs, illuminant;
      uint        n_scatters; // stored directly
    };

    // Default constr. provides sensible default values
    ProjectData();

    // Current mappings and gamuts used for rgb->spectral conversion
    std::array<Colr, 4> gamut_colr_i; // Gamut vertex values under primary color system
    std::array<Colr, 4> gamut_offs_j; // Gamut value offsets under secondary color system
    std::array<uint, 4> gamut_mapp_i; // Gamut vertex index of selected primary color system 
    std::array<uint, 4> gamut_mapp_j; // Gamut vertex index of selected secondary color system 
    std::array<Spec, 4> gamut_spec;   // Resulting metameric spectra given above constraints

    // List of named user-loaded or program-provided mappings, illuminants, and cmfs
    std::vector<std::pair<std::string, Spec>> illuminants;
    std::vector<std::pair<std::string, CMFS>> cmfs;
    std::vector<std::pair<std::string, Mapp>> mappings;
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
    ProjectData  project_data;
    ProjectState project_state = ProjectState::eUnloaded; 

    /* History modification components */

    std::vector<ProjectMod> mods;       // Stack of project data modifications
    int                     mod_i = -1; // Index of current last modification for undo/redo 

    /* Loaded (non-saved) components */

    Texture2d3f         loaded_texture;  // RGB texture image extracted from project data
    std::vector<Mapp>   loaded_mappings; // Spectral mappings extracted from project data
    AlArray3fMesh       loaded_chull;    // Approximate convex hull surrounding RGB texture
    AlArray3fWireframe  loaded_chull_wf; // Wireframe mesh of approximate convex hull

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