#pragma once

#include <concepts>
#include <span>

namespace met {
  // FWD
  // Scene components
  struct Scene;
  struct ColorSystem;
  struct Emitter;
  struct Object;
  struct Settings;
  struct Uplifting;

  namespace detail {
    // FWD
    // Overloads of component state
    struct UpliftingState;
    struct ObjectState;
    struct SettingsState;
  } // namespace detail
} // namespace met