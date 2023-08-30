#include <metameric/core/image.hpp>
#include <algorithm>
#include <execution>
#include <cstdint>
#include <limits>

namespace met {
  template <typename Ty> requires (is_approx_comparable<Ty>)
  Image<Ty>::Image(ImageLoadInfo info) {
    met_trace();

  }

  template <typename Ty> requires (is_approx_comparable<Ty>)
  Image<Ty>::Image(ImageCreateInfo<Ty> info) {
    met_trace();

  }

  /* Explicit template instantiations */

#define declare_image(ImageDenom)                                                                   \
  template class Image<eig::Array1   ## ImageDenom>;                                                \
  template class Image<eig::Array3   ## ImageDenom>;                                                \
  template class Image<eig::Array4   ## ImageDenom>;                                                \
  template class Image<eig::AlArray3 ## ImageDenom>;

  declare_image(f );
  declare_image(u );
  declare_image(s );
  declare_image(us);
} // namespace met