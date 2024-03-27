#pragma once

namespace met {
  // FWD Scene components
  struct Scene;
  struct ColorSystem;
  struct Emitter;
  struct Object;
  struct Settings;
  struct Uplifting;

  namespace detail {
    // FWD Overloads of component state
    struct UpliftingState;
    struct VertexState;
    struct ObjectState;
    struct SettingsState;
  } // namespace detail
} // namespace met