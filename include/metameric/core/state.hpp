#pragma once

#include <metameric/core/knn.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <filesystem>
#include <functional>
#include <utility>

namespace met {
  // FWD
  struct ProjectData;
  struct ProjectModification;
  struct ApplicationData;

  /* Save states in which project data can exist */
  enum class ProjectSaveState {
    eUnloaded, // Project is not currently loaded
    eNew,      // Project has no previous save, is newly created
    eSaved,    // Project has previous save, and has not been modified
    eUnsaved,  // Project has previous save, and has been modified
  };

  /* Wrapper object to hold saveable project data */
  struct ProjectData {
    /* Set of keys of cmfs/illuminants that together form a spectral mapping */
    struct Mapp {
      std::string cmfs, illuminant;
      uint        n_scatters; // stored directly
    };

    // Default constr. provides sensible default values
    ProjectData();

    // Current mappings and gamuts used for rgb->spectral conversion
    std::array<Colr, 4> gamut_colr_i; // Gamut vertex values under primary color system
    std::array<Colr, 4> gamut_colr_j; // Gamut vertex values under secondary color system
    std::array<uint, 4> gamut_mapp_i; // Gamut vertex index of primary color system 
    std::array<uint, 4> gamut_mapp_j; // Gamut vertex index of secondary color system 
    std::array<Spec, 4> gamut_spec;   // Resulting metameric spectra given above constraints

    // List of named user-loaded or program-provided mappings, illuminants, and cmfs
    std::vector<std::pair<std::string, Spec>> illuminants;
    std::vector<std::pair<std::string, CMFS>> cmfs;
    std::vector<std::pair<std::string, Mapp>> mappings;
  };

  /* Wrapper object to hold a modification to project data */
  struct ProjectMod {
    // Short description of performed action
    std::string name;

    // Performed action that is/has been applied, and its reverse
    std::function<void(ProjectData &)> redo, undo;
  };

  /* States in which project data caches can exist */
  enum class CacheState {
    eFresh, // Data cache is up to date
    eStale  // Data cache is stale
  };

  /* Wrapper to hold all major application data */
  struct ApplicationData {
    /* Project (saved) components */

    fs::path         project_path;
    ProjectData      project_data;
    ProjectSaveState project_state = ProjectSaveState::eUnloaded; 

    /* History modification components */

    std::vector<ProjectMod> mods;       // Stack of project data modifications
    int                     mod_i = -1; // Index of current last modification

    /* Loaded (non-saved) components */

    Texture2d3f       loaded_texture;  // RGB texture image loaded from project data
    std::vector<Mapp> loaded_mappings; // Spectral mappings loaded from project data
    KNNGrid<Spec>     loaded_knn_grid; // Placeholder spectral KNN dataset

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