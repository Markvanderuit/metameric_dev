#pragma once

#include <metameric/core/detail/scene.hpp>

namespace met {  
  /* Project settings layout. */
  struct Settings {
    // Texture render size; input res, 2048x2048, 1024x1024, or 512x512
    enum class TextureSize { eFull, eHigh, eMed, eLow } texture_size;

    friend auto operator<=>(const Settings &, const Settings &) = default;
  };

  namespace detail {
    // Fine-grained state tracker helper
    struct SettingsState : public ComponentStateBase<Settings> {
      using Base = Settings;
      using ComponentStateBase<Base>::m_mutated;

      ComponentState<Base::TextureSize> texture_size;

    public:
      virtual 
      bool update(const Base &o) override {
        return m_mutated = texture_size.update(o.texture_size);
      }
    };
  } // namespace detail
} // namespace settings