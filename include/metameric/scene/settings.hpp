// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/scene/detail/utility.hpp>

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

template<>
struct fmt::formatter<met::Settings::RendererType> {
  template <typename context_ty>
  constexpr auto parse(context_ty& ctx) { 
    return ctx.begin(); 
  }

  template <typename fmt_context_ty>
  constexpr auto format(const met::Settings::RendererType& ty, fmt_context_ty& ctx) const {
    std::string s;
    switch (ty) {
      case met::Settings::RendererType::ePath   : s = "path";      break;
      case met::Settings::RendererType::eDirect : s = "direct";    break;
      case met::Settings::RendererType::eDebug  : s = "debug";     break;
      default                                   : s = "undefined"; break;
    }
    return fmt::format_to(ctx.out(), "{}", s);
  }
};