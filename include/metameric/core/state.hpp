#pragma once

#include <metameric/core/knn.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <filesystem>

namespace met {
  // FWD
  struct ProjectData;
  struct ApplicationData;

  namespace io {
    ProjectData load_project(const fs::path &path);
    void        save_project(const fs::path &path, const ProjectData &data);
  } // namespace io

  /* Load states in which a project can exist */
  enum class ProjectState {
    eUnloaded,  // Project is not currently loaded
    eUnsaved,   // Project has no previous save, is likely newly created
    eSaved,     // Project has previous save, and has not been modified
    eModified,  // Project has previous save, but has been modified
  };

  /* Wrapper object to hold loadable/saveable project data */
  struct ProjectData {
    // Default constr. provides sensible default values
    ProjectData();

    // Current mapping and gamut used for rgb->spectral conversion
    SpectralMapping      rgb_mapping;
    std::array<Color, 4> rgb_gamut;
    std::array<Spec, 4>  spec_gamut;

    // List of mappings for spectral->rgb conversion to be performed at runtime
    std::unordered_map<std::string, SpectralMapping> spectral_mappings;

    // List of user-loaded or program-provided illuminants
    std::unordered_map<std::string, CMFS> loaded_cmfs;
    std::unordered_map<std::string, Spec> loaded_illuminants;
  };

  /* Wrapper to hold all major application data */
  struct ApplicationData {
    /* Project components */

    fs::path project_path;
    ProjectData           project_data;
    ProjectState          project_state = ProjectState::eUnloaded; 

    /* Auxiliary components */

    Texture2d3f           rgb_texture;
    KNNGrid<Spec>         spec_knn_grid;
    VoxelGrid<Spec>       spec_vox_grid;

    /* Project state handling */

    void save(const fs::path &path);
    void load(const fs::path &path);
    void clear();
  };
} // namespace met