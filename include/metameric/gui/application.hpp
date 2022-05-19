#pragma once

#include <filesystem>

namespace met {
  struct ApplicationCreateInfo {
    // Pass parameters to application here
    std::filesystem::path texture_path;
  };
  
  // Initialize and run the metameric application
  void create_application(ApplicationCreateInfo info);
} // namespace met