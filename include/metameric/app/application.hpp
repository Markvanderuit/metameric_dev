#pragma once

#include <metameric/core/io.hpp>

namespace met {
  enum class AppliationColorMode {
    eDark,
    eLight
  };

  struct ApplicationCreateInfo {
    // In case of a new project load
    fs::path texture_path  = "";
    fs::path database_path = "";

    // In case of a existing project load
    fs::path project_path = "";

    // Application color theme
    AppliationColorMode color_mode = AppliationColorMode::eDark;
  };

  // Initialize and run the metameric application
  void create_application(ApplicationCreateInfo info);
} // namespace met