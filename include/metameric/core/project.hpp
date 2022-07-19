#pragma once

#include <metameric/core/spectrum.hpp>
#include <filesystem>

namespace met {
  struct ProjectLoadInfo {
    std::filesystem::path project_path; // Path to project data file

    AppliationColorMode color_mode = AppliationColorMode::eDark;
  };

  struct ProjectCreateInfo {
    std::filesystem::path texture_path;  // Path to RGB texture
    std::filesystem::path database_path; // Path to spectral datbase

    AppliationColorMode color_mode = AppliationColorMode::eDark;
  };

  struct Project {
    Project(ProjectLoadInfo info);
    Project(ProjectCreateInfo info);

    // Path to cached versions of loaded data
    std::filesystem::path texture_cache_path;  // User-loaded rgb texture
    std::filesystem::path database_cache_path; // Prog-loaded spectral db
  
    // Current mapping and gamut used for rgb->spectral conversion
    SpectralMapping      rgb_mapping;
    std::array<Color, 4> rgb_gamut;

    // List of mappings for spectral->rgb conversion to be performed at runtime
    std::unordered_map<std::string, SpectralMapping> spectral_mappings;

    // List of user-loaded or program-provided illuminants
    std::unordered_map<std::string, CMFS> loaded_cmfs;
    std::unordered_map<std::string, Spec> loaded_illuminants;
  };

  namespace io {
    Project load_project_from_file(const std::filesystem::path &path);
    void    write_project_to_file(const Project &p, const std::filesystem::path &path);
  }
} // namespace met