#pragma once

#include <filesystem>

namespace met {
  enum class AppliationColorMode {
    eDark,
    eLight
  };

  struct ApplicationCreateInfo {
    std::filesystem::path texture_path;
    std::filesystem::path spectral_db_path;
    
    AppliationColorMode color_mode = AppliationColorMode::eDark;
  };
  
  // Initialize and run the metameric application
  void create_application(ApplicationCreateInfo info);
} // namespace met