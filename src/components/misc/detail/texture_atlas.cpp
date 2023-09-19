#include <metameric/components/misc/detail/texture_atlas.hpp>

namespace met::detail {
  constexpr auto atlas_split = [](auto padded_size, auto space) {
    using Space = decltype(space);

    Space result = { .layer_i = space.layer_i, .offs = space.offs, .size = padded_size };
    std::vector<Space> remainder;

    // Split off horizontal remainder
    if (uint remainder_x = space.size.x() - padded_size.x(); remainder_x > 0)
      remainder.push_back({ .layer_i = space.layer_i,
                            .offs    = space.offs + eig::Array2u(padded_size.x(), 0),
                            .size 
                               = { space.size.x() - padded_size.x(), padded_size.y() } });
    
    // Split off vertical remainder
    if (uint remainder_y = space.size.y() - padded_size.y(); remainder_y > 0)
      remainder.push_back({ .layer_i = space.layer_i,
                            .offs    = space.offs + eig::Array2u(0, padded_size.y()),
                            .size    = { space.size.x(), space.size.y() - padded_size.y() } });

    return std::pair { result, remainder };
  };

  template <typename T, uint D>
  void TextureAtlas<T, D>::reserve(vect size, uint count, uint levels, uint padding) {
    met_trace_full();
    std::vector<vect> v(count, size);
    reserve(v, levels, padding);
  }
  
  template <typename T, uint D>
  void TextureAtlas<T, D>::reserve(std::span<vect> sizes, uint levels, uint padding) {
    met_trace_full();

    // Catch
    if (sizes.empty()) {
      m_texture = {{ .size = { 1 + 2 * padding, 1 + 2 * padding, 1 }} };
      m_texture_views.clear();
      m_padding = padding;
      return;
    }

    // Clear out view objects on potential reallocates
    m_texture_views.clear();
    
    m_padding = padding;

    // Determine maximum reserved sizes, and current layer where we are reserving space
    uint width  = rng::max(sizes, {}, [](auto v) { return v.x(); }).x() + 2 * padding,
         height = rng::max(sizes, {}, [](auto v) { return v.y(); }).y() + 2 * padding,
         layers = 1;

    // Keep the old layout and texture around for now
    std::vector<TextureAtlasSpace> old_resrv(m_resrv);
    Texture old_texture;
    if (m_texture.is_init())
      std::swap(old_texture, m_texture);

    // We start with empty space at a maximum size, which can be shrunk layer
    m_resrv.clear();
    m_empty = {{ .layer_i = 0, .offs = 0, .size = { width, height } }};

    // We process a work queue over the required sizes, sorted by area
    std::vector<vect> sizes_sorted(range_iter(sizes));
    rng::sort(sizes_sorted, rng::greater {}, [](const auto &v) { return v.prod(); });
    std::deque<vect> sizes_queue(range_iter(sizes_sorted));

    // Iterate over the work queue over the required sizes
    while (!sizes_queue.empty()) {
      vect size = sizes_queue.front() + 2 * m_padding;

      // Generate a view over empty spaces where the current work fits
      auto candidate_vw = vws::filter(m_empty, [&](auto s) { return (size <= s.size).all(); });

      // If no suitable empty space was found, add a layer and restart
      if (candidate_vw.empty()) {
        m_empty.push_back({ .layer_i = layers++, .offs  = 0, .size = { width, height } });
        continue;
      }

      // Find the smallest suitable space for the current work
      auto candidate_it = rng::min_element(candidate_vw, {}, [](auto s) { return s.size.prod(); });

      // Split canndidate into reserved part, and put remainder(s) into empty space
      // for later insertions
      auto [reserved, remainder] = atlas_split(size, *candidate_it);
      m_empty.erase(candidate_it.base());
      m_empty.insert(m_empty.end(), range_iter(remainder));

      // Strip padding from the reserved space, and store it
      reserved.offs += padding;
      reserved.size -= padding * 2;
      m_resrv.push_back(reserved);

      sizes_queue.pop_front();
    } // while (!sizes_queue.empty())

    // Allocate the new texture
    m_texture = {{ .size = eig::Array3u { width, height, layers }, .levels = levels }};

    // Generate relevant view objects
    for (uint i = 0; i < layers; ++i)
      for (uint j = 0; j < levels; ++j)
        m_texture_views.push_back({{ .texture   = &m_texture, .min_level = j, .min_layer = i }});
  }

  template <typename T, uint D>
  void TextureAtlas<T, D>::shrink_to_fit() {
    guard(m_texture.is_init());
    met_trace_full();

    // Clear out view objects on potential reallocates
    m_texture_views.clear();

    // Determine the maximum size necessary for all reserved space
    auto view = m_resrv | vws::transform([&](const auto &s) { return s.offs + s.size + m_padding; });
    auto maxm = rng::fold_left(view, vect(0), [](auto a, auto b) { return a.cwiseMax(b).eval();  });
    auto maxl = 1 + rng::max(m_resrv, {}, &TextureAtlasSpace::layer_i).layer_i;

    // Allocate the resulting texture
    m_texture = {{ .size   = eig::Array3u { maxm.x(), maxm.y(), maxl }, .levels = m_texture.levels() }};

    // Generate relevant view objects
    for (uint i = 0; i < m_texture.size().z(); ++i)
      for (uint j = 0; j < m_texture.levels(); ++j)
        m_texture_views.push_back({{ .texture   = &m_texture, .min_level = j, .min_layer = i }});
  }

  template <typename T, uint D>
  TextureAtlas<T, D>::TextureAtlas(TextureAtlasCreateInfo info)
  : m_method(info.method) {
    met_trace();
    reserve(info.sizes, info.levels, info.padding);
  }

  /* Explicit template instantiations */

  #define met_explicit_atlas(type, components) template class TextureAtlas<type, components>;
    
  #define met_explicit_atlas_components(type)    \
    met_explicit_atlas(type, 1)                  \
    met_explicit_atlas(type, 2)                  \
    met_explicit_atlas(type, 3)                  \
    met_explicit_atlas(type, 4)

  #define met_explicit_atlas_types()             \
    met_explicit_atlas_components(ushort)        \
    met_explicit_atlas_components(short)         \
    met_explicit_atlas_components(uint)          \
    met_explicit_atlas_components(int)           \
    met_explicit_atlas_components(float)

  met_explicit_atlas_types()
} // namespace met::detail