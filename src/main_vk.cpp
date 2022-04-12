#include <cstdlib>
#include <exception>
#include <fmt/core.h>

#include <metameric/core/vlk/engine.h>

int main() {
  try {
    metameric::vlk::Engine engine;
    engine.init();
    engine.run();
    engine.dstr();
  } catch (const std::exception &e) {
    fmt::print(stderr, e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}