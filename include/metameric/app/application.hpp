#pragma once

#include <metameric/core/data.hpp>
#include <metameric/core/io.hpp>

namespace met {
  struct ApplicationCreateInfo {
    // In case of a existing project load
    fs::path project_path = "";

    // Application color theme
    ApplicationData::ColorMode color_mode = ApplicationData::ColorMode::eDark;
  };

  // Initialize and run the metameric application
  void create_application(ApplicationCreateInfo info);
} // namespace met