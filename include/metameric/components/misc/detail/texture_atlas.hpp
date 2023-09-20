#include <metameric/core/scene.hpp>
#include <metameric/core/utility.hpp>
#include <small_gl/texture.hpp>
#include <algorithm>
#include <deque>
#include <execution>

namespace met::detail {
  enum class AtlasBuildMethod { eLayered, eSpread };

  // Return object for reserved space in a TextureAtlas
  struct TextureAtlasSpace {
    using vect = eig::Array2u;

    uint layer_i;
    vect offs, size;
  };

  // Helper object for initializing TextureAtlas
  struct TextureAtlasCreateInfo {
    using vect = eig::Array2u;

    std::vector<vect> sizes;
    uint              levels  = 1u;
    uint              padding = 0u;
    AtlasBuildMethod  method  = AtlasBuildMethod::eLayered;
  };

  /* TextureAtlas
     Simple wrapper around OpenGL-side array texture for handling of a number
     of similarly-sized textures.
   */
  template <typename T, uint D>
  struct TextureAtlas {
    using Texture     = gl::Texture2d<T, D, gl::TextureType::eImageArray>;
    using TextureView = gl::TextureView2d<T, D>;
    using vect        = eig::Array2u;

  private:
    AtlasBuildMethod               m_method;
    uint                           m_padding;
    Texture                        m_texture;
    std::vector<TextureView>       m_texture_views;
    std::vector<TextureAtlasSpace> m_resrv, m_empty;

  public:
    using InfoType = TextureAtlasCreateInfo;

    TextureAtlas() = default;
   ~TextureAtlas() = default;
    TextureAtlas(TextureAtlasCreateInfo info);

  public: // Reservation management
    void reserve(vect size, uint count, uint levels = 1u, uint padding = 0u);
    void reserve(std::span<vect> sizes, uint levels = 1u, uint padding = 0u);
    void shrink_to_fit();

  public:
          auto   size()    const { return m_texture.size();   }
          auto   levels()  const { return m_texture.levels(); }
    const auto & texture() const { return m_texture;          }
          auto & texture()       { return m_texture;          }

    const auto & view(uint layer = 0, uint level = 0) const { 
      return m_texture_views[layer * m_texture.levels() + level]; 
    }
    auto & view(uint layer = 0, uint level = 0) { 
      return m_texture_views[layer * m_texture.levels() + level]; 
    }

    const auto &reservation(uint i) const { return m_resrv[i]; }
    const auto &reservations()      const { return m_resrv;    }

    inline void swap(TextureAtlas &o) {
      met_trace();
      using std::swap;
      swap(m_padding,       o.m_padding);
      swap(m_method,        o.m_method);
      swap(m_texture,       o.m_texture);
      swap(m_texture_views, o.m_texture_views);
      swap(m_resrv,         o.m_resrv);
      swap(m_empty,         o.m_empty);
    }

    inline bool operator==(const TextureAtlas &o) const {
      return m_texture == o.m_texture; // unique, owned resource
    }

    met_declare_noncopyable(TextureAtlas);
  };
} // namespace met::detail