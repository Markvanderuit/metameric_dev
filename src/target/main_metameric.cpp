// STL includes
#include <cstdlib>
#include <exception>

// Misc includes
#include <fmt/core.h>

// Metameric includes
#include <metameric/app/application.hpp>

int main() {
  /* try { */
    met::create_application({ .database_path = "database.mat",
                              // .project_path  = "",
                              .project_path  = "C:/Users/mark/Documents/Drive/Metameric scenes/terrazzo.json",
                              .color_mode    = met::AppliationColorMode::eDark });
  /* } catch (const std::exception &e) {
    fmt::print(stderr, "{}\n", e.what());
    return EXIT_FAILURE;
  } */
  return EXIT_SUCCESS;
}