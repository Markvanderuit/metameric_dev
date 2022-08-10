// STL includes
#include <cstdlib>
#include <exception>

// Misc includes
#include <fmt/core.h>

// Metameric includes
#include <metameric/app/application.hpp>

int main() {
  /* try { */
    met::create_application({ .texture_path  = "texture.jpg", 
                              .database_path = "database.mat",
                              .color_mode    = met::AppliationColorMode::eDark });
  /* } catch (const std::exception &e) {
    fmt::print(stderr, "{}\n", e.what());
    return EXIT_FAILURE;
  } */
  return EXIT_SUCCESS;
}