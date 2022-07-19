#pragma once

#include <metameric/core/project.hpp>
#include <filesystem>

namespace met {
  enum class AppliationColorMode {
    eDark,
    eLight
  };

  struct ApplicationCreateInfo {
    // In case of a new project load
    std::filesystem::path texture_path;
    std::filesystem::path database_path;
    
    AppliationColorMode color_mode = AppliationColorMode::eDark;
  };

  // Initialize and run the application
  void run_application_empty();                 // Run application; no project provided
  void run_application(ProjectCreateInfo info); // Run application; initialize a new project
  void run_application(ProjectLoadInfo info);   // Run application; load an existing project
  
  // Initialize and run the metameric application
  void create_application(ApplicationCreateInfo info);
} // namespace met