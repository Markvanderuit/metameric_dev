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
#include <metameric/core/utility.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/texture.hpp>

namespace met::detail {
  // Object describing a single texture patch reserved inside an arbitrary
  // atlas, fit for std140/std430 buffer layout. Kept separate as it is
  // template-independent, and we use it between different-typed atlases.
  struct alignas(16) AtlasBlockLayout {
    alignas(4) uint         layer_i;
    alignas(8) eig::Array2u offs, size;
    alignas(8) eig::Array2f uv0, uv1;
  };
  static_assert(sizeof(AtlasBlockLayout) == 48);
  
  // Object describing an std140 buffer layout for atlas data
  struct AtlasBufferLayout {
    alignas(4) uint size;
    std::array<AtlasBlockLayout, detail::met_max_textures> data;
  };
  
  /* TextureAtlas
     Simple wrapper around OpenGL-side array texture for handling of a number
     of similarly-sized textures.
   */
  template <typename T, uint D>
  struct TextureAtlas {
    using TextureArray = gl::TextureArray2d<T, D>;
    using TextureView  = gl::TextureView2d<T, D>;

    // Build methods; either prefer adding extra layers, or grow the texture
    // horizontally/vertically if capacity is insufficient
    enum BuildMethod { eLayered, eSpread };

    // Helper object for construction of TextureAtlas
    struct CreateInfo {
      std::vector<eig::Array2u> sizes;
      uint                      levels  = 1u;
      uint                      padding = 0u;
      BuildMethod               method  = BuildMethod::eSpread;
    };

  private:
    // Current reserved spaces and remainder spaces
    std::vector<AtlasBlockLayout> 
                             m_patches, m_free;
    bool                     m_is_invalitated;
    
    // Texture/construction information
    BuildMethod              m_method  = BuildMethod::eSpread;
    uint                     m_levels  = 1u;
    uint                     m_padding = 0u;

    // GL-side objects
    TextureArray             m_texture;
    std::vector<TextureView> m_texture_views;
    gl::Buffer               m_buffer;
    AtlasBufferLayout       *m_buffer_map;

    // Helper private methods
    void init_views();
    void dstr_views();
    
  public: // Construction
    using InfoType = CreateInfo;

    TextureAtlas() = default;
   ~TextureAtlas() = default;
    TextureAtlas(InfoType info);

  public: // Texture space management
    // Given a range of sizes, ensure all sizes have a reserved space available.
    // Potentially grows the underlying texture, invalidating its contents
    void resize(eig::Array2u size, uint count);
    void resize(std::span<eig::Array2u> sizes);

    // Remove all reservations
    void clear();

    // Ensure the underlying texture's capacity is greater or equal than `size`
    void reserve(eig::Array3u size);

    // Reduce the underlying texture's capacity to tightly fit the current patch sizes
    void shrink_to_fit();

    // Return the current underlying texture's capacity, ergo its full size
    eig::Array3u capacity() const;

  public:
    // Test if the last call to texture.resize()/reserve() invalidated
    // the texture's contents, necessitating a rebuild of said contents
    bool is_invalitated() const { return m_is_invalitated; }
    void set_invalitated(bool b) { m_is_invalitated = b; }

    // Test if the underlying data even exists
    bool is_init() const { return texture().is_init(); }

    // General accessors
    const auto & texture()    const { return m_texture;    }
          auto & texture()          { return m_texture;    }
    const auto & buffer()     const { return m_buffer;     }
          auto & buffer()           { return m_buffer;     }
          auto   levels()     const { return m_levels;     }
          auto   padding()    const { return m_padding;    }

    // View textures to the texture's levels
    const auto & view(uint layer = 0, uint level = 0) const { 
      return m_texture_views[layer * m_texture.levels() + level]; 
    }
    auto & view(uint layer = 0, uint level = 0) { 
      return m_texture_views[layer * m_texture.levels() + level]; 
    }

    // Return information regarding the available spaces in the texture
    const auto &patch(uint i) const { return m_patches[i]; }
    const auto &patches()     const { return m_patches;    }

    inline void swap(TextureAtlas &o) {
      met_trace();
      using std::swap;
      swap(m_padding,       o.m_padding);
      swap(m_levels,        o.m_levels);
      swap(m_method,        o.m_method);
      swap(m_patches,       o.m_patches);
      swap(m_free,          o.m_free);
      swap(m_texture,       o.m_texture);
      swap(m_buffer,        o.m_buffer);
      swap(m_buffer_map,    o.m_buffer_map);
      swap(m_texture_views, o.m_texture_views);
    }

    inline bool operator==(const TextureAtlas &o) const {
      return m_texture == o.m_texture; // unique, owned resource
    }

    met_declare_noncopyable(TextureAtlas);
  };

  /* Shorthand notations for common texture atlas types follow */

  using TextureAtlas2d1f = TextureAtlas<float, 1>;
  using TextureAtlas2d2f = TextureAtlas<float, 2>;
  using TextureAtlas2d3f = TextureAtlas<float, 3>;
  using TextureAtlas2d4f = TextureAtlas<float, 4>;

  using TextureAtlas2d1i = TextureAtlas<int, 1>;
  using TextureAtlas2d2i = TextureAtlas<int, 2>;
  using TextureAtlas2d3i = TextureAtlas<int, 3>;
  using TextureAtlas2d4i = TextureAtlas<int, 4>;

  using TextureAtlas2d1s = TextureAtlas<short, 1>;
  using TextureAtlas2d2s = TextureAtlas<short, 2>;
  using TextureAtlas2d3s = TextureAtlas<short, 3>;
  using TextureAtlas2d4s = TextureAtlas<short, 4>;

  using TextureAtlas2d1ui = TextureAtlas<uint, 1>;
  using TextureAtlas2d2ui = TextureAtlas<uint, 2>;
  using TextureAtlas2d3ui = TextureAtlas<uint, 3>;
  using TextureAtlas2d4ui = TextureAtlas<uint, 4>;

  using TextureAtlas2d1us = TextureAtlas<ushort, 1>;
  using TextureAtlas2d2us = TextureAtlas<ushort, 2>;
  using TextureAtlas2d3us = TextureAtlas<ushort, 3>;
  using TextureAtlas2d4us = TextureAtlas<ushort, 4>;
} // namespace met::detail