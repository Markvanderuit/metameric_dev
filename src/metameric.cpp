// STL includes
#include <cstdlib>
#include <exception>

// Misc includes
#include <fmt/core.h>

// Metameric includes
#include <metameric/gui/application.hpp>

int main() {
  try {
    met::create_application({ .texture_path     = "texture.png", 
                              .spectral_db_path = "database.mat",
                              .color_mode       = met::AppliationColorMode::eDark });
  } catch (const std::exception &e) {
    fmt::print(stderr, "{}\n", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}