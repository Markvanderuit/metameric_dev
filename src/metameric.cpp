// STL includes
#include <cstdlib>
#include <exception>

// Misc includes
#include <fmt/core.h>

// Metameric includes
#include <metameric/gui/application.hpp>

int main() {
  using namespace met;
  try {
    create_application({});
  } catch (const std::exception &e) {
    fmt::print(stderr, "{}", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}