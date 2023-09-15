#include <metameric/core/scene.hpp>
#include <metameric/core/utility.hpp>
#include <small_gl/texture.hpp>
#include <algorithm>
#include <deque>
#include <execution>
#include <ranges>
#include <vector>

namespace met::detail {
  template <typename T, uint C>
  class TextureAtlas {

    
  public:
    struct CreateInfo {
      
    };

    TextureAtlas() = default;
    TextureAtlas(CreateInfo info);
  };
} // namespace met::detail