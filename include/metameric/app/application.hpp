#pragma once

#include <metameric/core/data.hpp>
#include <metameric/core/io.hpp>

namespace met {
  struct ApplicationCreateInfo {
    // In case of a existing project load
    fs::path project_path = "";

    // Application color theme
    AppColorMode color_mode = AppColorMode::eDark;
  };

  // Initialize and run the metameric application
  void create_application(ApplicationCreateInfo info);
} // namespace met