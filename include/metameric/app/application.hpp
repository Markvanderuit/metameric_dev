#pragma once

#include <filesystem>

namespace met {
  enum class AppliationColorMode {
    eDark,
    eLight
  };

  struct ApplicationCreateInfo {
    // In case of a new project load
    std::filesystem::path texture_path  = "";
    std::filesystem::path database_path = "";

    // In case of a existing project load
    std::filesystem::path project_path = "";

    // Application color theme
    AppliationColorMode color_mode = AppliationColorMode::eDark;
  };

  // Initialize and run the metameric application
  void create_application(ApplicationCreateInfo info);
} // namespace met