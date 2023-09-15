#include <metameric/components/misc/detail/texture_atlas.hpp>

namespace met::detail {
  constexpr uint atlas_padding    = 16u;
  constexpr uint atlas_padding_2x = 2u * atlas_padding;

  constexpr auto atlas_area = [](auto space) -> uint { return space.size.prod(); };
  constexpr auto atlas_maxm = [](auto space) -> eig::Array2u { return (space.offs + space.size).eval(); };
  constexpr auto atlas_test = [](auto img, auto space) -> bool {
    return ((img.size + atlas_padding_2x) <= space.size).all();
  };
  constexpr auto atlas_split = [](auto img, auto space) {
    using Space = decltype(space);

    Space result = { .layer_i = space.layer_i,
                     .offs    = space.offs + atlas_padding,
                     .size    = img.size };

    std::vector<Space> remainder;
    if (uint remainder_x = space.size.x() - atlas_padding_2x - img.size.x(); remainder_x > 0)
      remainder.push_back({ .layer_i = space.layer_i,
                            .offs    = space.offs + eig::Array2u(atlas_padding_2x + img.size.x(), 0),
                            .size    = { space.size.x() - atlas_padding_2x - img.size.x(), 
                                         img.size.x() + atlas_padding_2x }  });
    if (uint remainder_y = space.size.y() - atlas_padding_2x - img.size.y(); remainder_y > 0)
      remainder.push_back({ .layer_i = space.layer_i,
                            .offs    = space.offs + eig::Array2u(0, atlas_padding_2x + img.size.y()),
                            .size    = { space.size.x(), 
                                         space.size.y() - img.size.y() - atlas_padding_2x } });

    return std::pair { result, remainder };
  };

  /* template <typename T, uint D>
  void TextureAtlas<T, D>::reserve(const eig::Array3u &size) {
    met_trace_full();
    
  }

  template <typename T, uint D>
  void TextureAtlas<T, D>::resize(const eig::Array3u &size) {
    met_trace_full();

    Texture texture_new = {{ .size = size }};

  } */
  
  template <typename T, uint D>
  TextureAtlas<T, D>::TextureAtlas(CreateInfo info)
  : m_inputs(info.inputs) {
    met_trace_full();
    
    // Current nr. of layers in use for the different image formats
    uint n_layers = 1;

    // Space vectors; we start with (uninitialized) reserved space for all images, and empty space for all 
    // formats and at a maximum size, which will be shrunk later
    m_resrv.clear();
    m_resrv.resize(info.inputs.size());
    m_empty = {{ .layer_i = 0, .offs = 0, .size = 16384u }};

    // Process a work queue over the generated work, sorted by decreasing image area
    rng::sort(info.inputs, rng::greater {}, atlas_area);
    std::deque<ImageInput> work_queue(range_iter(info.inputs));
    while (!work_queue.empty()) {
      auto &work = work_queue.front();

      // Generate a view over empty spaces where the image would, potentially, fit;
      // this incorporates padding
      auto available_space = vws::filter(m_empty, [work](auto s) { return atlas_test(work, s); });

      // If no space is available, we add a layer and restart
      if (available_space.empty()) {
        m_empty.push_back({ .layer_i = n_layers++, .offs  = 0, .size  = 16384 });
        continue;
      }

      // Find the smallest available space for current work
      auto smallest_space_it = rng::min_element(available_space, {}, atlas_area);
      
      // Part of smallest space is reserved for the current image work
      // Split the smallest space; part is reserved for the current image, while
      // part is made available as empty space. The original is removed
      auto [resrv, remainder] = atlas_split(work, *smallest_space_it);
      m_empty.erase(smallest_space_it.base());
      m_resrv[work.image_i] = resrv;
      m_empty.insert(m_empty.end(), range_iter(remainder));

      // Work for this image is removed from the queue, as space has been found
      work_queue.pop_front();
    } // while (work_queue)

    auto maxm = rng::fold_left(m_resrv | vws::transform(atlas_maxm), eig::Array2u(0), 
      [](auto a, auto b) { return a.cwiseMax(b).eval(); });

    // Allocate the texture
    m_texture = {{ .size = eig::Array3u { maxm.x(), maxm.y(), n_layers } }};
  }

  /* Explicit template instantiations */

  #define met_explicit_atlas(type, components)   \
    template class TextureAtlas<type, components>;
    
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