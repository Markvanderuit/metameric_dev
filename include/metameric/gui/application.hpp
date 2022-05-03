#pragma once

namespace met {
  struct ApplicationCreateInfo {
    // Pass parameters to application here
  };
  
  // Initialize and run the metameric application
  void create_application(ApplicationCreateInfo info);
} // namespace met