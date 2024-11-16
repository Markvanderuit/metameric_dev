#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/detail/scene_components_utility.hpp>

namespace met {
  // Scene settings data layout
  struct Settings {
    // Selected viewport renderer; the rgb renderers are hacked in just for debugging
    enum class RendererType { 
      ePath,   // Spectral render, up to fixed path length
      eDirect, // Spectral render, direct light only
      eDebug,  // Spectral render, queries a value (eg albedo) and returns
    } renderer_type = RendererType::ePath;

    // Clamped texture sizes in atlas; input res, 2K, 1k, 512p
    enum class TextureSize { 
      eFull, eHigh, eMed, eLow 
    } texture_size = TextureSize::eHigh;

    // View component linked to scene viewport
    uint view_i = 0;

    // Render scaling used for scene viewport
    float view_scale   = .5f;

  public: // Boilerplate  
    auto operator<=>(const Settings &) const = default;

  public: // Helper methods to apply stored settings
    inline
    eig::Array2u apply_texture_size(const eig::Array2u &size) const {
      switch (texture_size) {
        case Settings::TextureSize::eHigh: return size.cwiseMin(2048u);
        case Settings::TextureSize::eMed:  return size.cwiseMin(1024u);
        case Settings::TextureSize::eLow:  return size.cwiseMin(512u);
        default:                           return size;
      }
    }
  };
  
  // Template specialization of SceneStateHandler that exposes fine-grained
  // state tracking for object members in the program view
  namespace detail {
    template <>
    struct SceneStateHandler<Settings> : public SceneStateHandlerBase<Settings> {
      SceneStateHandler<decltype(Settings::renderer_type)> renderer_type;
      SceneStateHandler<decltype(Settings::texture_size)>  texture_size;
      SceneStateHandler<decltype(Settings::view_i)>        view_i;
      SceneStateHandler<decltype(Settings::view_scale)>    view_scale;

    public:
      bool update(const Settings &o) override {
        met_trace();
        return m_mutated = 
        ( renderer_type.update(o.renderer_type)
        | texture_size.update(o.texture_size)
        | view_i.update(o.view_i)
        | view_scale.update(o.view_scale)
        );
      }
    };
  } // namespace detail
} // namespace met

namespace std {
  // Format Settings::RendererType, wich is an enum class
  template <>
  struct std::formatter<met::Settings::RendererType> : std::formatter<string_view> {
    auto format(const met::Settings::RendererType& ty, std::format_context& ctx) const {
      std::string s;
      switch (ty) {
        case met::Settings::RendererType::ePath   : s = "path";    break;
        case met::Settings::RendererType::eDirect : s = "direct";  break;
        case met::Settings::RendererType::eDebug  : s = "debug";   break;
      };
      return std::formatter<std::string_view>::format(s, ctx);
    }
  };
} // namespace std