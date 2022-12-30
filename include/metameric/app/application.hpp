#pragma once

#include <metameric/core/data.hpp>
#include <metameric/core/io.hpp>

namespace met {
  struct ApplicationCreateInfo {
    // In case of a new project load
    fs::path database_path = "";

    // In case of a existing project load
    fs::path project_path = "";

    // Application color theme
    ApplicationColorMode color_mode = ApplicationColorMode::eDark;
  };

  // Initialize and run the metameric application
  void create_application(ApplicationCreateInfo info);
} // namespace met