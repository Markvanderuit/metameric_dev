#include <cstdlib>
#include <exception>
#include <fmt/core.h>
#include <metameric/app/application.hpp>

int main() {
  /* try { */
    met::create_application({ .database_path = "database.mat",
                              .color_mode    = met::AppColorMode::eDark });
  /* } catch (const std::exception &e) {
    fmt::print(stderr, "{}\n", e.what());
    return EXIT_FAILURE;
  } */
  return EXIT_SUCCESS;
}