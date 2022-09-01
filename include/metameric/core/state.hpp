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
  enum class ProjectState {
    eUnloaded, // Project is not currently loaded
    eNew,      // Project has no previous save, is newly created
    eSaved,    // Project has previous save, and has not been modified
    eUnsaved,  // Project has previous save, and has been modified
  };

  /* Wrapper object to hold saveable project data */
  struct ProjectData {
    // Default constr. provides sensible default values
    ProjectData();

    // Current mapping and gamut used for rgb->spectral conversion
    std::array<Colr, 4> rgb_gamut;
    std::array<Spec, 4>  spec_gamut;

    /* Set of names of cmfs/illuminants that together form a spectral mapping */
    struct Mapping {
      std::string cmfs;
      std::string illuminant;
      uint        n_scatters; // stored directly
    };

    // List of named user-loaded or program-provided mappings, illuminants, and cmfs
    std::vector<std::pair<std::string, Mapping>> mappings;
    std::vector<std::pair<std::string, CMFS>>    cmfs;
    std::vector<std::pair<std::string, Spec>>    illuminants;

    // Given a mapping key, gather loaded data mapping into a SpectralMapping object
    SpectralMapping load_mapping(const std::string &key) const;
    CMFS            load_cmfs(const std::string &key) const;
    Spec            load_illuminant(const std::string &key) const;
  };

  /* Wrapper object to hold a modification to project data */
  struct ProjectMod {
    // Short description of performed action
    std::string name;

    // Performed action that is/has been applied
    std::function<void(ProjectData &)> redo;

    // Action to undo/remove the performed action
    std::function<void(ProjectData &)> undo;
  };

  /* Wrapper to hold all major application data */
  struct ApplicationData {
    /* Project components */

    fs::path     project_path;
    ProjectData  project_data;
    ProjectState project_state = ProjectState::eUnloaded; 

    /* History modification components */

    std::vector<ProjectMod> mods;
    int                     mod_i = -1;

    /* Loaded components */

    Texture2d3f                  loaded_texture;  // RGB texture object loaded from project data
    std::vector<SpectralMapping> loaded_mappings; // Spectral mappings loaded from project data
    KNNGrid<Spec>                spec_knn_grid;   // Placeholder spectral KNN dataset
    VoxelGrid<Spec>              spec_vox_grid;   // Placeholder spectral voxel grid

    /* Project state handling */
    
    // Create a new project and set state to ''new''
    void create(Texture2d3f &&texture);
    void create(const fs::path &texture_path);

    // Load a project from file and set state to 'saved'
    void load(const fs::path &load_path);

    // Save the current project to file and set state to 'saved'
    void save(const fs::path &save_path);

    // Unload the current project and set state to 'unloaded'
    void unload();

    /* Project modification handling */

    // Apply a modification to project data and set state to 'unsaved'
    void touch(ProjectMod &&mod);

    // Step through the modification at mod_i
    void redo();
    void undo();

    /* Spectral mapping handling */

    void load_mappings();
  };

  namespace io {
    ProjectData load_project(const fs::path &path);
    void        save_project(const fs::path &path, const ProjectData &data);
  } // namespace io
} // namespace met