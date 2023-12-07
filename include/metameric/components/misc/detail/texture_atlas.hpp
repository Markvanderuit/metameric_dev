#include <metameric/core/scene.hpp>
#include <metameric/core/utility.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/texture.hpp>

namespace met::detail {
  // TextureAtlasBase
  // Common base of TextureAtlas<T, D> objects, defining related types and enums
  struct TextureAtlasBase {
    using vec2 = eig::Array2u;
    using vec3 = eig::Array3u;

    // Build methods; either prefer adding extra layers, or grow the texture
    // horizontally/vertically if capacity is insufficient
    enum BuildMethod {
      eLayered, eSpread
    };

    // Object describing a single texture patch reserved inside the atlas,
    // fit for std140/std430 buffer layout
    struct PatchLayout {
      alignas(4) uint layer_i;
      alignas(8) vec2 offs, size;
    };

    // Helper object for initializing TextureAtlas
    struct CreateInfo {
      std::vector<vec2> sizes;
      uint              levels  = 1u;
      uint              padding = 0u;
      BuildMethod       method  = BuildMethod::eSpread;
    };
  };

  /* TextureAtlas
     Simple wrapper around OpenGL-side array texture for handling of a number
     of similarly-sized textures.
   */
  template <typename T, uint D>
  struct TextureAtlas : TextureAtlasBase {
    using InfoType    = TextureAtlasBase::CreateInfo;
    using Texture     = gl::Texture2d<T, D, gl::TextureType::eImageArray>;
    using TextureView = gl::TextureView2d<T, D>;

  private:
    BuildMethod              m_method  = BuildMethod::eLayered;
    uint                     m_levels  = 1u;
    uint                     m_padding = 0u;
    std::vector<TextureView> m_texture_views;
    std::vector<PatchLayout> m_patches, m_free;
    Texture                  m_texture;
    gl::Buffer               m_buffer;
    std::span<PatchLayout>   m_buffer_map;

    void init_views();
    void dstr_views();
    void reserve_buffer(size_t size);
    
  public: // Construction
    TextureAtlas() = default;
   ~TextureAtlas() = default;
    TextureAtlas(InfoType info);

  public: // Texture space management
    // Given a range of sizes, ensure all sizes have a reserved space available.
    // Potentially grows the underlying texture
    void resize(vec2 size, uint count);
    void resize(std::span<vec2> sizes);

    // Remove all reservations
    void clear();

    // Ensure the underlying texture's capacity is greater or equal than `size`
    void reserve(vec3 size);

    vec3 capacity() const { 
      return m_texture.is_init() ? m_texture.size() : 0;
    }

  public: // General accessors
          auto   levels()    const { return m_levels;         }
          auto   padding()   const { return m_padding;        }
    const auto & texture()   const { return m_texture;        }
          auto & texture()         { return m_texture;        }
    const auto & buffer()    const { return m_buffer;         }
          auto & buffer()          { return m_buffer;         }

    const auto & view(uint layer = 0, uint level = 0) const { 
      return m_texture_views[layer * m_texture.levels() + level]; 
    }
    auto & view(uint layer = 0, uint level = 0) { 
      return m_texture_views[layer * m_texture.levels() + level]; 
    }

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
      swap(m_texture_views, o.m_texture_views);
    }

    inline bool operator==(const TextureAtlas &o) const {
      return m_texture == o.m_texture; // unique, owned resource
    }

    met_declare_noncopyable(TextureAtlas);
  };
} // namespace met::detail