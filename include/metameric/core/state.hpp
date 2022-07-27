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

  namespace io {
    ProjectData load_project(const fs::path &path);
    void        save_project(const fs::path &path, const ProjectData &data);
  } // namespace io

  /* Save states in which a project can exist */
  enum class ProjectState {
    eUnloaded, // Project is not currently open
    eNew,      // Project has no previous save, is likely newly created
    eSaved,    // Project has previous save, and has not been modified
    eUnsaved,  // Project has previous save, but has been modified
  };

  /* Wrapper data to hold spectral mapping keys */
  struct MappingData {
    std::string cmfs;
    std::string illuminant;
    uint        n_scatters;
  };

  /* Wrapper object to hold saveable project data */
  struct ProjectData {
    template <typename T>
    using StrPair = std::pair<std::string, T>;

  public:
    // Default constr. provides sensible default values
    ProjectData();

    // Given a mapping key, gather loaded data into a SpectralMapping object
    SpectralMapping load_mapping(const std::string &key) const;
    CMFS            load_cmfs(const std::string &key) const;
    Spec            load_illuminant(const std::string &key) const;

    // Current mapping and gamut used for rgb->spectral conversion
    // SpectralMapping      rgb_mapping;
    std::array<Color, 4> rgb_gamut;
    std::array<Spec, 4>  spec_gamut;

    // List of named mappings for spectral->rgb conversion to be performed at runtime
    std::unordered_map<std::string, SpectralMapping> spectral_mappings;

    // List of named user-loaded or program-provided illuminants, cmfs, and spectral->rgb mappings
    std::vector<StrPair<MappingData>> loaded_mappings;
    std::vector<StrPair<CMFS>>        loaded_cmfs;
    std::vector<StrPair<Spec>>        loaded_illuminants;
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

    /* Miscellaneous components */

    Texture2d3f                  rgb_texture;
    KNNGrid<Spec>                spec_knn_grid;
    VoxelGrid<Spec>              spec_vox_grid;
    std::vector<SpectralMapping> loaded_mappings;

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

    // Apply a modification and set state to 'unsaved'
    void touch(ProjectMod &&mod);

    // Step through the modification at mod_i
    void redo();
    void undo();

    /* Spectral mapping handling */

    void load_mappings();
  };
} // namespace met