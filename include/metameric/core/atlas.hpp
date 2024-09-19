#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/utility.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/texture.hpp>

namespace met {
  // Object describing a single texture patch reserved inside an arbitrary
  // atlas, fit for std140/std430 buffer layout. Kept separate as it is
  // template-independent, and we use it between different-typed atlases.
  struct TextureAtlasPatchLayout {
    alignas(4) uint layer_i;
    alignas(8) eig::Array2u offs, size;
    alignas(8) eig::Array2f uv0, uv1;
  };
  
  /* TextureAtlas
     Simple wrapper around OpenGL-side array texture for handling of a number
     of similarly-sized textures.
   */
  template <typename T, uint D>
  struct TextureAtlas {
    using Texture     = gl::Texture2d<T, D, gl::TextureType::eImageArray>;
    using TextureView = gl::TextureView2d<T, D>;
    using PatchLayout = TextureAtlasPatchLayout;

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
    std::vector<PatchLayout> m_patches, m_free;
    bool                     m_is_invalitated;
    
    // Texture/construction information
    BuildMethod              m_method  = BuildMethod::eLayered;
    uint                     m_levels  = 1u;
    uint                     m_padding = 0u;

    // GL-side objects
    Texture                  m_texture;
    std::vector<TextureView> m_texture_views;
    gl::Buffer               m_buffer;
    std::span<PatchLayout>   m_buffer_map;

    // Helper private methods
    void init_views();
    void dstr_views();
    void reserve_buffer(size_t size);
    
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
      swap(m_texture_views, o.m_texture_views);
    }

    inline bool operator==(const TextureAtlas &o) const {
      return m_texture == o.m_texture; // unique, owned resource
    }

    met_declare_noncopyable(TextureAtlas);
  };
} // namespace met