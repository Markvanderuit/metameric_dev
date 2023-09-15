#include <metameric/core/scene.hpp>
#include <metameric/core/utility.hpp>
#include <small_gl/texture.hpp>
#include <algorithm>
#include <deque>
#include <execution>
#include <ranges>
#include <vector>

/* 
  Need to be able to

  - provide a list of images and their sizes, and have the
    place reserve them **and** create corresponding buffers.
  - allowing adding/removing specific images
*/

namespace met::detail {
  template <typename T, uint D>
  struct TextureAtlas {
    struct ImageInput {
      uint         image_i; // Index of input image in outside space
      eig::Array2u size;    // 2D reserved space of input image
    };

    struct ImageResrv {
      uint         layer_i;
      eig::Array2u offs;
      eig::Array2u size;
    };

    struct CreateInfo {
      std::vector<ImageInput> inputs;
    };

  private:
    using Texture = gl::Texture2d<T, D, gl::TextureType::eImageArray>;

    Texture                  m_texture;
    std::vector<ImageInput>  m_inputs;
    std::vector<ImageResrv>  m_resrv;
    std::vector<ImageResrv>  m_empty;

  public:
    using InfoType = CreateInfo;

    TextureAtlas() = default;
    TextureAtlas(CreateInfo info);

  public:
          auto   size()    const { return m_texture.size(); }
    const auto & texture() const { return m_texture;        }
          auto & texture()       { return m_texture;        }

    const ImageResrv &reservation(uint i) const { return m_resrv[i]; };

    inline void swap(TextureAtlas &o) {
      met_trace();
      using std::swap;
      swap(m_texture, o.m_texture);
      swap(m_inputs,  o.m_inputs);
      swap(m_resrv,   o.m_resrv);
      swap(m_empty,   o.m_empty);
    }

    inline bool operator==(const TextureAtlas &o) const {
      return m_texture == o.m_texture;
    }

    met_declare_noncopyable(TextureAtlas);
  };
} // namespace met::detail